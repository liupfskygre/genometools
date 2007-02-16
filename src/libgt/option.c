/*
  Copyright (c) 2006-2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2006-2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include "array.h"
#include "error.h"
#include "mailaddress.h"
#include "minmax.h"
#include "option.h"
#include "str.h"
#include "undef.h"
#include "xansi.h"

#define TERMINAL_WIDTH	80

typedef enum {
  OPTION_BOOL,
  OPTION_DOUBLE,
  OPTION_HELP,
  OPTION_HELPDEV,
  OPTION_OUTPUTFILE,
  OPTION_INT,
  OPTION_UINT,
  OPTION_LONG,
  OPTION_ULONG,
  OPTION_STRING,
  OPTION_VERSION
} Option_type;

struct OptionParser {
  char *progname,
       *synopsis,
       *one_liner;
  Array *options;
  bool parser_called;
  ShowCommentFunc comment_func;
  void *comment_func_data;
};

struct Option {
  Option_type option_type;
  Str *option_str,
      *description;
  void *value;
  union {
    bool b;
    double d;
    FILE *fp;
    int i;
    unsigned int ui;
    long l;
    unsigned long ul;
    const char *s;
  } default_value;
  union {
    int i;
    unsigned int ui;
    unsigned long ul;
  } min_value;
  bool is_set,
       is_mandatory,
       hide_default,
       min_value_set,
       is_development_option;
  Array *implications, /* contains option arrays, from each array at least one
                          option needs to be set */
        *exclusions;
  const Option *mandatory_either_option;
};

static Option *option_new(const char *option_str,
                          const char *description,
                          void *value)
{
  Option *o = xcalloc(1, sizeof(Option));

  assert(option_str && strlen(option_str) && option_str[0] != '-');

  o->option_str = str_new_cstr(option_str);
  o->description = str_new_cstr(description);
  o->value = value;

  return o;
}

static Option* option_new_help(void)
{
  Option *o = option_new("help", "display help and exit", NULL);
  o->option_type = OPTION_HELP;
  o->default_value.b = 0;
  return o;
}

static Option* option_new_helpdev(void)
{
  Option *o = option_new("helpdev",
                         "display help for development options and exit", NULL);
  o->option_type = OPTION_HELPDEV;
  o->default_value.b = false;
  o->is_development_option = true;
  return o;
}

static Option* option_new_version(ShowVersionFunc versionfunc)
{
  Option *o = option_new("version", "display version information and exit",
                         versionfunc);
  o->option_type = OPTION_VERSION;
  return o;
}

OptionParser* option_parser_new(const char *synopsis, const char *one_liner)
{
  OptionParser *op = xmalloc(sizeof(OptionParser));
  assert(synopsis && one_liner);
  /* enforce upper case letter at start and '.' at end of one line desc. */
  assert(strlen(one_liner) && isupper(one_liner[0]));
  assert(one_liner[strlen(one_liner)-1] == '.');
  op->progname = NULL;
  op->synopsis = xstrdup(synopsis);
  op->one_liner = xstrdup(one_liner);
  op->options = array_new(sizeof(Option*));
  op->parser_called = false;
  op->comment_func = NULL;
  op->comment_func_data = NULL;
  return op;
}

void option_parser_add_option(OptionParser *op, Option *o)
{
  assert(op && o);
  array_add(op->options, o);
}

void option_parser_set_comment_func(OptionParser *op,
                                    ShowCommentFunc comment_func, void *data)
{
  assert(op);
  op->comment_func = comment_func;
  op->comment_func_data = data;
}

