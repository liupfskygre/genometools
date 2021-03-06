#ifndef FT_FRONT_PRUNE_H
#define FT_FRONT_PRUNE_H
#include "match/ft-trimstat.h"
#include "match/ft-polish.h"
#include "match/ft-front-generation.h"
#include "core/encseq_api.h"

typedef struct
{
  void *space;
  GtUword offset, allocated;
} GtAllocatedMemory;

typedef enum
{
  GT_OUTSENSE_TRIM_ALWAYS,
  GT_OUTSENSE_TRIM_ON_NEW_PP,
  GT_OUTSENSE_TRIM_NEVER
} GtTrimmingStrategy;

typedef enum
{
  GT_EXTEND_CHAR_ACCESS_ENCSEQ,
  GT_EXTEND_CHAR_ACCESS_ENCSEQ_READER,
  GT_EXTEND_CHAR_ACCESS_DIRECT,
  GT_EXTEND_CHAR_ACCESS_ANY
} GtExtendCharAccess;

typedef struct
{
  const GtEncseq *encseq;
  GtAllocatedMemory *sequence_cache;
  GtEncseqReader *encseq_r;
  const GtUchar *bytesequence;
  GtUword totallength,
          full_totallength;
  GtReadmode readmode;
  GtExtendCharAccess extend_char_access;
  bool twobit_possible, haswildcards;
} GtFTsequenceResources;

GtUword front_prune_edist_inplace(
                       bool forward,
                       GtAllocatedMemory *frontspace_reservoir,
                       GtFtPolished_point *best_polished_point,
                       GtFrontTrace *fronttrace,
                       const GtFtPolishing_info *pol_info,
                       GtTrimmingStrategy trimstrategy,
                       GtUword history,
                       GtUword minmatchnum,
                       GtUword maxalignedlendifference,
                       bool showfrontinfo,
                       GtUword seedlength,
                       GtFTsequenceResources *ufsr,
                       GtUword ustart,
                       GtUword uulen,
                       GtUword vseqstartpos,
                       GtFTsequenceResources *vfsr,
                       GtUword vstart,
                       GtUword vlen,
                       bool cam_generic,
                       GtFtTrimstat *trimstat);

typedef struct GtFullFrontEdistTrace GtFullFrontEdistTrace;

GtFullFrontEdistTrace *gt_full_front_edist_trace_new(void);

void gt_full_front_edist_trace_delete(GtFullFrontEdistTrace *fet);

GtFrontTrace *gt_full_front_trace_get(GtFullFrontEdistTrace *fet);

GtUword gt_full_front_edist_trace_distance(GtFullFrontEdistTrace *fet,
                                           const GtUchar *useq,
                                           GtUword ulen,
                                           const GtUchar *vseq,
                                           GtUword vlen);

#endif
