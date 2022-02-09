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


#include "access/xloginsert.h"
#include "storage/directmgr.h"

void
unbuffered_prep(UnBufferedWriteState *wstate, bool fsync_self, bool
		request_fsync)
{
	wstate->fsync_self = fsync_self;
	wstate->request_fsync = request_fsync;
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

	smgrimmedsync(smgrrel, forknum);
}