/* XXX: undebugged garbage */
#if 0
static void show_long_description(unsigned long initial_space,
                                  const char *desc, unsigned long len)
{
  const unsigned long width = TERMINAL_WIDTH - initial_space + 1;
  const char *tmp_ptr, *desc_ptr = desc;
  unsigned long i;

  assert(initial_space < TERMINAL_WIDTH);

  while (desc_ptr < desc + len) {
    for (tmp_ptr = MIN(desc_ptr + width - 1, desc + len - 1);
         tmp_ptr >= desc_ptr;
         tmp_ptr--)
      if (*tmp_ptr == ' ' || *tmp_ptr == '\n') break;
    for (; desc_ptr < tmp_ptr; desc_ptr++)
      putchar(*desc_ptr);
    desc_ptr++;
    assert(*desc_ptr == ' ' || desc_ptr == desc + len - 1);
    putchar('\n');
    for  (i = 0; i < initial_space; i++)
      putchar(' ');
  }
}
#endif

static void show_help(OptionParser *op, bool show_development_options)
{
  unsigned long i, max_option_length = 0;
  Option *option;

  /* determine maximum option length */
  for (i = 0; i < array_size(op->options); i++) {
    option = *(Option**) array_get(op->options, i);
    if (str_length(option->option_str) > max_option_length)
      max_option_length = str_length(option->option_str);
  }

  printf("Usage: %s %s\n", op->progname, op->synopsis);
  printf("%s\n\n", op->one_liner);
  for (i = 0; i < array_size(op->options); i++) {
    option = *(Option**) array_get(op->options, i);
    /* skip option if necessary */
    if ((show_development_options && !option->is_development_option) ||
        (!show_development_options && option->is_development_option)) {
      continue;
    }
    printf("-%s%*s ", str_get(option->option_str),
           (int) (max_option_length - str_length(option->option_str)), "");
    if (str_length(option->description) >
        TERMINAL_WIDTH - max_option_length - 2) {
      /* option description is too long to fit in one line without wrapping */
      assert(0);
#if 0
      show_long_description(max_option_length + 2,
                            str_get(option->description),
                            str_length(option->description));
#endif
    }
    else
      xputs(str_get(option->description));

    /* show default value for some types of options */
    if (!option->hide_default) {
      if (option->option_type == OPTION_INT) {
        printf("%*s  default: %d\n", (int) max_option_length, "",
               option->default_value.i);
      }
      else if (option->option_type == OPTION_UINT) {
        printf("%*s  default: %ud\n", (int) max_option_length, "",
               option->default_value.ui);
      }
      else if (option->option_type == OPTION_LONG) {
        printf("%*s  default: ", (int) max_option_length, "");
        if (option->default_value.ul == UNDEFLONG)
          xputs("undefined");
        else
          printf("%ld\n", option->default_value.l);
      }
      else if (option->option_type == OPTION_ULONG) {
        printf("%*s  default: ", (int) max_option_length, "");
        if (option->default_value.ul == UNDEFULONG)
          xputs("undefined");
        else
          printf("%lu\n", option->default_value.ul);
      }
    }
  }
  if (op->comment_func)
    op->comment_func(op->progname, op->comment_func_data);
  printf("\nReport bugs to %s.\n", MAILADDRESS);
}

static void check_missing_argument(int argnum, int argc, Str *option)
{
  if (argnum + 1 >= argc)
    error("missing argument to option \"-%s\"", str_get(option));
}

static void check_mandatory_options(OptionParser *op)
{
  unsigned long i;
  Option *o;

  assert(op);

  for (i = 0; i < array_size(op->options); i++) {
    o = *(Option**) array_get(op->options, i);
    if (o->is_mandatory && !o->is_set)
      error("option \"-%s\" is mandatory", str_get(o->option_str));
  }
}

