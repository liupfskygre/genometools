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

#include <ctype.h>
#include <string.h>
#include "core/array2dim_api.h"
#include "core/minmax.h"
#include "core/types_api.h"
#include "core/divmodmul.h"
#include "core/ma_api.h"
#include "extended/affinealign.h"
#include "extended/diagonalbandalign.h"
#include "extended/linearalign_affinegapcost.h"
#include "extended/linearalign_utilities.h"
#include "extended/linearalign_affinegapcost.h"
#include "extended/reconstructalignment.h"

#include "extended/diagonalbandalign_affinegapcost.h"
#define LINEAR_EDIST_GAP          ((GtUchar) UCHAR_MAX)

static void diagonalband_fillDPtab_affine(AffinealignDPentry **Atabcolumn,
                                          const GtUchar *useq,
                                          GtUword ustart,
                                          GtUword ulen,
                                          const GtUchar *vseq,
                                          GtUword vstart,
                                          GtUword vlen,
                                          GtWord left_dist,
                                          GtWord right_dist,
                                          GtUword matchcost,
                                          GtUword mismatchcost,
                                          GtUword gap_opening,
                                          GtUword gap_extension,
                                          AffineAlignEdge from_edge,
                                          AffineAlignEdge edge)
{
  GtUword i,j, low_row, high_row;
  GtWord rcost, r_dist, d_dist, i_dist, minvalue;

  if ((left_dist > MIN(0, (GtWord)vlen-(GtWord)ulen))||
      (right_dist < MAX(0, (GtWord)vlen-(GtWord)ulen)))
  {
    gt_assert(false);
  }

  low_row = 0;
  high_row = -left_dist;

  /* first entry */
   switch (edge) {
    case Affine_R:
      Atabcolumn[0][0].Rvalue = 0;
      Atabcolumn[0][0].Redge = from_edge;
      Atabcolumn[0][0].Dvalue = GT_WORD_MAX;
      Atabcolumn[0][0].Ivalue = GT_WORD_MAX;
      break;
    case Affine_D:
      Atabcolumn[0][0].Rvalue = GT_WORD_MAX;
      Atabcolumn[0][0].Dvalue = 0;
      Atabcolumn[0][0].Dedge = from_edge;
      Atabcolumn[0][0].Ivalue = GT_WORD_MAX;
      break;
    case Affine_I:
      Atabcolumn[0][0].Rvalue = GT_WORD_MAX;
      Atabcolumn[0][0].Dvalue = GT_WORD_MAX;
      Atabcolumn[0][0].Ivalue = 0;
      Atabcolumn[0][0].Iedge = from_edge;
      break;
    default:
      Atabcolumn[0][0].Rvalue = 0;
      Atabcolumn[0][0].Dvalue = gap_opening;
      Atabcolumn[0][0].Ivalue = gap_opening;
    }
  /* first column */
  for (i = 1; i <= high_row; i++)
  {
    Atabcolumn[i][0].Rvalue = GT_WORD_MAX;
    r_dist = add_safe_max(Atabcolumn[i-1][0].Rvalue,
                         gap_opening + gap_extension);
    d_dist = add_safe_max(Atabcolumn[i-1][0].Dvalue, gap_extension);
    i_dist = add_safe_max(Atabcolumn[i-1][0].Ivalue,
                         gap_opening + gap_extension);
    Atabcolumn[i][0].Dvalue = MIN3(r_dist, d_dist, i_dist);
    Atabcolumn[i][0].Ivalue = GT_WORD_MAX;

    Atabcolumn[i][0].Redge = Affine_X;
    Atabcolumn[i][0].Dedge = set_edge(r_dist, d_dist, i_dist);
    Atabcolumn[i][0].Iedge = Affine_X;
  }
  for (; i <= ulen; i++)
  {
    /* invalid values */
    Atabcolumn[i][0].Rvalue = GT_WORD_MAX;
    Atabcolumn[i][0].Dvalue = GT_WORD_MAX;
    Atabcolumn[i][0].Ivalue = GT_WORD_MAX;
  }

  /* next columns */
  for (j = 1; j <= vlen; j++)
  {
    /* below diagonal band*/
    for (i = 0; i <= low_row; i++)
    {
      if (j <= right_dist)
      {
        Atabcolumn[i][j].Redge = Affine_X;
        Atabcolumn[i][j].Dedge = Affine_X;
        r_dist = add_safe_max(Atabcolumn[i][j-1].Rvalue,
                               gap_extension + gap_opening);
        d_dist = add_safe_max(Atabcolumn[i][j-1].Dvalue,
                               gap_extension + gap_opening);
        i_dist = add_safe_max(Atabcolumn[i][j-1].Ivalue,gap_extension);

        minvalue = MIN3(r_dist, d_dist, i_dist);
        Atabcolumn[i][j].Ivalue = minvalue;
        Atabcolumn[i][j].Rvalue = GT_WORD_MAX;
        Atabcolumn[i][j].Dvalue = GT_WORD_MAX;

        Atabcolumn[i][j].Iedge = set_edge(r_dist, d_dist, i_dist);
      }
      else{
        Atabcolumn[i][j].Rvalue = GT_WORD_MAX;
        Atabcolumn[i][j].Dvalue = GT_WORD_MAX;
        Atabcolumn[i][j].Ivalue = GT_WORD_MAX;
        Atabcolumn[i][j].Iedge = Affine_X;
      }
    }
    if ( j > right_dist)
      low_row++;
    if (high_row < ulen)
      high_row ++;

    /* diagonalband */
    for (; i <= high_row; i++)
    {
      /* compute A_affine(i,j,I) */
      r_dist=add_safe_max(Atabcolumn[i][j-1].Rvalue,gap_extension+gap_opening);
      d_dist=add_safe_max(Atabcolumn[i][j-1].Dvalue,gap_extension+gap_opening);
      i_dist=add_safe_max(Atabcolumn[i][j-1].Ivalue,gap_extension);
      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[i][j].Ivalue = minvalue;
      Atabcolumn[i][j].Iedge = set_edge(r_dist, d_dist, i_dist);

      /* compute A_affine(i,j,R) */
      rcost = tolower((int)useq[ustart+i-1]) ==
              tolower((int)vseq[vstart+j-1]) ?
              matchcost:mismatchcost;
      r_dist = add_safe_max(Atabcolumn[i-1][j-1].Rvalue, rcost);
      d_dist = add_safe_max(Atabcolumn[i-1][j-1].Dvalue, rcost);
      i_dist = add_safe_max(Atabcolumn[i-1][j-1].Ivalue, rcost);
      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[i][j].Rvalue = minvalue;
      Atabcolumn[i][j].Redge = set_edge(r_dist, d_dist, i_dist);

      /* compute A_affine(i,j,D) */
      r_dist = add_safe_max(Atabcolumn[i-1][j].Rvalue,
                         gap_extension+gap_opening);
      d_dist = add_safe_max(Atabcolumn[i-1][j].Dvalue,gap_extension);
      i_dist = add_safe_max(Atabcolumn[i-1][j].Ivalue,
                          gap_extension+gap_opening);
      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[i][j].Dvalue = minvalue;
      Atabcolumn[i][j].Dedge = set_edge(r_dist, d_dist, i_dist);

    }
    /* above diagonal band */
    for (; i <= ulen; i++)
    {
      Atabcolumn[i][j].Rvalue = GT_WORD_MAX;
      Atabcolumn[i][j].Dvalue = GT_WORD_MAX;
      Atabcolumn[i][j].Ivalue = GT_WORD_MAX;
    }
  }
}

