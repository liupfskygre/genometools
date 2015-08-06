/*
  Copyright (c) 2015 Annika <annika.seidel@studium.uni-hamburg.de>
  Copyright (c) 2015 Center for Bioinformatics, University of Hamburg

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

#include <string.h>
#include "core/assert_api.h"
#include "core/minmax.h"
#include "core/error.h"
#include "core/types_api.h"
#include "core/divmodmul.h"
#include "core/ma_api.h"
#include "extended/affinealign.h"
#include "extended/affinealign_linear.h"
#include "extended/reconstructalignment.h"

#define LINEAR_EDIST_GAP          ((GtUchar) UCHAR_MAX)
typedef enum {
  R,
  D,
  I,
  X /*unknown*/
} Edge;

typedef struct {
  GtUword Rvalue, Dvalue, Ivalue;

  Edge Redge,
       Dedge,
       Iedge;
}Atabentry;

typedef struct {
  GtUword idx;
  Edge edge;
}Rnode;

typedef struct {
  Rnode R,D,I;
}Rtabentry;

static GtUword add_safe(const GtUword val1, const GtUword val2)
{
  if (val1 != GT_UWORD_MAX && val2 != GT_UWORD_MAX)
  {
     gt_assert(val1+val2 >= val1 && val1+val2 >= val2);/*check overflow*/
     return val1+val2;
  }

    return GT_UWORD_MAX;
}

static Edge set_edge(const GtUword Rdist,
                     const GtUword Ddist,
                     const GtUword Idist)
{
  GtUword minvalue;

  minvalue = MIN3(Rdist, Ddist, Idist);

  if (Ddist == minvalue)
    return D;
  else if (Idist == minvalue)
    return I;
  else if (Rdist == minvalue)
    return R;
  return X;
}

static void set_Rtabentry(Rnode *rnode,
                          const Rtabentry *rtab,
                          const Edge edge )
{
  if (edge == R)
  {
    rnode->idx = rtab->R.idx;
    rnode->edge = rtab->R.edge;
  }
  if (edge == D)
  {
    rnode->idx = rtab->D.idx;
    rnode->edge = rtab->D.edge;
  }
  if (edge == I)
  {
    rnode->idx = rtab->I.idx;
    rnode->edge = rtab->I.edge;
  }
}

static void firstAtabRtabcolumn(const GtUword ulen,
                                Atabentry *Atabcolumn,
                                Rtabentry *Rtabcolumn,
                                const GtWord gap_opening,
                                const GtWord gap_extension,
                                Edge edge)
{
  GtUword rowindex;
  switch (edge) {
  case R:
    Atabcolumn[0].Rvalue = 0;
    Atabcolumn[0].Dvalue = GT_UWORD_MAX;
    Atabcolumn[0].Ivalue = GT_UWORD_MAX;
    break;
  case D:
    Atabcolumn[0].Rvalue = GT_UWORD_MAX;
    Atabcolumn[0].Dvalue = 0;
    Atabcolumn[0].Ivalue = GT_UWORD_MAX;
    break;
  case I:
    Atabcolumn[0].Rvalue = GT_UWORD_MAX;
    Atabcolumn[0].Dvalue = GT_UWORD_MAX;
    Atabcolumn[0].Ivalue = 0;
    break;
  default:
    Atabcolumn[0].Rvalue = 0;
    Atabcolumn[0].Dvalue = gap_opening;
    Atabcolumn[0].Ivalue = gap_opening;
  }

  Atabcolumn[0].Redge = X;
  Atabcolumn[0].Dedge = X;
  Atabcolumn[0].Iedge = X;

  Rtabcolumn[0].R.idx = 0;
  Rtabcolumn[0].D.idx = 0;
  Rtabcolumn[0].I.idx = 0;

  Rtabcolumn[0].R.edge = R;
  Rtabcolumn[0].D.edge = D;
  Rtabcolumn[0].I.edge = I;

  for (rowindex = 1; rowindex <= ulen; rowindex++)
  {
    Atabcolumn[rowindex].Rvalue = GT_UWORD_MAX;
    Atabcolumn[rowindex].Dvalue = add_safe(Atabcolumn[rowindex-1].Dvalue,
                                           gap_extension);
    Atabcolumn[rowindex].Ivalue = GT_UWORD_MAX;

    Atabcolumn[rowindex].Redge = X;
    Atabcolumn[rowindex].Dedge = D;
    Atabcolumn[rowindex].Iedge = X;

    Rtabcolumn[rowindex].R.idx = rowindex;
    Rtabcolumn[rowindex].D.idx = rowindex;
    Rtabcolumn[rowindex].I.idx = rowindex;

    Rtabcolumn[rowindex].R.edge = R;
    Rtabcolumn[rowindex].D.edge = D;
    Rtabcolumn[rowindex].I.edge = I;
  }
}