static void check_option_implications(OptionParser *op)
{
  unsigned long i, j, k, l;
  Array *implied_option_array;
  Option *o, *implied_option;
  unsigned int option_set;
  Str *error_str;

  for (i = 0; i < array_size(op->options); i++) {
    o = *(Option**) array_get(op->options, i);
    if (o->implications && o->is_set) {
      for (j = 0; j < array_size(o->implications); j++) {
        implied_option_array = *(Array**) array_get(o->implications, j);
        assert(array_size(implied_option_array));
	if (array_size(implied_option_array) == 1) {
	  /* special case: option implies exactly one option */
          implied_option = *(Option**) array_get(implied_option_array, 0);
          if (!implied_option->is_set) {
            error("option \"-%s\" requires option \"-%s\"",
	          str_get(o->option_str), str_get(implied_option->option_str));
          }
	}
	else {
          /* ``either'' case: option implied at least one of the options given
	     in array */
          option_set = 0;
	  for (k = 0; k < array_size(implied_option_array); k++) {
            implied_option = *(Option**) array_get(implied_option_array, k);
	    if (implied_option->is_set) {
              option_set = 1;
	      break;
	    }
          }
	  if (!option_set) {
            error_str = str_new_cstr("option \"-");
	    str_append_str(error_str, o->option_str);
	    str_append_cstr(error_str, "\" requires option");
	    for (l = 0; l < array_size(implied_option_array) - 1; l++) {
	      str_append_cstr(error_str, " \"-");
              str_append_str(error_str, (*(Option**)
	                     array_get(implied_option_array, l))->option_str);
              str_append_cstr(error_str, "\"");
	      if (array_size(implied_option_array) > 2)
	        str_append_char(error_str, ',');
            }
	    str_append_cstr(error_str, " or \"-");
            str_append_str(error_str, (*(Option**)
                           array_get(implied_option_array,
	                             array_size(implied_option_array) - 1))
		                     ->option_str);
            str_append_cstr(error_str, "\"");
	    error("%s", str_get(error_str));
	  }
	}
      }
    }
  }
}

static void check_option_exclusions(OptionParser *op)
{
  unsigned long i, j;
  Option *o, *excluded_option;

  for (i = 0; i < array_size(op->options); i++) {
    o = *(Option**) array_get(op->options, i);
    if (o->exclusions && o->is_set) {
      for (j = 0; j < array_size(o->exclusions); j++) {
        excluded_option = *(Option**) array_get(o->exclusions, j);
        if (excluded_option->is_set) {
          error("option \"-%s\" and option \"-%s\" exclude each other",
                str_get(o->option_str), str_get(excluded_option->option_str));
        }
      }
    }
  }
}

static void check_mandatory_either_options(OptionParser *op)
{
  unsigned long i;
  Option *o;

  for (i = 0; i < array_size(op->options); i++) {
    o = *(Option**) array_get(op->options, i);
    if (o->mandatory_either_option) {
      if (!o->is_set && !o->mandatory_either_option->is_set) {
        error("either option \"-%s\" or option \"-%s\" is mandatory",
               str_get(o->option_str),
               str_get(o->mandatory_either_option->option_str));
      }
    }
  }
}

