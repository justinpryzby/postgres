#include "storage/bufpage.h"
#include "storage/smgr.h"

typedef struct UnBufferedWriteState {
	XLogRecPtr	redo;
	SMgrRelation	smgr_rel;
} UnBufferedWriteState;

void unbuffered_prep(UnBufferedWriteState *wstate);
void unbuffered_finish(UnBufferedWriteState *wstate, ForkNumber forknum);
void unbuffered_write(UnBufferedWriteState *wstate, ForkNumber forknum, BlockNumber
               blocknum, Page page);
void unbuffered_extend(UnBufferedWriteState *wstate, ForkNumber forknum, BlockNumber
               blocknum, Page page, bool empty);