/* calculate alignment with diagonalband in square space O(n²) with
 * affine gapcosts */
GtWord diagonalbandalignment_in_square_space_affine(GtAlignment *align,
                                                    const GtUchar *useq,
                                                    GtUword ustart,
                                                    GtUword ulen,
                                                    const GtUchar *vseq,
                                                    GtUword vstart,
                                                    GtUword vlen,
                                                    GtWord left_dist,
                                                    GtWord right_dist,
                                                    GtUword matchcost,
                                                    GtUword mismatchcost,
                                                    GtUword gap_opening,
                                                    GtUword gap_extension)
{
  GtWord distance;
  AffinealignDPentry **Atabcolumn;

  gt_assert(align != NULL);
  gt_array2dim_malloc(Atabcolumn, (ulen+1), (vlen+1));

  diagonalband_fillDPtab_affine(Atabcolumn, useq, ustart, ulen, vseq, vstart,
                                vlen, left_dist, right_dist, matchcost,
                                mismatchcost, gap_opening, gap_extension,
                                Affine_X, Affine_X);

  distance = MIN3(Atabcolumn[ulen][vlen].Rvalue,
                  Atabcolumn[ulen][vlen].Dvalue,
                  Atabcolumn[ulen][vlen].Ivalue);

  /* reconstruct alignment from 2dimarray Atabcolumn */
  affinealign_traceback(align, Atabcolumn, ulen, vlen);

  gt_array2dim_delete(Atabcolumn);
  return distance;
}

/* calculate only distance with diagonalband in square space O(n²) with
 * affine gapcosts */
static GtWord diagonalband_square_space_affine(const GtUchar *useq,
                                              GtUword ustart,
                                              GtUword ulen,
                                              const GtUchar *vseq,
                                              GtUword vstart,
                                              GtUword vlen,
                                              GtWord left_dist,
                                              GtWord right_dist,
                                              GtUword matchcost,
                                              GtUword mismatchcost,
                                              GtUword gap_opening,
                                              GtUword gap_extension)
{
  GtWord  distance;
  AffinealignDPentry **Atabcolumn;

   if ((left_dist > MIN(0, (GtWord)vlen-(GtWord)ulen))||
      (right_dist < MAX(0, (GtWord)vlen-(GtWord)ulen)))
  {
    return GT_WORD_MAX;
  }

  gt_array2dim_malloc(Atabcolumn, (ulen+1), (vlen+1));
  diagonalband_fillDPtab_affine(Atabcolumn, useq, ustart, ulen, vseq, vstart,
                                vlen, left_dist, right_dist, matchcost,
                                mismatchcost, gap_opening, gap_extension,
                                Affine_X, Affine_X);

  distance = MIN3(Atabcolumn[ulen][vlen].Rvalue,
                  Atabcolumn[ulen][vlen].Dvalue,
                  Atabcolumn[ulen][vlen].Ivalue);

  gt_array2dim_delete(Atabcolumn);
  return distance;
}

static Rnode evaluate_affineDBcrosspoints_from_2dimtab(AffineDiagentry *Dtab,
                                                AffinealignDPentry **Atabcolumn,
                                                GtUword ulen, GtUword vlen,
                                                GtUword gap_opening,
                                                GtUword rowoffset,
                                                AffineAlignEdge from_edge,
                                                AffineAlignEdge edge)
{
  GtUword i, j;
  Rnode rnode;
  Diagentry *tempnode;
  gt_assert(Atabcolumn != NULL);

  i = ulen;
  j = vlen;

  edge = minAdditionalCosts(&Atabcolumn[i][j], edge, gap_opening);

  switch (edge)
  {
    case Affine_I:
      tempnode = &Dtab[vlen].val_I;
      rnode = (Rnode) {vlen, Affine_I};
      break;
    case Affine_D:
      tempnode = &Dtab[vlen].val_D;
      rnode = (Rnode) {vlen, Affine_D};
      break;
    default:
      tempnode = &Dtab[vlen].val_R;
      rnode = (Rnode) {vlen, Affine_R};
  }

  while (i > 0 || j > 0) {
    if (j == vlen)
      rnode.edge = edge;
    switch (edge) {
      case Affine_R:
        gt_assert(Atabcolumn[i][j].Rvalue != GT_WORD_MAX);
        Dtab[j].val_R.currentrowindex = i + rowoffset;
        edge = Atabcolumn[i][j].Redge;
        tempnode->edge = Affine_R;
        tempnode = &Dtab[j].val_R;
        gt_assert(i > 0 && j > 0);
        i--;
        j--;
        break;
      case Affine_D:
        edge = Atabcolumn[i][j].Dedge;
        gt_assert(i);
        i--;
        break;
      case Affine_I:
        Dtab[j].val_I.currentrowindex = i + rowoffset;
        edge = Atabcolumn[i][j].Iedge;
        tempnode->edge = Affine_I;
        tempnode = &Dtab[j].val_I;
        gt_assert(j);
        j--;
        break;
      default:
        gt_assert(false);
    }
  }
  tempnode->edge = edge;
  /* special case for first crosspoint */
  Dtab[0].val_R = (Diagentry) {GT_UWORD_MAX, rowoffset, from_edge};
  Dtab[0].val_D = (Diagentry) {GT_UWORD_MAX, rowoffset, from_edge};
  Dtab[0].val_I = (Diagentry) {GT_UWORD_MAX, rowoffset, from_edge};

  return rnode;
}

/* create affine DBcrosspointtab to combine square calculating with linear
 * calculating. from_edge describes type of crosspoint node, edge describes the
 * incoming way to next unkonown crosspoint and to_edge describes type of
 * previous crosspoint.
 * Returns edge and index of lastcrosspoint in matrix.
 */
static Rnode  affineDtab_in_square_space(AffineDiagentry *Dtab,
                                 const GtUchar *useq,
                                 GtUword ustart,
                                 GtUword ulen,
                                 const GtUchar *vseq,
                                 GtUword vstart,
                                 GtUword vlen,
                                 GtWord left_dist,
                                 GtWord right_dist,
                                 GtUword matchcost,
                                 GtUword mismatchcost,
                                 GtUword gap_opening,
                                 GtUword gap_extension,
                                 GtUword rowoffset,
                                 AffineAlignEdge from_edge,
                                 AffineAlignEdge edge,
                                 AffineAlignEdge to_edge)
{
  AffinealignDPentry **Atabcolumn;

  gt_assert(Dtab != NULL);

  gt_array2dim_malloc(Atabcolumn, (ulen+1), (vlen+1));
  diagonalband_fillDPtab_affine(Atabcolumn, useq, ustart, ulen, vseq, vstart,
                                vlen, left_dist, right_dist, matchcost,
                                mismatchcost, gap_opening, gap_extension,
                                from_edge, edge);

  Rnode rnode = evaluate_affineDBcrosspoints_from_2dimtab(Dtab, Atabcolumn,
                                                 ulen, vlen, gap_opening,
                                                 rowoffset, from_edge, to_edge);
  gt_array2dim_delete(Atabcolumn);
  return rnode;
}