static void nextAtabRtabcolumn(const GtUchar *useq, const GtUword ustart,
                               const GtUword ulen,
                               const GtUchar b,
                               Atabentry *Atabcolumn,
                               Rtabentry *Rtabcolumn,
                               const GtWord matchcost,
                               const GtWord mismatchcost,
                               const GtWord gap_opening,
                               const GtWord gap_extension,
                               const GtUword midcolumn,
                               const GtUword colindex)
{
  Atabentry Anw, Awe;
  Rtabentry Rnw, Rwe;
  GtUword rcost, rowindex, Rdist,
          Ddist, Idist, minvalue;
  bool rtab = false;

  Anw = Atabcolumn[0];
  Rnw = Rtabcolumn[0];

  Rdist = add_safe(Atabcolumn[0].Rvalue,gap_extension+gap_opening);
  Ddist = add_safe(Atabcolumn[0].Dvalue,gap_extension+gap_opening);
  Idist = add_safe(Atabcolumn[0].Ivalue,gap_extension);

  minvalue = MIN3(Rdist, Ddist, Idist);
  Atabcolumn[0].Ivalue = minvalue;
  Atabcolumn[0].Rvalue = GT_UWORD_MAX;
  Atabcolumn[0].Dvalue = GT_UWORD_MAX;

  Atabcolumn[0].Redge = X;
  Atabcolumn[0].Dedge = X;
  Atabcolumn[0].Iedge = I;

  if (colindex > midcolumn)
  {
    Rnw = Rtabcolumn[0];
    Rtabcolumn[0].R.idx = Rtabcolumn[0].I.idx;
    Rtabcolumn[0].D.idx = Rtabcolumn[0].I.idx;
    Rtabcolumn[0].I.idx = Rtabcolumn[0].I.idx;

    Rtabcolumn[0].R.edge = X;
    Rtabcolumn[0].D.edge = X;
    Rtabcolumn[0].I.edge = Rtabcolumn[0].I.edge;

    rtab = true;
  }

  for (rowindex = 1; rowindex <= ulen; rowindex++)
  {
    Awe = Atabcolumn[rowindex];
    Rwe = Rtabcolumn[rowindex];

    rcost = useq[ustart+rowindex-1]==b? matchcost:mismatchcost;
    Rdist = add_safe(Anw.Rvalue, rcost);
    Ddist = add_safe(Anw.Dvalue, rcost);
    Idist = add_safe(Anw.Ivalue, rcost);

    minvalue = MIN3(Rdist, Ddist, Idist);
    Atabcolumn[rowindex].Rvalue = minvalue;
    Atabcolumn[rowindex].Redge = set_edge(Rdist, Ddist, Idist);

    Rdist = add_safe(Atabcolumn[rowindex-1].Rvalue,gap_extension+gap_opening);
    Ddist = add_safe(Atabcolumn[rowindex-1].Dvalue,gap_extension);
    Idist = add_safe(Atabcolumn[rowindex-1].Ivalue,gap_extension+gap_opening);

    minvalue = MIN3(Rdist, Ddist, Idist);
    Atabcolumn[rowindex].Dvalue = minvalue;
    Atabcolumn[rowindex].Dedge = set_edge(Rdist, Ddist, Idist);

    Rdist = add_safe(Awe.Rvalue,gap_extension+gap_opening);
    Ddist = add_safe(Awe.Dvalue,gap_extension+gap_opening);
    Idist = add_safe(Awe.Ivalue,gap_extension);

    minvalue = MIN3(Rdist, Ddist, Idist);
    Atabcolumn[rowindex].Ivalue = minvalue;
    Atabcolumn[rowindex].Iedge = set_edge(Rdist, Ddist, Idist);

    if (rtab)
    {
      set_Rtabentry(&Rtabcolumn[rowindex].R, &Rnw,
                     Atabcolumn[rowindex].Redge);
      set_Rtabentry(&Rtabcolumn[rowindex].D, &Rtabcolumn[rowindex-1],
                     Atabcolumn[rowindex].Dedge);
      set_Rtabentry(&Rtabcolumn[rowindex].I, &Rwe,
                    Atabcolumn[rowindex].Iedge);
    }
    Anw=Awe;
    Rnw=Rwe;
  }
}

