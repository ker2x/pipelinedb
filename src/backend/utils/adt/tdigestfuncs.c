/*-------------------------------------------------------------------------
 *
 * tdigestfuncs.c
 *		t-digest functions
 *
 * Copyright (c) 2013-2015, PipelineDB
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tdigestfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "pipeline/tdigest.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/datum.h"
#include "utils/tdigestfuncs.h"
#include "utils/typcache.h"

Datum
tdigest_print(PG_FUNCTION_ARGS)
{
	StringInfoData buf;
	TDigest *t = (TDigest *) PG_GETARG_VARLENA_P(0);

	initStringInfo(&buf);
	appendStringInfo(&buf, "{ count = %ld, k = %d, centroids: %d }", t->total_weight, (int) t->compression, t->num_centroids);

	PG_RETURN_TEXT_P(CStringGetTextDatum(buf.data));
}

static TDigest *
tdigest_create(uint32_t k)
{
	if (k < 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("k must be non-zero")));
	return TDigestCreateWithCompression(k);
}

static TDigest *
tdigest_startup(FunctionCallInfo fcinfo, uint32_t k)
{
	TDigest *t;

	if (k)
		t = tdigest_create(k);
	else
		t = TDigestCreate();

	SET_VARSIZE(t, TDigestSize(t));

	return t;
}

/*
 * tdigest_agg transition function -
 * 	adds the given element to the transition tdigest
 */
Datum
tdigest_agg_trans(PG_FUNCTION_ARGS)
{
	MemoryContext old;
	MemoryContext context;
	TDigest *state;
	float8 incoming = PG_GETARG_FLOAT8(1);

	if (!AggCheckCallContext(fcinfo, &context))
		elog(ERROR, "tdigest_agg_trans called in non-aggregate context");

	old = MemoryContextSwitchTo(context);

	if (PG_ARGISNULL(0))
		state = tdigest_startup(fcinfo, 0);
	else
		state = (TDigest *) PG_GETARG_POINTER(0);

	TDigestAdd(state, incoming, 1);

	MemoryContextSwitchTo(old);

	PG_RETURN_POINTER(state);
}

/*
 * tdigest_agg transition function -
 *
 * 	adds the given element to the transition tdigest using the given value for compression
 */
Datum
tdigest_agg_transp(PG_FUNCTION_ARGS)
{
	MemoryContext old;
	MemoryContext context;
	TDigest *state;
	float8 incoming = PG_GETARG_FLOAT8(1);
	uint64_t k = PG_GETARG_INT32(2);

	if (!AggCheckCallContext(fcinfo, &context))
		elog(ERROR, "tdigest_agg_trans called in non-aggregate context");

	old = MemoryContextSwitchTo(context);

	if (PG_ARGISNULL(0))
		state = tdigest_startup(fcinfo, k);
	else
		state = (TDigest *) PG_GETARG_POINTER(0);

	TDigestAdd(state, incoming, 1);

	MemoryContextSwitchTo(old);

	PG_RETURN_POINTER(state);
}

/*
 * tdigest_merge_agg transition function -
 *
 * 	returns the union of the transition state and the given tdigest
 */
Datum
tdigest_merge_agg_trans(PG_FUNCTION_ARGS)
{
	MemoryContext old;
	MemoryContext context;
	TDigest *state;
	TDigest *incoming = (TDigest *) PG_GETARG_VARLENA_P(0);

	if (!AggCheckCallContext(fcinfo, &context))
		elog(ERROR, "tdigest_merge_agg_trans called in non-aggregate context");

	old = MemoryContextSwitchTo(context);

	if (PG_ARGISNULL(0))
		state = TDigestCreateWithCompression(incoming->compression);
	else
		state = (TDigest *) PG_GETARG_POINTER(0);

	TDigestMerge(state, incoming);

	MemoryContextSwitchTo(old);

	PG_RETURN_POINTER(state);
}

/*
 * tdigest_quantile
 */
Datum
tdigest_quantile(PG_FUNCTION_ARGS)
{
	TDigest *t;
	float8 q;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	t = (TDigest *) PG_GETARG_VARLENA_P(0);
	q = PG_GETARG_FLOAT8(1);

	PG_RETURN_FLOAT8(TDigestQuantile(t, q));
}

/*
 * tdigest_cdf
 */
Datum
tdigest_cdf(PG_FUNCTION_ARGS)
{
	TDigest *t;
	float8 x;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	t = (TDigest *) PG_GETARG_VARLENA_P(0);
	x = PG_GETARG_FLOAT8(1);

	PG_RETURN_FLOAT8(TDigestCDF(t, x));
}

Datum
tdigest_empty(PG_FUNCTION_ARGS)
{
	TDigest *t = TDigestCreate();
	SET_VARSIZE(t, TDigestSize(t));
	PG_RETURN_POINTER(t);
}

Datum
tdigest_emptyp(PG_FUNCTION_ARGS)
{
	uint64_t k = PG_GETARG_INT32(0);
	TDigest *t = tdigest_create(k);
	SET_VARSIZE(t, TDigestSize(t));
	PG_RETURN_POINTER(t);
}

Datum
tdigest_add(PG_FUNCTION_ARGS)
{
	TDigest *t = (TDigest *) PG_GETARG_VARLENA_P(0);
	float8 incoming = PG_GETARG_FLOAT8(1);
	TDigestAdd(t, incoming, 1);
	PG_RETURN_POINTER(t);
}