/* calculate only distance with diagonalband in linear space O(n)
 * with affine gapcosts */
static GtWord diagonalband_linear_affine(const GtUchar *useq,
                                         GtUword ustart,
                                         GtUword ulen,
                                         const GtUchar *vseq,
                                         GtUword vstart,
                                         GtUword vlen,
                                         GtWord left_dist,
                                         GtWord right_dist,
                                         GtUword matchcost,
                                         GtUword mismatchcost,
                                         GtUword gap_opening,
                                         GtUword gap_extension)
{
  GtUword colindex, rowindex, low_row, high_row, width;
  GtWord distance, rcost, r_dist, d_dist, i_dist, minvalue;
  AffinealignDPentry *Atabcolumn, northwestAffinealignDPentry,
                      westAffinealignDPentry;
  bool last_row = false;
  distance = GT_WORD_MAX;

  if ((left_dist > MIN(0, (GtWord)vlen-(GtWord)ulen))||
      (right_dist < MAX(0, (GtWord)vlen-(GtWord)ulen)))
  {
    return GT_WORD_MAX;
  }

  width = right_dist - left_dist + 1;
  Atabcolumn = gt_malloc(sizeof(*Atabcolumn) * width);

  low_row = 0;
  high_row = -left_dist;
  Atabcolumn[low_row].Rvalue = 0;
  Atabcolumn[low_row].Dvalue = gap_opening;
  Atabcolumn[low_row].Ivalue = gap_opening;

  for (rowindex = low_row+1; rowindex <= high_row; rowindex ++)
  {
    Atabcolumn[rowindex-low_row].Rvalue = GT_WORD_MAX;
    Atabcolumn[rowindex-low_row].Dvalue = add_safe_max(
                                          Atabcolumn[rowindex-low_row-1].Dvalue,
                                          gap_extension);
    Atabcolumn[rowindex-low_row].Ivalue = GT_WORD_MAX;
  }
  if (high_row == ulen)
    last_row = true;
  for (colindex = 1; colindex <= vlen; colindex++)
  {
    northwestAffinealignDPentry = Atabcolumn[0];
    if (high_row < ulen)
      high_row ++;
    if (colindex > right_dist)
    {
      westAffinealignDPentry = Atabcolumn[1];
      low_row++;
    }
    else
      westAffinealignDPentry = Atabcolumn[0];
    if (!last_row && rowindex == high_row)
      {
        westAffinealignDPentry.Rvalue = GT_WORD_MAX;
        westAffinealignDPentry.Dvalue = GT_WORD_MAX;
        westAffinealignDPentry.Ivalue = GT_WORD_MAX;
      }

    r_dist = add_safe_max(westAffinealignDPentry.Rvalue,
                          gap_extension+gap_opening);
    d_dist = add_safe_max(westAffinealignDPentry.Dvalue,
                          gap_extension+gap_opening);
    i_dist = add_safe_max(westAffinealignDPentry.Ivalue,gap_extension);
    minvalue = MIN3(r_dist, d_dist, i_dist);
    Atabcolumn[0].Ivalue = minvalue;
    Atabcolumn[0].Rvalue = GT_WORD_MAX;
    Atabcolumn[0].Dvalue = GT_WORD_MAX;

    if (low_row > 0 )
    {
      rcost = tolower((int)useq[ustart+rowindex-1]) ==
              tolower((int)vseq[vstart+colindex-1]) ?
                                                    matchcost:mismatchcost;
      r_dist = add_safe_max(northwestAffinealignDPentry.Rvalue, rcost);
      d_dist = add_safe_max(northwestAffinealignDPentry.Dvalue, rcost);
      i_dist = add_safe_max(northwestAffinealignDPentry.Ivalue, rcost);
      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[0].Rvalue = minvalue;
    }
    for (rowindex = low_row + 1; rowindex <= high_row; rowindex++)
    {
      northwestAffinealignDPentry = westAffinealignDPentry;
      if (!last_row && rowindex == high_row)
      {
        westAffinealignDPentry.Rvalue = GT_WORD_MAX;
        westAffinealignDPentry.Dvalue = GT_WORD_MAX;
        westAffinealignDPentry.Ivalue = GT_WORD_MAX;
      }
      else if (low_row > 0)
        westAffinealignDPentry = Atabcolumn[rowindex-low_row+1];
      else
        westAffinealignDPentry = Atabcolumn[rowindex-low_row];

      if (rowindex == ulen)
        last_row = true;

      r_dist = add_safe_max(westAffinealignDPentry.Rvalue,
                            gap_extension+gap_opening);
      d_dist = add_safe_max(westAffinealignDPentry.Dvalue,
                            gap_extension+gap_opening);
      i_dist = add_safe_max(westAffinealignDPentry.Ivalue,gap_extension);

      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[rowindex-low_row].Ivalue = minvalue;

      rcost = tolower((int)useq[ustart+rowindex-1]) ==
              tolower((int)vseq[vstart+colindex-1]) ?
                                                    matchcost:mismatchcost;
      r_dist = add_safe_max(northwestAffinealignDPentry.Rvalue, rcost);
      d_dist = add_safe_max(northwestAffinealignDPentry.Dvalue, rcost);
      i_dist = add_safe_max(northwestAffinealignDPentry.Ivalue, rcost);

      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[rowindex-low_row].Rvalue = minvalue;

      r_dist = add_safe_max(Atabcolumn[rowindex-low_row-1].Rvalue,
                            gap_extension+gap_opening);
      d_dist = add_safe_max(Atabcolumn[rowindex-low_row-1].Dvalue,
                           gap_extension);
      i_dist = add_safe_max(Atabcolumn[rowindex-low_row-1].Ivalue,
                           gap_extension+gap_opening);

      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[rowindex-low_row].Dvalue = minvalue;
    }
  }

  distance = MIN3(Atabcolumn[high_row-low_row].Rvalue,
                  Atabcolumn[high_row-low_row].Dvalue,
                  Atabcolumn[high_row-low_row].Ivalue);
  gt_free(Atabcolumn);

  return distance;
}

/* helpfunctions */
static void inline  set_invalid_Diagentry(Diagentry *node)
{
  gt_assert(node != NULL);
  node->currentrowindex = GT_UWORD_MAX;
  node->edge = Affine_X;
  node->lastcpoint = GT_UWORD_MAX;
}

static void inline set_valid_Diagentry(Diagentry *node_to,
                                       const Rtabentry *entry_from,
                                       GtWord minvalue, GtWord r_dist,
                                       GtWord i_dist, GtWord d_dist)
{
  gt_assert(node_to != NULL && entry_from != NULL);
  if (minvalue == r_dist)
  {
    node_to->edge = entry_from->val_R.edge;
    node_to->lastcpoint = entry_from->val_R.idx;
  }
  else if (minvalue == i_dist)
  {
    node_to->edge = entry_from->val_I.edge;
    node_to->lastcpoint = entry_from->val_I.idx;
  }
  else if (minvalue == d_dist)
  {
    node_to->edge = entry_from->val_D.edge;
    node_to->lastcpoint = entry_from->val_D.idx;
  }
}

