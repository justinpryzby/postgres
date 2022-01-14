/*-------------------------------------------------------------------------
 *
 * directmgr.c
 *	  routines for managing unbuffered IO
 *
 * Portions Copyright (c) 1996-2021, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/direct/directmgr.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"


#include "access/xlog.h"
#include "access/xloginsert.h"
#include "storage/directmgr.h"

// TODO: do_optimization can be derived from request_fsync and fsync_self, I
// think. but is that true in all cases and also is it confusing?
void
unbuffered_prep(UnBufferedWriteState *wstate, bool do_optimization, bool
		fsync_self, bool request_fsync)
{
	/*
	 * No reason to do optimization when not required to fsync self
	 */
	Assert(!do_optimization || (do_optimization && fsync_self));

	wstate->do_optimization = do_optimization;
	wstate->fsync_self = fsync_self;
	wstate->request_fsync = request_fsync;

	wstate->redo = do_optimization ? GetRedoRecPtr() : InvalidXLogRecPtr;
}

void
unbuffered_write(UnBufferedWriteState *wstate, bool do_wal, SMgrRelation
		smgrrel, ForkNumber forknum, BlockNumber blocknum, Page page)
{
	PageSetChecksumInplace(page, blocknum);

	smgrwrite(smgrrel, forknum, blocknum, (char *) page,
			!wstate->request_fsync);

	if (do_wal)
		log_newpage(&(smgrrel)->smgr_rnode.node, forknum,
					blocknum, page, true);
}

void
unbuffered_extend(UnBufferedWriteState *wstate, bool do_wal, SMgrRelation
		smgrrel, ForkNumber forknum, BlockNumber blocknum, Page page, bool
		empty)
{
	/*
	 * Don't checksum empty pages
	 */
	if (!empty)
		PageSetChecksumInplace(page, blocknum);

	smgrextend(smgrrel, forknum, blocknum, (char *) page,
			!wstate->request_fsync);

	if (do_wal)
		log_newpage(&(smgrrel)->smgr_rnode.node, forknum,
					blocknum, page, true);

}

/*
 * When writing data outside shared buffers, a concurrent CHECKPOINT can move
 * the redo pointer past our WAL entries and won't flush our data to disk. If
 * the database crashes before the data makes it to disk, our WAL won't be
 * replayed and the data will be lost.
 * Thus, if a CHECKPOINT begins between unbuffered_prep() and
 * unbuffered_finish(), the backend must fsync the data itself.
 */
void
unbuffered_finish(UnBufferedWriteState *wstate, SMgrRelation smgrrel,
		ForkNumber forknum)
{
	if (!wstate->fsync_self)
		return;

	if (wstate->do_optimization && !RedoRecPtrChanged(wstate->redo))
		return;

	elog(DEBUG1, "backend fsync");
	smgrimmedsync(smgrrel, forknum);
}
