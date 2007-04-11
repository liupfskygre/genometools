/*
  Copyright (c) 2003-2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2003-2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#ifndef GTEXT_H
#define GTEXT_H

/* The extended GenomeTools library (libgtext) header */
#include <libgtext/addintrons_stream.h>         /* add introns stream */
#include <libgtext/affinealign.h>               /* affine align module */
#include <libgtext/align.h>                     /* align module */
#include <libgtext/alignment.h>                 /* alignment class */
#include <libgtext/bsearch.h>                   /* bsearch module */
#include <libgtext/cds_stream.h>                /* CDS stream */
#include <libgtext/consensus_sa.h>              /* consensus spl. align. mod. */
#include <libgtext/csa_stream.h>                /* consensus spl. align. str. */
#include <libgtext/coin_hmm.h>                  /* the coin HMM class */
#include <libgtext/compare.h>                   /* compare module */
#include <libgtext/dice_hmm.h>                  /* the dice HMM class */
#include <libgtext/extractfeat_stream.h>        /* extract feat. stream class */
#include <libgtext/evaluator.h>                 /* evaluator class */
#include <libgtext/filter_stream.h>             /* filter stream class */
#include <libgtext/gff3_in_stream.h>            /* GFF3 input stream class */
#include <libgtext/gff3_out_stream.h>           /* GFF3 output stream class */
#include <libgtext/gtdata.h>                    /* gtdata/ module */
#include <libgtext/gtf_in_stream.h>             /* GTF input stream class */
#include <libgtext/genome_stream.h>             /* genome stream class */
#include <libgtext/hmm.h>                       /* HMM class */
#include <libgtext/linearalign.h>               /* linear alignment module */
#include <libgtext/linearedist.h>               /* linear edit distance mod. */
#include <libgtext/merge_stream.h>              /* merge stream class */
#include <libgtext/mergefeat_stream_sorted.h>   /* merge feat. stream class */
#include <libgtext/mergefeat_stream_unsorted.h> /* merge feat. stream class */
#include <libgtext/msa.h>                       /* multiple seq. align. class */
#include <libgtext/mutate.h>                    /* mutate module */
#include <libgtext/neighborjoining.h>           /* the Neighbor-Joining class */
#include <libgtext/qgramdist.h>                 /* q-gram distance module */
#include <libgtext/seqid2file.h>                /* seqid2file module */
#include <libgtext/sort_stream.h>               /* sort stream class */
#include <libgtext/splicedseq.h>                /* splicedseq class */
#include <libgtext/stat_visitor.h>              /* status visitor class */
#include <libgtext/stream_evaluator.h>          /* string class */
#include <libgtext/swalign.h>                   /* Smith-Waterman align. mod. */
#include <libgtext/toolbox.h>                   /* toolbox class */
#include <libgtext/upgma.h>                     /* UPGMA class */

#endif