static void inline set_invalid_Rnode(Rnode *node)
{
  gt_assert(node != NULL);
  node->idx = GT_UWORD_MAX;
  node->edge = Affine_X;
}

static void inline set_valid_Rnode(Rnode *node_to, Rtabentry *entry_from,
                                   GtWord minvalue, GtWord r_dist,
                                   GtWord i_dist, GtWord d_dist)
{
  gt_assert(node_to != NULL && entry_from != NULL);
  if (minvalue == r_dist)
    *node_to = entry_from->val_R;
  else if (minvalue == i_dist)
     *node_to = entry_from->val_I;
  else if (minvalue == d_dist)
     *node_to = entry_from->val_D;
}

/* calculate first column */
static void firstaffineDBtabcolumn(AffinealignDPentry *Atabcolumn,
                                   Rtabentry *Rtabcolumn,
                                   AffineDiagentry *Diagcolumn,
                                   AffineAlignEdge edge,
                                   AffineAlignEdge from_edge,
                                   GtUword offset,
                                   GtWord left_dist,
                                   GtWord right_dist,
                                   GtUword gap_opening,
                                   GtUword gap_extension)
{
  GtUword rowindex, low_row, high_row;
  GtWord diag;

  diag = GT_DIV2(left_dist + right_dist);
  low_row = 0;
  high_row = -left_dist;

  Atabcolumn[low_row].Rvalue = GT_WORD_MAX;
  Atabcolumn[low_row].Dvalue = GT_WORD_MAX;
  Atabcolumn[low_row].Ivalue = GT_WORD_MAX;

  set_invalid_Diagentry(&Diagcolumn[0].val_R);
  set_invalid_Diagentry(&Diagcolumn[0].val_D);
  set_invalid_Diagentry(&Diagcolumn[0].val_I);

  set_invalid_Rnode(&Rtabcolumn[0].val_R);
  set_invalid_Rnode(&Rtabcolumn[0].val_D);
  set_invalid_Rnode(&Rtabcolumn[0].val_I);

  switch (edge) {
  case Affine_R:
    Atabcolumn[low_row].Rvalue = 0;
    Rtabcolumn[0].val_R.edge = from_edge;
    if (diag == 0)
    {
      Diagcolumn[0].val_R.currentrowindex = 0 + offset;
      Diagcolumn[0].val_R.edge = from_edge;
      Rtabcolumn[0].val_R.idx = 0;
      Rtabcolumn[0].val_R.edge = Affine_R;
    }
    break;
  case Affine_D:
    Atabcolumn[low_row].Dvalue = 0;
    Rtabcolumn[0].val_D.edge = from_edge;
    if (diag == 0)
    {
      Diagcolumn[0].val_D.currentrowindex = 0 + offset;
      Diagcolumn[0].val_D.edge = from_edge;
      Rtabcolumn[0].val_D.idx = 0;
      Rtabcolumn[0].val_D.edge = Affine_D;
    }
    break;
  case Affine_I:
    Atabcolumn[low_row].Ivalue = 0;
    Rtabcolumn[0].val_I.edge = from_edge;
    if (diag == 0)
    {
      Diagcolumn[0].val_I.currentrowindex = 0 + offset;
      Diagcolumn[0].val_I.edge = from_edge;
      Rtabcolumn[0].val_I.idx = 0;
      Rtabcolumn[0].val_I.edge = Affine_I;
    }
    break;
  default:
    Atabcolumn[low_row].Rvalue = 0;
    Atabcolumn[low_row].Dvalue = gap_opening;
    Atabcolumn[low_row].Ivalue = gap_opening;
    Rtabcolumn[0].val_I.edge = from_edge;
    Rtabcolumn[0].val_R.edge = from_edge;
    Rtabcolumn[0].val_D.edge = from_edge;
    if (diag == 0)
    {
      Diagcolumn[0].val_R.currentrowindex = 0 + offset;
      Diagcolumn[0].val_D.currentrowindex = 0 + offset;
      Diagcolumn[0].val_I.currentrowindex = 0 + offset;

      Rtabcolumn[0].val_R.idx = 0;
      Rtabcolumn[0].val_R.edge = Affine_R;
      Rtabcolumn[0].val_D.idx = 0;
      Rtabcolumn[0].val_D.edge = Affine_D;
      Rtabcolumn[0].val_I.idx = 0;
      Rtabcolumn[0].val_I.edge = Affine_I;
    }
  }

  for (rowindex = low_row+1; rowindex <= high_row; rowindex++)
  {
    Atabcolumn[rowindex-low_row].Rvalue = GT_WORD_MAX;
    Atabcolumn[rowindex-low_row].Dvalue = add_safe_max(
                                          Atabcolumn[rowindex-low_row-1].Dvalue,
                                          gap_extension);
    Atabcolumn[rowindex-low_row].Ivalue = GT_WORD_MAX;

    if (diag == -(GtWord)rowindex)
    {
      Diagcolumn[0].val_D.edge = from_edge;
      Diagcolumn[0].val_D.lastcpoint = GT_UWORD_MAX;
      Diagcolumn[0].val_D.currentrowindex = rowindex + offset;
      Rtabcolumn[rowindex-low_row].val_D.idx = 0;
      Rtabcolumn[rowindex-low_row].val_D.edge = Affine_D;
      set_invalid_Diagentry(&Diagcolumn[0].val_R);
      set_invalid_Diagentry(&Diagcolumn[0].val_I);
    }
    else
    {
      Rtabcolumn[rowindex-low_row] = Rtabcolumn[rowindex-low_row-1];
    }
  }

}

