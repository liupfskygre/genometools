/*
  Copyright (c) 2011 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2011 Center for Bioinformatics, University of Hamburg

  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef FIRSTCODES_TAB_H
#define FIRSTCODES_TAB_H

typedef struct
{
  unsigned long differentcodes,
                *allfirstcodes,
                *countocc;
} GtFirstcodestab;

unsigned long gt_firstcodes_get_leftborder(const GtFirstcodestab *fct,
                                           unsigned long idx);

unsigned long gt_firstcodes_numofallcodes(const GtFirstcodestab *fct);

unsigned long gt_firstcodes_findfirstlarger(const GtFirstcodestab *fct,
                                            unsigned long suftaboffset);

unsigned long gt_firstcodes_idx2code(const GtFirstcodestab *fct,
                                     unsigned long idx);

#endif