static OPrval parse(OptionParser *op, int *parsed_args, int argc, char **argv,
                    ShowVersionFunc versionfunc,
                    unsigned int min_additional_arguments,
                    unsigned int max_additional_arguments, Error *err)
{
  int argnum, int_value;
  unsigned long i;
  double double_value;
  Option *option;
  bool option_parsed;
  long long_value;

  error_check(err);
  assert(op);
  assert(!op->parser_called); /* to avoid multiple adding of common options */

  op->progname = xstrdup(argv[0]);

  /* add common options */
  option = option_new_help();
  option_parser_add_option(op, option);
  option = option_new_helpdev();
  option_parser_add_option(op, option);
  option = option_new_version(versionfunc);
  option_parser_add_option(op, option);

  for (argnum = 1; argnum < argc; argnum++) {
    if (!(argv[argnum] && argv[argnum][0] == '-' && strlen(argv[argnum]) > 1))
      break;

    /* look for matching option */
    option_parsed = false;
    for (i = 0; i < array_size(op->options); i++) {
      option = *(Option**) array_get(op->options, i);

      if (strcmp(argv[argnum]+1, str_get(option->option_str)) == 0) {
        /* make sure option has not been used before */
        if (option->is_set)
          error("option \"%s\" already set", str_get(option->option_str));
        option->is_set = true;
        switch (option->option_type) {
          case OPTION_BOOL:
            /* XXX: the next argument (if any) is an option
               (boolean parsing not implemented yet) */
            /* assert(!argv[argnum+1] || argv[argnum+1][0] == '-'); */
            *(bool*) option->value = true;
            option_parsed = true;
            break;
          case OPTION_DOUBLE:
            check_missing_argument(argnum, argc, option->option_str);
            argnum++;
            if (sscanf(argv[argnum], "%lf", &double_value) != 1) {
              error("argument to option \"-%s\" must be floating-point number",
                    str_get(option->option_str));
            }
            *(double*) option->value = double_value;
            option_parsed = true;
            break;
          case OPTION_HELP:
            show_help(op, false);
            return OPTIONPARSER_REQUESTS_EXIT;
          case OPTION_HELPDEV:
            show_help(op, true);
            return OPTIONPARSER_REQUESTS_EXIT;
          case OPTION_OUTPUTFILE:
            check_missing_argument(argnum, argc, option->option_str);
            argnum++;
            *(FILE**) option->value = xfopen(argv[argnum] , "w");
            option_parsed = true;
            break;
          case OPTION_INT:
            check_missing_argument(argnum, argc, option->option_str);
            argnum++;
            if (sscanf(argv[argnum], "%d", &int_value) != 1) {
              error("argument to option \"-%s\" must be an integer",
                    str_get(option->option_str));
            }
            /* minimum value check */
            if (option->min_value_set && int_value < option->min_value.i) {
              error("argument to option \"-%s\" must be an integer >= %d",
                    str_get(option->option_str), option->min_value.i);
            }
            *(int*) option->value = int_value;
            option_parsed = true;
            break;
          case OPTION_UINT:
            check_missing_argument(argnum, argc, option->option_str);
            argnum++;
            if (sscanf(argv[argnum], "%d", &int_value) != 1 || int_value < 0) {
              error("argument to option \"-%s\" must be a non-negative integer",
                     str_get(option->option_str));
            }
            /* minimum value check */
            if (option->min_value_set && int_value < option->min_value.ui) {
              error("argument to option \"-%s\" must be an integer >= %u",
                    str_get(option->option_str), option->min_value.ui);
            }
            *(unsigned int*) option->value = int_value;
            option_parsed = true;
            break;
          case OPTION_LONG:
            check_missing_argument(argnum, argc, option->option_str);
            argnum++;
            if (sscanf(argv[argnum], "%ld", &long_value) != 1) {
              error("argument to option \"-%s\" must be an integer",
                    str_get(option->option_str));
            }
            *(long*) option->value = long_value;
            option_parsed = true;
            break;
          case OPTION_ULONG:
            check_missing_argument(argnum, argc, option->option_str);
            argnum++;
            if (sscanf(argv[argnum], "%ld", &long_value) != 1 ||
                long_value < 0) {
              error("argument to option \"-%s\" must be a non-negative integer",
                    str_get(option->option_str));
            }
            /* minimum value check */
            if (option->min_value_set && long_value < option->min_value.ul) {
              error("argument to option \"-%s\" must be an integer >= %lu",
                    str_get(option->option_str), option->min_value.ul);
            }
            *(unsigned long*) option->value = long_value;
            option_parsed = true;
            break;
          case OPTION_STRING:
            check_missing_argument(argnum, argc, option->option_str);
            argnum++;
            str_set(option->value, argv[argnum]);
            option_parsed = true;
            break;
          case OPTION_VERSION:
            ((ShowVersionFunc) option->value)(op->progname);
            return OPTIONPARSER_REQUESTS_EXIT;
          default: assert(0);
        }
      }
      if (option_parsed)
        break;
    }

    if (option_parsed)
      continue;

    /* no matching option found -> error */
    error("unknown option: %s (-help shows possible options)", argv[argnum]);
  }

  /* check for minimum number of additional arguments, if necessary */
  if (min_additional_arguments != UNDEFUINT &&
      argc - argnum < min_additional_arguments) {
    error("missing argument\nUsage: %s %s", op->progname, op->synopsis);
  }

  /* check for maximal number of additional arguments, if necessary */
  if (max_additional_arguments != UNDEFUINT &&
      argc - argnum > max_additional_arguments) {
    error("superfluous argument \"%s\"\nUsage: %s %s",
          argv[argnum + max_additional_arguments],
          op->progname, op->synopsis);
  }

  check_mandatory_options(op);
  check_option_implications(op);
  check_option_exclusions(op);
  check_mandatory_either_options(op);

  op->parser_called = true;
  *parsed_args = argnum;

  return OPTIONPARSER_OK;
}