/* calculate all columns */
static Rnode evaluateallaffineDBcolumns(AffinealignDPentry *Atabcolumn,
                                        Rtabentry *Rtabcolumn,
                                        AffineDiagentry *Diagcolumn,
                                        AffineAlignEdge edge,
                                        AffineAlignEdge from_edge,
                                        AffineAlignEdge to_edge,
                                        GtUword offset,
                                        const GtUchar *useq,
                                        GtUword ustart, GtUword ulen,
                                        const GtUchar *vseq,
                                        GtUword vstart, GtUword vlen,
                                        GtWord left_dist, GtWord right_dist,
                                        GtUword matchcost,
                                        GtUword mismatchcost,
                                        GtUword gap_opening,
                                        GtUword gap_extension)
{
  GtUword colindex, rowindex, low_row, high_row;
  /*lowest and highest row between a diagonal band*/
  GtWord diag, r_dist, d_dist, i_dist, minvalue, rcost;

  bool last_row = false;
  AffinealignDPentry northwestAffinealignDPentry, westAffinealignDPentry;
  Rtabentry northwestRtabentry, westRtabentry;
  Rnode lastcpoint = {GT_UWORD_MAX, Affine_X};

  if ((left_dist > MIN(0, (GtWord)vlen-(GtWord)ulen))||
      (right_dist < MAX(0, (GtWord)vlen-(GtWord)ulen)))
  {
    gt_assert(false);
  }

  diag = GT_DIV2(left_dist + right_dist);
  low_row = 0;
  high_row = -left_dist;
  if (high_row == ulen)
    last_row = true;

 /* first column */
  firstaffineDBtabcolumn(Atabcolumn, Rtabcolumn, Diagcolumn, edge, from_edge,
                   offset, left_dist, right_dist, gap_opening, gap_extension);
  /* next columns */
  for (colindex = 1; colindex <= vlen; colindex++)
  {
    northwestAffinealignDPentry = Atabcolumn[0];
    northwestRtabentry = Rtabcolumn[0];

    if (high_row < ulen)
      high_row ++;
    if (colindex > right_dist)
    {
      westAffinealignDPentry = Atabcolumn[1];
      westRtabentry = Rtabcolumn[1];
      low_row++;
    }
    else
    {
      westAffinealignDPentry = Atabcolumn[0];
      westRtabentry = Rtabcolumn[0];
    }
    if (!last_row && low_row == high_row)
    {/* prev is outside of diagonalband*/
      westAffinealignDPentry = (AffinealignDPentry)
                               {GT_WORD_MAX, GT_WORD_MAX, GT_WORD_MAX};
      westRtabentry.val_R = (Rnode) {GT_UWORD_MAX, Affine_X};
      westRtabentry.val_D = (Rnode) {GT_UWORD_MAX, Affine_X};
      westRtabentry.val_I = (Rnode) {GT_UWORD_MAX, Affine_X};
    }

    /* insertion */
    r_dist = add_safe_max(westAffinealignDPentry.Rvalue,
                          gap_extension + gap_opening);
    d_dist = add_safe_max(westAffinealignDPentry.Dvalue,
                          gap_extension + gap_opening);
    i_dist = add_safe_max(westAffinealignDPentry.Ivalue, gap_extension);

    minvalue = MIN3(r_dist, d_dist, i_dist);
    Atabcolumn[0].Ivalue = minvalue;
    Atabcolumn[0].Rvalue = GT_WORD_MAX;
    Atabcolumn[0].Dvalue = GT_WORD_MAX;

    if (diag == (GtWord)colindex - (GtWord)low_row)
    {
      set_invalid_Diagentry(&Diagcolumn[colindex].val_R);
      set_invalid_Diagentry(&Diagcolumn[colindex].val_D);
      set_valid_Diagentry(&Diagcolumn[colindex].val_I, &westRtabentry, minvalue,
                          r_dist, i_dist, d_dist);
      Diagcolumn[colindex].val_I.currentrowindex = low_row + offset;
      set_invalid_Rnode(&Rtabcolumn[0].val_R);
      set_invalid_Rnode(&Rtabcolumn[0].val_D);
      Rtabcolumn[0].val_I.idx = colindex;
      Rtabcolumn[0].val_I.edge = Affine_I;
    }
    else
    {
      set_valid_Rnode(&Rtabcolumn[0].val_I, &westRtabentry, minvalue,
                       r_dist, i_dist, d_dist);
      Rtabcolumn[0].val_D = (Rnode) {GT_UWORD_MAX, Affine_X};
      Rtabcolumn[0].val_R = (Rnode) {GT_UWORD_MAX, Affine_X};
    }

    /* replacement possible for 0-entry */
    if (low_row > 0 )
    {
      rcost = tolower((int)useq[ustart+low_row-1]) ==
              tolower((int)vseq[vstart+colindex-1]) ?
                                                   matchcost:mismatchcost;
      r_dist = add_safe_max(northwestAffinealignDPentry.Rvalue, rcost);
      d_dist = add_safe_max(northwestAffinealignDPentry.Dvalue, rcost);
      i_dist = add_safe_max(northwestAffinealignDPentry.Ivalue, rcost);

      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[0].Rvalue = minvalue;

      if (diag == (GtWord)colindex - (GtWord)low_row)
      {
        set_valid_Diagentry(&Diagcolumn[colindex].val_R, &northwestRtabentry,
                            minvalue, r_dist, i_dist, d_dist);
        Diagcolumn[colindex].val_R.currentrowindex = low_row + offset;
        Rtabcolumn[0].val_R.idx = colindex;
        Rtabcolumn[0].val_R.edge = Affine_R;
      }
      else
      {
        set_valid_Rnode(&Rtabcolumn[0].val_R, &northwestRtabentry,
                         minvalue, r_dist, i_dist, d_dist);
      }
    }
    for (rowindex = low_row + 1; rowindex <= high_row; rowindex++)
    {
      northwestAffinealignDPentry = westAffinealignDPentry;
      northwestRtabentry = westRtabentry;

      if (!last_row && rowindex == high_row)
      {/* prev is outside of diagonalband*/
        westAffinealignDPentry = (AffinealignDPentry)
                                 {GT_WORD_MAX, GT_WORD_MAX, GT_WORD_MAX};
        westRtabentry.val_R = (Rnode) {GT_UWORD_MAX, Affine_X};
        westRtabentry.val_D = (Rnode) {GT_UWORD_MAX, Affine_X};
        westRtabentry.val_I = (Rnode) {GT_UWORD_MAX, Affine_X};
      }
      else if (low_row > 0)
      {/* shifted diagonalband*/
        westAffinealignDPentry = Atabcolumn[rowindex-low_row+1];
        westRtabentry = Rtabcolumn[rowindex-low_row+1];
      }
      else
      {/* normaly prev*/
        westAffinealignDPentry = Atabcolumn[rowindex-low_row];
        westRtabentry = Rtabcolumn[rowindex-low_row];
      }
      if (rowindex == ulen)
        last_row = true;

      /* insertion */
      r_dist = add_safe_max(westAffinealignDPentry.Rvalue,
                            gap_extension+gap_opening);
      d_dist = add_safe_max(westAffinealignDPentry.Dvalue,
                            gap_extension+gap_opening);
      i_dist = add_safe_max(westAffinealignDPentry.Ivalue,gap_extension);

      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[rowindex-low_row].Ivalue = minvalue;
      if (diag == (GtWord)colindex - (GtWord)rowindex)
      {
        set_valid_Diagentry(&Diagcolumn[colindex].val_I, &westRtabentry,
                             minvalue, r_dist, i_dist, d_dist);
        Diagcolumn[colindex].val_I.currentrowindex = rowindex+offset;
        Rtabcolumn[rowindex-low_row].val_I.idx = colindex;
        Rtabcolumn[rowindex-low_row].val_I.edge = Affine_I;
      }
      else
      {
        set_valid_Rnode(&Rtabcolumn[rowindex-low_row].val_I, &westRtabentry,
                         minvalue,r_dist,i_dist,d_dist);
      }
      /* replacement */
      rcost = tolower((int)useq[ustart+rowindex-1]) ==
              tolower((int)vseq[vstart+colindex-1]) ?
                                                   matchcost:mismatchcost;
      r_dist = add_safe_max(northwestAffinealignDPentry.Rvalue, rcost);
      d_dist = add_safe_max(northwestAffinealignDPentry.Dvalue, rcost);
      i_dist = add_safe_max(northwestAffinealignDPentry.Ivalue, rcost);
      minvalue = MIN3(r_dist, d_dist, i_dist);

      Atabcolumn[rowindex-low_row].Rvalue = minvalue;
      if (diag == (GtWord)colindex - (GtWord)rowindex)
      {
        set_valid_Diagentry(&Diagcolumn[colindex].val_R, &northwestRtabentry,
                            minvalue, r_dist, i_dist, d_dist);
        Diagcolumn[colindex].val_R.currentrowindex = rowindex+offset;
        Rtabcolumn[rowindex-low_row].val_R.idx = colindex;
        Rtabcolumn[rowindex-low_row].val_R.edge = Affine_R;
      }
      else
      {
        set_valid_Rnode(&Rtabcolumn[rowindex-low_row].val_R,&northwestRtabentry,
                         minvalue,r_dist,i_dist,d_dist);
      }

      /* deletion */
      r_dist = add_safe_max(Atabcolumn[rowindex-low_row-1].Rvalue,
                           gap_extension+gap_opening);
      d_dist = add_safe_max(Atabcolumn[rowindex-low_row-1].Dvalue,
                           gap_extension);
      i_dist = add_safe_max(Atabcolumn[rowindex-low_row-1].Ivalue,
                           gap_extension+gap_opening);

      minvalue = MIN3(r_dist, d_dist, i_dist);
      Atabcolumn[rowindex-low_row].Dvalue = minvalue;

      if (diag == (GtWord)colindex - (GtWord)rowindex)
      {
        set_valid_Diagentry(&Diagcolumn[colindex].val_D,
                           &Rtabcolumn[rowindex-low_row-1], minvalue,
                            r_dist,i_dist, d_dist);
        Diagcolumn[colindex].val_D.currentrowindex = rowindex+offset;
        Rtabcolumn[rowindex-low_row].val_D.idx = colindex;
        Rtabcolumn[rowindex-low_row].val_D.edge = Affine_D;
      }
      else
      {
        set_valid_Rnode(&Rtabcolumn[rowindex-low_row].val_D,
                        &Rtabcolumn[rowindex-low_row-1], minvalue,
                         r_dist,i_dist,d_dist);
      }
    }

  }
  /* last crosspoint of optimal path */
  r_dist = Atabcolumn[high_row-low_row].Rvalue;
  d_dist = Atabcolumn[high_row-low_row].Dvalue;
  i_dist = Atabcolumn[high_row-low_row].Ivalue;

  switch (to_edge)
  {
    case Affine_I:
      r_dist = add_safe_max (r_dist, gap_opening);
      d_dist = add_safe_max (d_dist, gap_opening);
      break;
    case Affine_D:
      r_dist = add_safe_max (r_dist, gap_opening);
      i_dist = add_safe_max (i_dist, gap_opening);
      break;
    default:
      break;
  }

  minvalue = MIN3(r_dist, d_dist, i_dist);
  if (minvalue == r_dist)
    lastcpoint = Rtabcolumn[high_row-low_row].val_R;
  else if (minvalue == i_dist)
    lastcpoint = Rtabcolumn[high_row-low_row].val_I;
  else if (minvalue == d_dist)
    lastcpoint = Rtabcolumn[high_row-low_row].val_D;

  return lastcpoint;
}

