/*-------------------------------------------------------------------------
 *
 * cmsketchfuncs.h
 *
 * Copyright (c) 2013-2015, PipelineDB
 *
 *	  Interface for Count-Min Sketch functions
 *
 *-------------------------------------------------------------------------
 */
#ifndef CMSKETCHFUNCS_H
#define CMSKETCHFUNCS_H

#include "postgres.h"
#include "fmgr.h"

extern Datum cmsketch_print(PG_FUNCTION_ARGS);
extern Datum cmsketch_agg_trans(PG_FUNCTION_ARGS);
extern Datum cmsketch_agg_transp(PG_FUNCTION_ARGS);
extern Datum cmsketch_merge_agg_trans(PG_FUNCTION_ARGS);
extern Datum cmsketch_count(PG_FUNCTION_ARGS);
extern Datum cmsketch_empty(PG_FUNCTION_ARGS);
extern Datum cmsketch_emptyp(PG_FUNCTION_ARGS);
extern Datum cmsketch_add(PG_FUNCTION_ARGS);

#endif
