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
#include "access/xlogdefs.h"
#include "common/relpath.h"
#include "storage/bufpage.h"
#include "storage/smgr.h"
#include "utils/relcache.h"

#ifndef DIRECTMGR_H
#define DIRECTMGR_H

/*
 * After committing the pg_buffer_stats patch, this will contain a pointer to a
 * PgBufferAccess struct to count the writes and extends done in this way.
 */ 
typedef struct UnBufferedWriteState
{
	SMgrRelation smgr_rel;
	XLogRecPtr redo;
} UnBufferedWriteState;
/*
 * prototypes for functions in directmgr.c
 */
extern void unbuffered_prep(UnBufferedWriteState *wstate);
extern void unbuffered_finish(UnBufferedWriteState *wstate, ForkNumber forknum);
extern void
unbuffered_extend(UnBufferedWriteState *wstate, ForkNumber forknum, BlockNumber
		blocknum, Page page, bool empty);
extern void
unbuffered_write(UnBufferedWriteState *wstate, ForkNumber forknum, BlockNumber
		blocknum, Page page);

#endif							/* DIRECTMGR_H */