static GtUword evaluateallAtabRtabcolumns(const GtUchar *useq,
                                          const GtUword ustart,
                                          const GtUword ulen,
                                          const GtUchar *vseq,
                                          const GtUword vstart,
                                          const GtUword vlen,
                                          Atabentry *Atabcolumn,
                                          Rtabentry *Rtabcolumn,
                                          const GtWord matchcost,
                                          const GtWord mismatchcost,
                                          const GtWord gap_opening,
                                          const GtWord gap_extension,
                                          GtUword midcolumn,
                                          Edge edge)
{
  GtUword colindex;

  firstAtabRtabcolumn(ulen, Atabcolumn, Rtabcolumn,
                      gap_opening, gap_extension, edge);

  for (colindex = 1UL; colindex <= vlen; colindex++)
  {
    nextAtabRtabcolumn(useq, ustart,ulen,
                       vseq[vstart+colindex-1],
                       Atabcolumn,
                       Rtabcolumn,
                       matchcost,
                       mismatchcost,
                       gap_opening,
                       gap_extension,
                       midcolumn,
                       colindex);
  }

  return MIN3(Atabcolumn[ulen].Rvalue,
              Atabcolumn[ulen].Dvalue,
              Atabcolumn[ulen].Ivalue);
}

static Edge minAdditionalCosts(const Atabentry entry,
                               const Edge edge,
                               const GtUword gap_opening)
{
  GtUword Rdist, Ddist, Idist, minvalue;
    Rdist = entry.Rvalue;
    Ddist = entry.Dvalue;
    Idist = entry.Ivalue;
  switch (edge) {
  case D:
    Rdist = entry.Rvalue+gap_opening;
    Ddist = entry.Dvalue;
    Idist = entry.Ivalue+gap_opening;
   break;
  case I:
    Rdist = entry.Rvalue+gap_opening;
    Ddist = entry.Dvalue+gap_opening;
    Idist = entry.Ivalue;
    break;
  default:
    Rdist = entry.Rvalue;
    Ddist = entry.Dvalue;
    Idist = entry.Ivalue;
  }

  minvalue = MIN3(Rdist, Ddist, Idist);
  if (Rdist == minvalue)
    return R;
  else if (Ddist == minvalue)
    return D;
  else if (Idist == minvalue)
    return I;
  return X;
}

static GtUword evaluateaffinecrosspoints(const GtUchar *useq,
                                         const GtUword ustart,
                                         const GtUword ulen,
                                         const GtUchar *vseq,
                                         const GtUword vstart,
                                         const GtUword vlen,
                                         Atabentry *Atabcolumn,
                                         Rtabentry *Rtabcolumn,
                                         GtUword *Ctab,
                                         GtUword rowoffset,
                                         const GtWord matchcost,
                                         const GtWord mismatchcost,
                                         const GtWord gap_opening,
                                         const GtWord gap_extension,
                                         Edge from_edge,Edge to_edge)
{
  GtUword  midrow, midcol, distance, colindex;
  Edge bottomtype, midtype;

  if (vlen >= 2UL)
  {
    midcol = GT_DIV2(vlen);
    distance = evaluateallAtabRtabcolumns(useq, ustart, ulen,
                                          vseq, vstart, vlen,
                                          Atabcolumn, Rtabcolumn,
                                          matchcost, mismatchcost,
                                          gap_opening,
                                          gap_extension,
                                          midcol, from_edge);

    bottomtype = minAdditionalCosts(Atabcolumn[ulen], to_edge, gap_opening);
    switch (bottomtype) {
      case R: midrow = (Rtabcolumn[ulen].R).idx;
              midtype = (Rtabcolumn[ulen].R).edge;
              break;
      case D: midrow = (Rtabcolumn[ulen].D).idx;
              midtype = (Rtabcolumn[ulen].D).edge;
              break;
      case I: midrow = (Rtabcolumn[ulen].I).idx;
              midtype = (Rtabcolumn[ulen].I).edge;
              break;
      case X: /*never reach this line*/
             fprintf(stderr,"the impossible happend\n");
             exit(GT_EXIT_PROGRAMMING_ERROR);
    }

    Ctab[midcol] = rowoffset + midrow;
    gt_assert(midcol > 0);
    if (midrow == 0) {
      for (colindex = midcol-1; colindex > 0; colindex--)
        Ctab[colindex] = Ctab[midcol];
    }
    else{/* upper left corner */
      switch (midtype) {
        case R:
          if (midcol > 1)
            Ctab[midcol-1] = (Ctab[midcol] == 0? 0:Ctab[midcol]-1);

            (void) evaluateaffinecrosspoints(useq, ustart, midrow-1,
                                             vseq, vstart, midcol-1,
                                             Atabcolumn,Rtabcolumn,
                                             Ctab,rowoffset,
                                             matchcost, mismatchcost,
                                             gap_opening,
                                             gap_extension,
                                             from_edge,midtype);
          break;
        case D:
          (void) evaluateaffinecrosspoints(useq,ustart,midrow-1,
                                           vseq,vstart,midcol,
                                           Atabcolumn,Rtabcolumn,
                                           Ctab,rowoffset,
                                           matchcost, mismatchcost,
                                           gap_opening,
                                           gap_extension,
                                           from_edge,midtype);
          break;
        case I:
          if (midcol>1)
            Ctab[midcol-1] = (Ctab[midcol]);
          (void) evaluateaffinecrosspoints(useq,ustart,midrow,
                                           vseq,vstart,midcol-1,
                                           Atabcolumn,Rtabcolumn,
                                           Ctab,rowoffset,
                                           matchcost, mismatchcost,
                                           gap_opening,
                                           gap_extension,
                                           from_edge,midtype);
          break;
        case X: /*never reach this line*/
                fprintf(stderr,"the impossible happend\n");
                exit(GT_EXIT_PROGRAMMING_ERROR);
      }
    }
   /*bottom right corner */
   evaluateaffinecrosspoints(useq, ustart+midrow, ulen-midrow,
                             vseq, vstart+midcol,vlen-midcol,
                             Atabcolumn,Rtabcolumn,
                             Ctab+midcol,rowoffset+midrow,
                             matchcost, mismatchcost,
                             gap_opening,
                             gap_extension,
                             midtype, to_edge);
    return distance;
  }
  return 0;
}