/* calculate affine crosspoint realting to diagonal in recursive way */
static Rnode evaluateaffineDBcrosspoints(AffinealignDPentry *Atabcolumn,
                                         Rtabentry *Rtabcolumn,
                                         AffineDiagentry *Diagcolumn,
                                         AffineAlignEdge edge,
                                         AffineAlignEdge from_edge,
                                         AffineAlignEdge to_edge,
                                         GtUword rowoffset,
                                         GtUword coloffset,
                                         const GtUchar *useq,
                                         GtUword ustart,
                                         GtUword ulen,
                                         GtUword original_ulen,
                                         const GtUchar *vseq,
                                         GtUword vstart,
                                         GtUword vlen,
                                         GtWord left_dist,
                                         GtWord right_dist,
                                         GtUword matchcost,
                                         GtUword mismatchcost,
                                         GtUword gap_opening,
                                         GtUword gap_extension)
{
  GtUword idx, currentrowindex, new_ulen;
  GtWord new_left, new_right, diag;
  Diagentry cpoint, prevcpoint;
  Rnode rpoint, temprpoint, lastrpoint;
  AffineAlignEdge cedge;

  diag = GT_DIV2(left_dist+right_dist);
  if (ulen == 0)
  {
    for (idx = 0; idx <=vlen; idx++)
    {
      Diagcolumn[idx].val_I.currentrowindex = rowoffset;
      Diagcolumn[idx].val_I.edge = Affine_I;
    }
    Diagcolumn[0].val_I.edge = from_edge;
    return (Rnode) {vlen, Affine_I};
  }

  if (vlen == 0)
  {
    Diagcolumn[0].val_D.currentrowindex = ulen;
    Diagcolumn[0].val_D.edge = from_edge;
    return (Rnode) {0, Affine_D};
  }

  if ((ulen+1)*(vlen+1) <= (original_ulen+1))
  { /* product of subsquences is in O(n) */
    return affineDtab_in_square_space(Diagcolumn, useq, ustart, ulen, vseq,
                                      vstart, vlen, left_dist, right_dist,
                                      matchcost, mismatchcost, gap_opening,
                                      gap_extension, rowoffset, from_edge, edge,
                                      to_edge);
  }

  rpoint = evaluateallaffineDBcolumns(Atabcolumn, Rtabcolumn, Diagcolumn, edge,
                              from_edge, to_edge, rowoffset, useq, ustart, ulen,
                              vseq, vstart, vlen, left_dist, right_dist,
                              matchcost, mismatchcost,
                              gap_opening, gap_extension);
  lastrpoint = rpoint;
  idx = rpoint.idx;

  /* if no crosspoint is found */
  if (idx == GT_UWORD_MAX)
  {
    if (diag < 0)
      return evaluateaffineDBcrosspoints(Atabcolumn, Rtabcolumn,Diagcolumn,
                                edge, from_edge, to_edge, rowoffset, coloffset,
                                useq, ustart, ulen, original_ulen, vseq, vstart,
                                vlen, diag+1, right_dist, matchcost,
                                mismatchcost, gap_opening, gap_extension);
    else if (diag > 0)
      return evaluateaffineDBcrosspoints(Atabcolumn, Rtabcolumn, Diagcolumn,
                                edge, from_edge, to_edge, rowoffset, coloffset,
                                useq, ustart, ulen, original_ulen, vseq, vstart,
                                vlen, left_dist, diag-1, matchcost,
                                mismatchcost, gap_opening, gap_extension);
    else
    {
      gt_assert(false); /* there have to be an crosspoint */
    }
  }
  else
  {
    switch (rpoint.edge) {
    case Affine_R:
      cpoint = Diagcolumn[rpoint.idx].val_R;
      cedge = Affine_R;
      currentrowindex = Diagcolumn[rpoint.idx].val_R.currentrowindex;
      break;
    case Affine_D:
      cpoint = Diagcolumn[rpoint.idx].val_D;
      cedge = Affine_D;
      currentrowindex = Diagcolumn[rpoint.idx].val_D.currentrowindex;
      break;
    case Affine_I:
      cpoint = Diagcolumn[rpoint.idx].val_I;
      cedge = Affine_I;
      currentrowindex = Diagcolumn[rpoint.idx].val_I.currentrowindex;
      break;
    default:
      gt_assert(false);
    }
  }

  /* exception, if last cpoint != (m+1)entry */
  if (idx != vlen)
  {;
    if (diag + ((GtWord)ulen-(GtWord)vlen) > 0)
    {
      new_left = MAX((GtWord)left_dist-diag+1,
                    -((GtWord)ulen - ((GtWord)currentrowindex+1
                    -(GtWord)rowoffset)));
      new_right = 0;
      new_ulen =  ulen - (currentrowindex+1-rowoffset);

      lastrpoint = evaluateaffineDBcrosspoints(Atabcolumn, Rtabcolumn,
                           Diagcolumn+idx, Affine_D, cpoint.edge, to_edge,
                           currentrowindex+1, coloffset+idx, useq,
                           currentrowindex+1, new_ulen, original_ulen, vseq,
                           vstart+idx, vlen-idx, new_left, new_right,
                           matchcost, mismatchcost, gap_opening, gap_extension);
      lastrpoint.idx += idx;
    }
    else
    {
      new_left = -1;
      new_right =  MIN((GtWord)right_dist-((GtWord)diag)-1,
                      ((GtWord)vlen-(GtWord)idx-1));
      new_ulen = ulen - (currentrowindex-rowoffset);

      lastrpoint = evaluateaffineDBcrosspoints(
                           Atabcolumn, Rtabcolumn, Diagcolumn+idx+1,
                           Affine_I, cedge, to_edge, currentrowindex,
                           coloffset+idx+1, useq, currentrowindex, new_ulen,
                           original_ulen, vseq, vstart+idx+1, vlen-idx-1,
                           new_left, new_right,
                           matchcost, mismatchcost, gap_opening, gap_extension);
      lastrpoint.idx += (idx+1);
    }
  }

  /* look at all 'normally' crosspoints */
  while (cpoint.lastcpoint != GT_UWORD_MAX)
  {
    rpoint = (Rnode) {idx, cedge};
    prevcpoint = cpoint;

    idx = prevcpoint.lastcpoint;
    switch (prevcpoint.edge) {
    case Affine_R:
      cpoint = Diagcolumn[idx].val_R;
      cedge = Affine_R;
      currentrowindex = Diagcolumn[idx].val_R.currentrowindex;
      break;
    case Affine_D:
      cpoint = Diagcolumn[idx].val_D;
      cedge = Affine_D;
      currentrowindex = Diagcolumn[idx].val_D.currentrowindex;
      break;
    case Affine_I:
      cpoint = Diagcolumn[idx].val_I;
      cedge = Affine_I;
      currentrowindex = Diagcolumn[idx].val_I.currentrowindex;
      break;
    default:
      cedge = Affine_X;
      gt_assert(false);
    }

    if (rpoint.edge == Affine_R ||
       ((rpoint.edge == Affine_I) && (rpoint.idx-idx == 1)))
    {
      continue;/* next crosspoint is also on the diagonal*/
    }
    else if (rpoint.edge == Affine_D)
    {
      new_left = -1;
      new_right = MIN(right_dist-diag-1,
                     (GtWord)rpoint.idx-(GtWord)idx-1);
      new_ulen = prevcpoint.currentrowindex-
                 currentrowindex-1;

      temprpoint = evaluateaffineDBcrosspoints(
                           Atabcolumn, Rtabcolumn,Diagcolumn+idx+1,
                           Affine_I, cedge, Affine_D, currentrowindex,
                           coloffset + idx + 1, useq, currentrowindex, new_ulen,
                           original_ulen, vseq, vstart + idx + 1,
                           rpoint.idx-idx-1, new_left, new_right,
                           matchcost, mismatchcost, gap_opening, gap_extension);
      if (temprpoint.idx + idx + 1 < vlen)
      {
        GtUword update_idx = temprpoint.idx+1+idx+1;
        Diagcolumn[update_idx].val_R.edge = temprpoint.edge;
        Diagcolumn[update_idx].val_D.edge = temprpoint.edge;
        Diagcolumn[update_idx].val_I.edge = temprpoint.edge;
      }
      if (temprpoint.idx + idx + 1 == lastrpoint.idx)
      {
        lastrpoint = temprpoint;
        lastrpoint.idx += idx + 1;
      }
    }
    else if (rpoint.edge == Affine_I)
    {
      new_left = MAX(left_dist-diag+1,
                        -((GtWord)Diagcolumn[rpoint.idx].val_I.currentrowindex-
                          (GtWord)currentrowindex-1));
      new_right = 0;
      new_ulen = prevcpoint.currentrowindex-currentrowindex-1;

      temprpoint = evaluateaffineDBcrosspoints(
                         Atabcolumn, Rtabcolumn, Diagcolumn + idx,
                         Affine_D, cpoint.edge, Affine_I, currentrowindex + 1,
                         coloffset + idx, useq, currentrowindex+1, new_ulen,
                         original_ulen, vseq, vstart+idx, rpoint.idx-1-idx,
                         new_left, new_right, matchcost, mismatchcost,
                         gap_opening, gap_extension);
      Diagcolumn[rpoint.idx].val_I.edge = temprpoint.edge;
    }
    else
    {
      /* if (Diagcolumn[cpoint].edge == Linear_X), never reach this line */
      gt_assert(false);
    }
  }

  /* exception, if first crosspoint != 0-entry */
   if (vstart-coloffset != idx)
   {
     switch (cedge) {
     case Affine_D:
       new_left =  MAX(-((GtWord)Diagcolumn[idx].val_D.currentrowindex-
                        (GtWord)ustart-1), left_dist);
       new_right = MIN(right_dist, (GtWord)idx);
       new_ulen = Diagcolumn[idx].val_D.currentrowindex-ustart-1;

       rpoint = evaluateaffineDBcrosspoints(Atabcolumn, Rtabcolumn, Diagcolumn,
                                 edge, from_edge,Affine_D,rowoffset, coloffset,
                                 useq, ustart, new_ulen, original_ulen,
                                 vseq, vstart, idx, new_left, new_right,
                                 matchcost, mismatchcost, gap_opening,
                                 gap_extension);
       if (idx + 1 <= vlen)
       {
         Diagcolumn[idx+1].val_R.edge = rpoint.edge;
         Diagcolumn[idx+1].val_D.edge = rpoint.edge;
         Diagcolumn[idx+1].val_I.edge = rpoint.edge;
       }
       if (rpoint.idx == lastrpoint.idx)
         lastrpoint = rpoint;
       break;
     case Affine_I:
       new_left = MAX(left_dist,
                 -((GtWord)cpoint.currentrowindex-(GtWord)ustart));

       new_right = MIN((GtWord)idx-1, right_dist);
       rpoint = evaluateaffineDBcrosspoints(Atabcolumn, Rtabcolumn, Diagcolumn,
                                edge, from_edge, Affine_I,rowoffset, coloffset,
                                useq, ustart, cpoint.currentrowindex-ustart,
                                original_ulen, vseq, vstart, idx-1, new_left,
                                new_right, matchcost, mismatchcost,gap_opening,
                                gap_extension);

       Diagcolumn[idx].val_I.edge = rpoint.edge;
       break;
     default:
       gt_assert(false);
     }
   }

  return lastrpoint;
}