OPrval option_parser_parse(OptionParser *op, int *parsed_args, int argc,
                           char **argv, ShowVersionFunc versionfunc,
                           Error *err)
{
  error_check(err);
  return parse(op, parsed_args, argc, argv, versionfunc, UNDEFUINT, UNDEFUINT,
               err);
}

OPrval option_parser_parse_min_args(OptionParser *op, int *parsed_args,
                                    int argc, char **argv,
                                    ShowVersionFunc versionfunc,
                                    unsigned int min_additional_arguments,
                                    Error *err)
{
  error_check(err);
  return parse(op, parsed_args, argc, argv, versionfunc,
               min_additional_arguments, UNDEFUINT, err);
}

OPrval option_parser_parse_max_args(OptionParser *op, int *parsed_args,
                                    int argc, char **argv,
                                    ShowVersionFunc versionfunc,
                                    unsigned int max_additional_arguments,
                                    Error *err)
{
  error_check(err);
  return parse(op, parsed_args, argc, argv, versionfunc, UNDEFUINT,
               max_additional_arguments, err);
}

OPrval option_parser_parse_min_max_args(OptionParser *op, int *parsed_args,
                                        int argc, char **argv,
                                        ShowVersionFunc versionfunc,
                                        unsigned int min_additional_arguments,
                                        unsigned int max_additional_arguments,
                                        Error *err)
{
  error_check(err);
  return parse(op, parsed_args, argc, argv, versionfunc,
               min_additional_arguments, max_additional_arguments, err);
}

void option_parser_free(OptionParser *op)
{
  unsigned long i;
  if (!op) return;
  free(op->progname);
  free(op->synopsis);
  free(op->one_liner);
  for (i = 0; i < array_size(op->options); i++)
    option_free(*(Option**) array_get(op->options, i));
  array_free(op->options);
  free(op);
}

Option* option_new_outputfile(FILE **outfp)
{
  Option *o = option_new("o", "redirect output to specified file (will "
                         "overwrite existing file!)", outfp);
  o->option_type = OPTION_OUTPUTFILE;
  o->default_value.fp = stdout;
  *outfp = stdout;
  return o;
}

Option* option_new_verbose(bool *value)
{
  return option_new_bool("v", "be verbose", value, false);
}

Option* option_new_debug(bool *value)
{
  Option *o = option_new_bool("debug", "enable debugging output", value, false);
  o->is_development_option = true;
  return o;
}

Option* option_new_bool(const char *option_str,
                        const char *description,
                        bool *value,
                        bool default_value)
{
  Option *o = option_new(option_str, description, value);
  o->option_type = OPTION_BOOL;
  o->default_value.b = default_value;
  *value = default_value;
  return o;
}

Option* option_new_double(const char *option_str,
                          const char *description,
                          double *value,
                          double default_value)
{
  Option *o = option_new(option_str, description, value);
  o->option_type = OPTION_DOUBLE;
  o->default_value.d = default_value;
  *value = default_value;
  return o;
}

Option* option_new_int(const char *option_str,
                       const char *description,
                       int *value,
                       int default_value)
{
  Option *o = option_new(option_str, description, value);
  o->option_type = OPTION_INT;
  o->default_value.i = default_value;
  *value = default_value;
  return o;
}