static void determineCtab0(GtUword *Ctab, GtUchar vseq0,
                          const GtUchar *useq,GtUword ustart,
                          const GtWord matchcost,
                          const GtWord mismatchcost,
                          const GtWord gap_opening)
{
  GtUword rowindex;

  if (Ctab[1] == 1 || Ctab[1] == 0)
  {
    Ctab[0] = 0; return;
  }
  else
  {
    if (Ctab[2]-Ctab[1] > 1)
    {
      if (gap_opening > (mismatchcost-matchcost))
      {
        Ctab[0] = 0; return;
      }
      else
      {
        for (rowindex = 0; rowindex < Ctab[1]; rowindex++)
        {
          if (vseq0 == useq[ustart+rowindex])
          {
            Ctab[0] = rowindex;
            return;
          }
        }
        Ctab[0] = 0; return;
      }
    }
    else
    {
      if (vseq0 == useq[ustart+Ctab[1]-1])
      {
          Ctab[0] = Ctab[1]-1; return;
      }
      else if (vseq0 == useq[ustart])
      {
          Ctab[0] = 0; return;
      }
      if (gap_opening > (mismatchcost-matchcost))
      {
        Ctab[0] = Ctab[1]-1; return;
      }
      else
      {
        for (rowindex = 0; rowindex < Ctab[1]; rowindex++)
        {
          if (vseq0 == useq[ustart+rowindex])
          {
            Ctab[0] = rowindex;
            return;
          }
        }
         Ctab[0] = Ctab[1]-1; return;
      }
    }
  }

  Ctab[0] = (Ctab[1] > 0) ?  Ctab[1]-1 : 0;

}

