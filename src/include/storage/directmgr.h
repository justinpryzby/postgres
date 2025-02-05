/*-------------------------------------------------------------------------
 *
 * directmgr.h
 *	  POSTGRES unbuffered IO manager definitions.
 *
 *
 * Portions Copyright (c) 1996-2021, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/storage/directmgr.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef DIRECTMGR_H
#define DIRECTMGR_H

#include "access/xlogdefs.h"
#include "common/relpath.h"
#include "storage/block.h"
#include "storage/bufpage.h"
#include "storage/smgr.h"

/*
 * After committing the pg_buffer_stats patch, this will contain a pointer to a
 * PgBufferAccess struct to count the writes and extends done in this way.
 */
typedef struct UnBufferedWriteState
{
	/*
	 * When writing logged table data outside of shared buffers, there is a
	 * risk of a concurrent CHECKPOINT moving the redo pointer past the data's
	 * associated WAL entries. To avoid this, callers in this situation must
	 * fsync the pages they have written themselves.
	 *
	 * These callers can optionally use the following optimization:
	 * attempt to use the sync request queue and fall back to fsync'ing the
	 * pages themselves if the redo pointer moves between the start and finish
	 * of their write. In order to do this, they must set do_optimization to
	 * true so that the redo pointer is saved before the write begins.
	 *
	 * Callers able to use the checkpointer's sync request queue when writing
	 * data outside shared buffers (like fsm and vm) can set request_fsync to
	 * true so that these fsync requests are added to the queue.
	 */
	bool do_optimization;
	bool fsync_self;
	bool request_fsync;
	XLogRecPtr redo;
} UnBufferedWriteState;
/*
 * prototypes for functions in directmgr.c
 */
extern void
unbuffered_prep(UnBufferedWriteState *wstate, bool do_optimization, bool
		fsync_self, bool request_fsync);
extern void
unbuffered_write(UnBufferedWriteState *wstate, bool do_wal, SMgrRelation
		smgrrel, ForkNumber forknum, BlockNumber blocknum, Page page);
extern void
unbuffered_extend(UnBufferedWriteState *wstate, bool do_wal, SMgrRelation smgrrel,
		ForkNumber forknum, BlockNumber blocknum, Page page, bool empty);
extern void
unbuffered_finish(UnBufferedWriteState *wstate, SMgrRelation smgrrel,
		ForkNumber forknum);

#endif							/* DIRECTMGR_H */
