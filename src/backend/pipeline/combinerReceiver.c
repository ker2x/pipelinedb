/*-------------------------------------------------------------------------
 *
 * combinerReceiver.c
 *	  An implementation of DestReceiver that that allows combiners to receive
 *	  tuples from worker processes.
 *
 * Copyright (c) 2013-2015, PipelineDB
 *
 * IDENTIFICATION
 *	  src/backend/pipeline/combinerReceiver.c
 *
 */
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "postgres.h"
#include "pgstat.h"

#include "access/printtup.h"
#include "pipeline/combinerReceiver.h"
#include "pipeline/tuplebuf.h"
#include "miscadmin.h"
#include "storage/shm_alloc.h"
#include "utils/memutils.h"

typedef struct
{
	DestReceiver pub;
	TupleBufferBatchReader *reader;
	Bitmapset *queries;
	Oid cq_id;
} CombinerState;

static void
combiner_shutdown(DestReceiver *self)
{

}

static void
combiner_startup(DestReceiver *self, int operation,
		TupleDesc typeinfo)
{

}

static void
combiner_receive(TupleTableSlot *slot, DestReceiver *self)
{
	CombinerState *c = (CombinerState *) self;
	MemoryContext old = MemoryContextSwitchTo(ContQueryBatchContext);
	TupleBufferSlot *tbs;
	StreamTuple *tup;
	InsertBatchAck *acks = NULL;
	int num_acks = 0;

	if (synchronous_stream_insert)
	{
		List *acks_list = NIL;
		ListCell *lc_slot;
		ListCell *lc_ack;
		int i = 0;

		foreach(lc_slot, c->reader->yielded)
		{
			TupleBufferSlot *tbs = lfirst(lc_slot);
			int i;

			for (i = 0; i < tbs->tuple->num_acks; i++)
			{
				InsertBatchAck *ack = &tbs->tuple->acks[i];
				bool found = false;

				if (!ShmemDynAddrIsValid(ack->batch) || ack->batch_id != ack->batch->id)
					continue;

				foreach(lc_ack, acks_list)
				{
					InsertBatchAck *ack2 = lfirst(lc_ack);

					if (ack->batch_id == ack2->batch_id)
						found = true;
				}

				if (!found)
				{
					InsertBatchAck *ack2 = (InsertBatchAck *) palloc(sizeof(InsertBatchAck));
					memcpy(ack2, ack, sizeof(InsertBatchAck));
					acks_list = lappend(acks_list, ack2);
				}
			}
		}

		num_acks = list_length(acks_list);

		if (num_acks)
		{
			acks = (InsertBatchAck *) palloc(sizeof(InsertBatchAck) * num_acks);

			foreach(lc_ack, acks_list)
			{
				InsertBatchAck *ack = lfirst(lc_ack);

				InsertBatchIncrementNumCTuples(ack->batch);

				acks[i].batch_id = ack->batch->id;
				acks[i].batch = ack->batch;
				acks[i].count = 1;
				i++;
			}

			list_free_deep(acks_list);
		}
	}

	tup = MakeStreamTuple(ExecMaterializeSlot(slot), NULL, num_acks, acks);
	tbs = TupleBufferInsert(CombinerTupleBuffer, tup, c->queries);

	IncrementCQWrite(1, tbs->size);

	if (num_acks)
		pfree(acks);

	MemoryContextSwitchTo(old);
}

static void combiner_destroy(DestReceiver *self)
{
	CombinerState *c = (CombinerState *) self;
	bms_free(c->queries);
	pfree(c);
}

DestReceiver *
CreateCombinerDestReceiver(void)
{
	CombinerState *self = (CombinerState *) palloc0(sizeof(CombinerState));

	self->pub.receiveSlot = combiner_receive; /* might get changed later */
	self->pub.rStartup = combiner_startup;
	self->pub.rShutdown = combiner_shutdown;
	self->pub.rDestroy = combiner_destroy;
	self->pub.mydest = DestCombiner;

	return (DestReceiver *) self;
}

/*
 * SetCombinerDestReceiverParams
 *
 * Set parameters for a CombinerDestReceiver
 */
void
SetCombinerDestReceiverParams(DestReceiver *self, TupleBufferBatchReader *reader, Oid cq_id)
{
	CombinerState *c = (CombinerState *) self;
	c->reader = reader;
	c->cq_id = cq_id;
	c->queries = bms_add_member(c->queries, cq_id);
}