GtUword gt_calc_affinealign_linear(const GtUchar *useq, GtUword ustart,
                                   const GtUword ulen,
                                   const GtUchar *vseq, GtUword vstart,
                                   const GtUword vlen,
                                   GtAlignment *align,
                                   const GtWord matchcost,
                                   const GtWord mismatchcost,
                                   const GtWord gap_opening,
                                   const GtWord gap_extension)
{
  GtUword distance, *Ctab;
  Atabentry *Atabcolumn;
  Rtabentry *Rtabcolumn;
  GtAlignment *square_align;

  if (ulen == 0UL)
  {
      distance = construct_trivial_insertion_alignment(align, vlen,
                                                      gap_extension);
      distance += gap_opening;
  }
  else if (vlen == 0UL)
  {
      distance = construct_trivial_deletion_alignment(align, ulen,
                                                      gap_extension);
      distance += gap_opening;
  }
  else if (ulen == 1UL || vlen == 1UL )
  {
    square_align = gt_affinealign((const char*)useq+ustart, ulen,
                                  (const char*)vseq+vstart, vlen,
                                  (int)matchcost, (int)mismatchcost,
                                  (int)gap_opening,
                                  (int)gap_extension);
    gt_alignment_clone(square_align, align);

    distance = gt_alignment_eval_with_affine_score(align,matchcost,
                                                   mismatchcost,
                                                   gap_opening,
                                                   gap_extension);
    gt_alignment_delete(square_align);
  }
  else
  {
    Ctab = gt_malloc(sizeof *Ctab * (vlen+1));
    Atabcolumn = gt_malloc(sizeof *Atabcolumn * (ulen+1));
    Rtabcolumn = gt_malloc(sizeof *Rtabcolumn * (ulen+1));

    Ctab[vlen] = ulen;
    distance = evaluateaffinecrosspoints(useq, ustart, ulen, vseq, vstart, vlen,
                                         Atabcolumn, Rtabcolumn,
                                         Ctab, 0, matchcost, mismatchcost,
                                         gap_opening,gap_extension, X,X);

    determineCtab0(Ctab, vseq[vstart],useq, ustart,
                   matchcost, mismatchcost, gap_opening);

    reconstructalignment_from_Ctab(align, Ctab, vlen);

    gt_free(Ctab);
    gt_free(Atabcolumn);
    gt_free(Rtabcolumn);
  }

  return distance;
}

GtAlignment *gt_computeaffinelinearspace(const GtUchar *useq,
                                 const GtUword ustart, const GtUword ulen,
                                 const GtUchar *vseq,
                                 const GtUword vstart, const GtUword vlen,
                                 const GtWord matchcost,
                                 const GtWord mismatchcost,
                                 const GtWord gap_opening,
                                 const GtWord gap_extension)
{
  GtAlignment *align;

  gt_assert(useq && ulen && vseq && vlen);
  if (matchcost < 0 || mismatchcost < 0 || gap_opening < 0 || gap_extension < 0)
  {
    fprintf(stderr,"invalid cost value");
    exit(GT_EXIT_PROGRAMMING_ERROR);
  }
  align = gt_alignment_new_with_seqs(useq+ustart, ulen, vseq+vstart, vlen);
  (void) gt_calc_affinealign_linear(useq, ustart, ulen,
                                vseq, vstart, vlen,
                                align, matchcost, mismatchcost,
                                gap_opening,gap_extension);

  return align;
}

void gt_checkaffinelinearspace(GT_UNUSED bool forward,
                               const GtUchar *useq,
                               GtUword ulen,
                               const GtUchar *vseq,
                               GtUword vlen)
{
  GtAlignment *align_linear, *align_square;
  GtUword affine_score1, affine_score2, affine_score3;

  /*gt_assert(useq && ulen && vseq && vlen);*/
  if (strchr((const char*)useq, LINEAR_EDIST_GAP))
  {
    fprintf(stderr,"%s: sequence u contains gap symbol\n",__func__);
    exit(GT_EXIT_PROGRAMMING_ERROR);
  }
  if (strchr((const char*)vseq, LINEAR_EDIST_GAP))
  {
    fprintf(stderr,"%s: sequence v contains gap symbol\n",__func__);
    exit(GT_EXIT_PROGRAMMING_ERROR);
  }

  align_linear = gt_alignment_new_with_seqs(useq, ulen, vseq, vlen);
  affine_score1 = gt_calc_affinealign_linear(useq, 0, ulen,
                                             vseq, 0, vlen,
                                             align_linear, 0, 3, 4, 1);
  affine_score2 = gt_alignment_eval_with_affine_score(align_linear,0,3,4,1);

  if (affine_score1 != affine_score2)
  {
    fprintf(stderr,"gt_calc_affinealign_linear = "GT_WU" != "GT_WU
            " = gt_alignment_eval_with_affine_score\n", affine_score1,
                                                        affine_score2);

    exit(GT_EXIT_PROGRAMMING_ERROR);
  }

  align_square = gt_affinealign((const char *)useq, ulen,
                                (const char *)vseq, vlen,0,3,4,1);
  affine_score3 = gt_alignment_eval_with_affine_score(align_square,0,3,4,1);

  if (affine_score1 != affine_score3)
  {
    fprintf(stderr,"gt_calc_affinealign_linear = "GT_WU" != "GT_WU
            " = gt_affinealign\n", affine_score1, affine_score3);

    exit(GT_EXIT_PROGRAMMING_ERROR);
  }

  gt_alignment_delete(align_linear);
  gt_alignment_delete(align_square);
}
