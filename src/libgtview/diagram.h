/*
  Copyright (c) 2007 Malte Mader <mmader@zbh.uni-hamburg.de>
  Copyright (c) 2007 Sascha Steinbiss, Christin Schaerfer, Malte Mader
  Copyright (c) 2007 Christin Schaerfer <cschaerfer@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/
/**
 * \file diagram.h
 * \author Malte Mader <mmader@zbh.uni-hamburg.de>
 * \author Sascha Steinbiss <ssteinbiss@stud.zbh.uni-hamburg.de>
 * \author Christin Schaerfer <cschaerfer@zbh.uni-hamburg.de>
 */

#ifndef DIAGRAM_H
#define DIAGRAM_H

#include "libgtview/config.h"
#include "libgtview/block.h"
#include "libgtview/feature_index.h"
#include "libgtcore/array.h"
#include "libgtcore/range.h"
#include "libgtcore/hashtable.h"
#include "libgtext/genome_feature.h"
#include "libgtext/genome_feature_type.h"

/* holds a Block with associated type */
typedef struct
{
  GenomeFeatureType gft;
  Block *block;
} BlockTuple;

/* a node in the reverse lookup structure used for collapsing */
typedef struct
{
  GenomeNode *parent;
  Array *blocktuples;
} NodeInfoElement;

/*  The Diagram class */
typedef struct Diagram Diagram;

/*!
Initialize a new diagram object.
\param fi FeatureIndex source for feature data.
\param range Desired range of the diagram.
\param seqid Sequence region identifier for the desired region.
\param config Pointer to the configuration object.
\param env Pointer to Environment object.
*/
Diagram*    diagram_new(FeatureIndex *fi,
                        Range range,
                        const char *seqid,
                        Config *config,
                        Env *env);

/*!
Returns the range of a diagram's nodes.
\param diagram Pointer to diagram object.
*/
Range       diagram_get_range(Diagram *diagram);

/*!
Assign a new Config object to a Diagram.
\param diagram pointer to the diagram object.
\param config pointer to the configuration object.
\param env Pointer to Environment object.
*/
void        diagram_set_config(Diagram *diagram,
                               Config *config,
                               Env *env);

/*!
Returns the hashtable with the stored tracks.
\param diagram pointer to the diagram object.
*/
Hashtable*  diagram_get_tracks(Diagram *diagram);

/*!
Returns the total number of lines in the diagram.
\param diagram pointer to the diagram object.
\param env Pointer to Environment object.
*/
int         diagram_get_total_lines(Diagram *diagram,
                                    Env *env);

/*!
Returns the number of all lines in the diagram.
\param diagram pointer to the diagram object.
\param env Pointer to Environment object.
*/
int         diagram_get_number_of_tracks(Diagram *diagram);

/*
Delete the diagram object.
\param diagram pointer to the diagram object.
\param env Pointer to Environment object.
*/
void        diagram_delete(Diagram *diagram,
                           Env* env);

/*
Create a feature index test structure and
test the diagram functions.
\param env Pointer to Environment object.
*/
int         diagram_unit_test(Env* env);

#endif