/* calculating alignment in linear space within a specified diagonal band
 * with affine gapcosts */
static void gt_calc_diagonalbandaffinealign(const GtUchar *useq,
                                            GtUword ustart, GtUword ulen,
                                            const GtUchar *vseq,
                                            GtUword vstart, GtUword vlen,
                                            GtWord left_dist,
                                            GtWord right_dist,
                                            GtAlignment *align,
                                            GtUword matchcost,
                                            GtUword mismatchcost,
                                            GtUword gap_opening,
                                            GtUword gap_extension)
{
  AffinealignDPentry *Atabcolumn;
  Rtabentry *Rtabcolumn;
  AffineDiagentry *Diagcolumn;
  Rnode lastnode;
  GtUword idx;
  gt_assert(align != NULL);

  if ((left_dist > MIN(0, (GtWord)vlen-(GtWord)ulen))||
      (right_dist < MAX(0, (GtWord)vlen-(GtWord)ulen)))
  {
    fprintf(stderr,"ERROR: invalid diagonalband for global alignment "
                   "(ulen: "GT_WU", vlen: "GT_WU"):\n"
                   "left_dist <= MIN(0, vlen-ulen) and "
                   "right_dist <= MAX(0, vlen-ulen)\n",ulen, vlen);
    exit(GT_EXIT_PROGRAMMING_ERROR);
  }

  if (ulen == 0UL)
  {
    (void) construct_trivial_insertion_alignment(align, vlen, gap_extension);
    return;
  }
  else if (vlen == 0UL)
  {
    (void) construct_trivial_deletion_alignment(align,vlen, gap_extension);
    return;
  }
  else if (ulen == 1UL || vlen == 1UL )
  {
    (void) diagonalbandalignment_in_square_space_affine(align, useq, ustart,
                           ulen, vseq, vstart, vlen, left_dist, right_dist,
                           matchcost, mismatchcost, gap_opening,gap_extension);
    return;
  }

  Diagcolumn = gt_malloc(sizeof *Diagcolumn * (vlen+1));
  Atabcolumn = gt_malloc(sizeof *Atabcolumn * (ulen+1));
  Rtabcolumn = gt_malloc(sizeof *Rtabcolumn * (ulen+1));

  /* initialize Diagcolumn */
  for (idx = 0; idx <= vlen; idx++)
  {
    Diagcolumn[idx].val_R = (Diagentry) {GT_UWORD_MAX, GT_UWORD_MAX, Affine_X};
    Diagcolumn[idx].val_D = (Diagentry) {GT_UWORD_MAX, GT_UWORD_MAX, Affine_X};
    Diagcolumn[idx].val_I = (Diagentry) {GT_UWORD_MAX, GT_UWORD_MAX, Affine_X};
  }

  lastnode = evaluateaffineDBcrosspoints(
                            Atabcolumn, Rtabcolumn, Diagcolumn, Affine_X,
                            Affine_X, Affine_X, 0, 0, useq, ustart, ulen, ulen,
                            vseq, vstart, vlen, left_dist, right_dist,
                            matchcost, mismatchcost, gap_opening,gap_extension);
  /* reconstruct alignment */
  reconstructalignment_from_affineDtab(align, Diagcolumn, lastnode.edge,
                                       useq, ulen, vseq, vlen);

  gt_free(Diagcolumn);
  gt_free(Atabcolumn);
  gt_free(Rtabcolumn);
}