Option* option_new_int_min(const char *option_str,
                           const char *description,
                           int *value,
                           int default_value,
                           int min_value)
{
  Option *o = option_new_int(option_str, description, value, default_value);
  o->min_value_set = true;
  o->min_value.i = min_value;
  return o;
}

Option* option_new_uint(const char *option_str,
                        const char *description,
                        unsigned int *value,
                        unsigned int default_value)
{
  Option *o = option_new(option_str, description, value);
  o->option_type = OPTION_UINT;
  o->default_value.ui = default_value;
  *value = default_value;
  return o;
}

Option* option_new_uint_min(const char *option_str,
                            const char *description,
                            unsigned int *value,
                            unsigned int default_value,
                            unsigned int min_value)
{
  Option *o = option_new_int(option_str, description, (int*) value,
                             default_value);
  o->min_value_set = true;
  o->min_value.ui = min_value;
  return o;
}

Option* option_new_long(const char *option_str,
                        const char *description,
                        long *value,
                        long default_value)
{
  Option *o = option_new(option_str, description, value);
  o->option_type = OPTION_LONG;
  o->default_value.l = default_value;
  *value = default_value;
  return o;
}

Option* option_new_ulong(const char *option_str,
                         const char *description,
                         unsigned long *value,
                         unsigned long default_value)
{
  Option *o = option_new(option_str, description, value);
  o->option_type = OPTION_ULONG;
  o->default_value.ul = default_value;
  *value = default_value;
  return o;
}

Option* option_new_ulong_min(const char *option_str, const char *description,
                             unsigned long *value,
                             unsigned long default_value,
                             unsigned long min_value)
{
  Option *o = option_new_ulong(option_str, description, value, default_value);
  o->min_value_set = true;
  o->min_value.ul = min_value;
  return o;
}

Option* option_new_string(const char *option_str, const char *description,
                          Str *value, const char *default_value)
{
  Option *o = option_new(option_str, description, value);
  o->option_type = OPTION_STRING;
  o->default_value.s = default_value;
  str_set(value, default_value);
  return o;
}

void option_is_mandatory(Option *o)
{
  assert(o);
  o->is_mandatory = true;
}

void option_is_mandatory_either(Option *o, const Option *meo)
{
  assert(o && meo);
  assert(!o->mandatory_either_option);
  o->mandatory_either_option = meo;
}

void option_is_development_option(Option *o)
{
  assert(o);
  o->is_development_option = true;
}

void option_imply(Option *o, const Option *implied_option)
{
  Array *option_array;
  assert(o && implied_option);
  if (!o->implications)
    o->implications = array_new(sizeof(Array*));
  option_array = array_new(sizeof(Option*));
  array_add(option_array, implied_option);
  array_add(o->implications, option_array);
}

void option_imply_either_2(Option *o, const Option *io1, const Option *io2)
{
  Array *option_array;
  assert(o && io1 && io2);
  if (!o->implications)
    o->implications = array_new(sizeof(Array*));
  option_array = array_new(sizeof(Option*));
  array_add(option_array, io1);
  array_add(option_array, io2);
  array_add(o->implications, option_array);
}

void option_exclude(Option *o_a, Option *o_b)
{
  assert(o_a && o_b);
  if (!o_a->exclusions)
    o_a->exclusions = array_new(sizeof(Option*));
  if (!o_b->exclusions)
    o_b->exclusions = array_new(sizeof(Option*));
  array_add(o_a->exclusions, o_b);
  array_add(o_b->exclusions, o_a);
}

void option_hide_default(Option *o)
{
  assert(o);
  o->hide_default = true;
}

void option_free(Option *o)
{
  unsigned long i;
  if (!o) return;
  str_free(o->option_str);
  str_free(o->description);
  for (i = 0; i < array_size(o->implications); i++)
    array_free(*(Array**) array_get(o->implications, i));
  array_free(o->implications);
  array_free(o->exclusions);
  free(o);
}
