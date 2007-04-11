/*
  Copyright (c) 2003-2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2003-2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#include <libgtext/linearalign.h>

static void firstEDtabRtabcolumn(unsigned long *EDtabcolumn,
                                 unsigned long *Rtabcolumn, unsigned long ulen)
{
  unsigned long row;
  assert(EDtabcolumn && Rtabcolumn);
  for (row = 0; row <= ulen; row++) {
    EDtabcolumn[row] = row;
    Rtabcolumn[row] = row;
  }
}

static void nextEDtabRtabcolumn(unsigned long *EDtabcolumn,
                                unsigned long *Rtabcolumn,
                                unsigned long colindex, unsigned long midcolumn,
                                char b, const char *u, unsigned long ulen)
{
  unsigned long row, val, north_west_EDtab_entry, west_EDtab_entry,
                north_west_Rtab_entry, west_Rtab_entry = 0;
  bool update_Rtabcolumn = false;

  assert(EDtabcolumn && Rtabcolumn && u);

  /* saves the first entry of EDtabcolumn */
  west_EDtab_entry = EDtabcolumn[0];

  if (colindex > midcolumn) {
    /* only in this case Rtabcolumn needs to be updated */
    update_Rtabcolumn = true;
    Rtabcolumn[0] = 0;
  }

  EDtabcolumn[0]++;
  for (row = 1; row <= ulen; row++) {
    north_west_EDtab_entry = west_EDtab_entry;
    north_west_Rtab_entry  = west_Rtab_entry;
    west_EDtab_entry = EDtabcolumn[row];
    west_Rtab_entry  = Rtabcolumn[row];
    EDtabcolumn[row]++; /* 1. recurrence (Rtabcolumn[i] is unchanged) */
    /* 2. recurrence: */
    if ((val = north_west_EDtab_entry + (u[row-1] == b ? 0 : 1)) <
        EDtabcolumn[row]) {
      EDtabcolumn[row] = val;
      if (update_Rtabcolumn)
        Rtabcolumn[row] = north_west_Rtab_entry;
    }
    /* 3. recurrence: */
    if ((val = EDtabcolumn[row-1] + 1) < EDtabcolumn[row]) {
      EDtabcolumn[row] = val;
      if (update_Rtabcolumn)
        Rtabcolumn[row] = Rtabcolumn[row-1];
    }
  }
}

static unsigned long evaluateallcolumns(unsigned long *EDtabcolumn,
                                        unsigned long *Rtabcolumn,
                                        unsigned long midcol,
                                        const char *u, unsigned long ulen,
                                        const char *v, unsigned long vlen)
{
  unsigned long col;
  assert(EDtabcolumn && Rtabcolumn && u && v);
  firstEDtabRtabcolumn(EDtabcolumn, Rtabcolumn, ulen);
  for (col = 1; col <= vlen; col++) {
    nextEDtabRtabcolumn(EDtabcolumn, Rtabcolumn, col, midcol, v[col-1], u,
                        ulen);
  }
  return EDtabcolumn[ulen]; /* return distance */
}

static unsigned long evaluatecrosspoints(const char *u, unsigned long ulen,
                                         const char *v, unsigned long vlen,
                                         unsigned long *EDtabcolumn,
                                         unsigned long *Rtabcolumn,
                                         unsigned long *Ctab,
                                         unsigned long rowoffset)
{
  unsigned long midrow, midcol, dist;
  assert(u && v && EDtabcolumn && Rtabcolumn && Ctab);
  if (vlen >= 2) {
    midcol = vlen / 2;
    dist= evaluateallcolumns(EDtabcolumn, Rtabcolumn, midcol, u, ulen, v, vlen);
    midrow = Rtabcolumn[ulen];
    Ctab[midcol] = rowoffset + midrow;
    evaluatecrosspoints(u, midrow, v, midcol, EDtabcolumn, Rtabcolumn, Ctab,
                        rowoffset);
    evaluatecrosspoints(u + midrow, ulen - midrow, v + midcol, vlen - midcol,
                        EDtabcolumn, Rtabcolumn, Ctab + midcol,
                        rowoffset + midrow);
    return dist;
  }
  return 0;
}

static unsigned long determineCtab0(unsigned long *Ctab, char v0, const char *u)
{
  unsigned long row;
  assert(Ctab && u);
  for (row = 0; row < Ctab[1]; row++) {
    if (v0 == u[row]) {
      Ctab[0] = row;
      return Ctab[1] - 1;
    }
  }
  if (Ctab[1] > 0)
    Ctab[0] = Ctab[1] - 1;
  else
    Ctab[0] = 0;
  return Ctab[1];
}

static unsigned long computeCtab(const char *u, unsigned long ulen,
                                 const char *v, unsigned long vlen,
                                 unsigned long *Ctab, Env *env)
{
  unsigned long *EDtabcolumn, /* a column of the edit distance table */
                *Rtabcolumn,  /* a column of the row index table */
                dist;
  env_error_check(env);

  EDtabcolumn = env_ma_malloc(env, sizeof (unsigned long) * (ulen + 1));
  Rtabcolumn  = env_ma_malloc(env, sizeof (unsigned long) * (ulen + 1));

  if (vlen == 1) {
    Ctab[1] = ulen;
    dist = determineCtab0(Ctab, v[0], u);
  }
  else {
    dist = evaluatecrosspoints(u, ulen, v, vlen, EDtabcolumn, Rtabcolumn, Ctab,
                               0);
    Ctab[vlen] = ulen;
    determineCtab0(Ctab, v[0], u);
  }

  env_ma_free(Rtabcolumn, env);
  env_ma_free(EDtabcolumn, env);

  return dist;
}

static Alignment* reconstructalignment(unsigned long *Ctab,
                                       const char *u, unsigned long ulen,
                                       const char *v, unsigned long vlen,
                                       Env *env)
{
  unsigned long row = ulen, col = vlen;
  Alignment *alignment;
  env_error_check(env);
  assert(Ctab && u && ulen && v && vlen);
  alignment = alignment_new_with_seqs(u, ulen, v, vlen, env);
  while (row || col) {
  }
  return alignment;
}

Alignment* linearalign(const char *u, unsigned long ulen,
                       const char *v, unsigned long vlen, Env *env)
{
  unsigned long *Ctab, dist;
  Alignment *alignment;
  env_error_check(env);
  assert(u && ulen && v && vlen);
  Ctab = env_ma_malloc(env, sizeof (unsigned long) * (vlen + 1));
  dist = computeCtab(u, ulen, v, vlen, Ctab, env);
  alignment = reconstructalignment(Ctab, u, ulen, v, vlen, env);
  assert(dist == alignment_eval(alignment));
  env_ma_free(Ctab, env);
  return alignment;
}