/* compute alignment with affine gapcosts within a diagonal band */
void gt_computediagonalbandaffinealign(GtAlignment *align,
                                       const GtUchar *useq,
                                       GtUword ustart, GtUword ulen,
                                       const GtUchar *vseq,
                                       GtUword vstart, GtUword vlen,
                                       GtWord left_dist,
                                       GtWord right_dist,
                                       GtUword matchcost,
                                       GtUword mismatchcost,
                                       GtUword gap_opening,
                                       GtUword gap_extension)
{

  gt_assert(useq  && vseq);

  /* set new bounds, if left_dist or right_dist is out of sequence */
  left_dist = MAX(-(GtWord) ulen,left_dist);
  right_dist = MIN((GtWord) vlen,right_dist);

  gt_alignment_set_seqs(align, useq+ustart, ulen, vseq+vstart, vlen);
  gt_calc_diagonalbandaffinealign(useq, ustart, ulen, vseq, vstart, vlen,
                                  left_dist, right_dist, align, matchcost,
                                  mismatchcost, gap_opening, gap_extension);
}

void gt_checkdiagonalbandaffinealign(GT_UNUSED bool forward,
                                     const GtUchar *useq, GtUword ulen,
                                     const GtUchar *vseq, GtUword vlen)
{
  GtUword affine_cost1, affine_cost2, affine_cost3,
          matchcost = 0, mismatchcost = 1,
          gap_opening = 2, gap_extension = 1;
  GtWord left_dist, right_dist;
  GtAlignment *align_linear;

  if (memchr(useq, LINEAR_EDIST_GAP,ulen) != NULL)
  {
    fprintf(stderr,"%s: sequence u contains gap symbol\n",__func__);
    exit(GT_EXIT_PROGRAMMING_ERROR);
  }
    if (memchr(vseq, LINEAR_EDIST_GAP,vlen) != NULL)
  {
    fprintf(stderr,"%s: sequence v contains gap symbol\n",__func__);
    exit(GT_EXIT_PROGRAMMING_ERROR);
  }

  /* set left and right to set diagonalband to the whole matrix */
  left_dist = -ulen;
  right_dist = vlen;
  affine_cost1 = diagonalband_square_space_affine(useq, 0, ulen, vseq, 0, vlen,
                                                  left_dist, right_dist,
                                                  matchcost, mismatchcost,
                                                  gap_opening, gap_extension);

  align_linear = gt_alignment_new_with_seqs(useq, ulen, vseq, vlen);
  gt_calc_diagonalbandaffinealign(useq, 0, ulen, vseq, 0, vlen,
                                            left_dist, right_dist,align_linear,
                                            matchcost, mismatchcost,
                                            gap_opening, gap_extension);

  affine_cost2 = gt_alignment_eval_generic_with_affine_score(false,
                                                     align_linear, matchcost,
                                                     mismatchcost, gap_opening,
                                                     gap_extension);
  if (affine_cost1 != affine_cost2)
  {
    fprintf(stderr,"diagonalband_squarespace_affine = "GT_WU
            " != "GT_WU" = gt_calc_diagonalbandaffinealign\n",
            affine_cost1, affine_cost2);

    exit(GT_EXIT_PROGRAMMING_ERROR);
  }

  affine_cost3 = diagonalband_linear_affine(useq, 0, ulen, vseq, 0, vlen,
                                            left_dist, right_dist,
                                            matchcost, mismatchcost,
                                            gap_opening, gap_extension);
  if (affine_cost3 != affine_cost2)
  {
    fprintf(stderr,"diagonalband_linear_affine = "GT_WU
            " != "GT_WU" = gt_calc_diagonalbandaffinealign\n",
            affine_cost3, affine_cost2);

    exit(GT_EXIT_PROGRAMMING_ERROR);
  }
  gt_alignment_delete(align_linear);
}
