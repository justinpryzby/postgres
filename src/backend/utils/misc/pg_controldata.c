/*-------------------------------------------------------------------------
 *
 * pg_controldata.c
 *
 * Routines to expose the contents of the control data file via
 * a set of SQL functions.
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/pg_controldata.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "catalog/pg_control.h"
#include "common/controldata_utils.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "storage/lwlock.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"
#include "utils/timestamp.h"

Datum
pg_control_system(PG_FUNCTION_ARGS)
{
	Datum		values[4];
	bool		nulls[4];
	TupleDesc	tupdesc;
	HeapTuple	htup;
	ControlFileData *ControlFile;
	bool		crc_ok;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	/* read the control file */
	LWLockAcquire(ControlFileLock, LW_SHARED);
	ControlFile = get_controlfile(DataDir, &crc_ok);
	LWLockRelease(ControlFileLock);
	if (!crc_ok)
		ereport(ERROR,
				(errmsg("calculated CRC checksum does not match value stored in file")));

	memset(nulls, false, sizeof(nulls));
	values[0] = Int32GetDatum(ControlFile->pg_control_version);
	values[1] = Int32GetDatum(ControlFile->catalog_version_no);
	values[2] = Int64GetDatum(ControlFile->system_identifier);
	values[3] = TimestampTzGetDatum(time_t_to_timestamptz(ControlFile->time));

	htup = heap_form_tuple(tupdesc, values, nulls);

	PG_RETURN_DATUM(HeapTupleGetDatum(htup));
}

Datum
pg_control_checkpoint(PG_FUNCTION_ARGS)
{
	Datum		values[18];
	bool		nulls[18];
	TupleDesc	tupdesc;
	HeapTuple	htup;
	ControlFileData *ControlFile;
	XLogSegNo	segno;
	char		xlogfilename[MAXFNAMELEN];
	bool		crc_ok;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	/* Read the control file. */
	LWLockAcquire(ControlFileLock, LW_SHARED);
	ControlFile = get_controlfile(DataDir, &crc_ok);
	LWLockRelease(ControlFileLock);
	if (!crc_ok)
		ereport(ERROR,
				(errmsg("calculated CRC checksum does not match value stored in file")));

	/*
	 * Calculate name of the WAL file containing the latest checkpoint's REDO
	 * start point.
	 */
	XLByteToSeg(ControlFile->checkPointCopy.redo, segno, wal_segment_size);
	XLogFileName(xlogfilename, ControlFile->checkPointCopy.ThisTimeLineID,
				 segno, wal_segment_size);

	/* Populate the values and null arrays */
	memset(nulls, false, sizeof(nulls));
	values[0] = LSNGetDatum(ControlFile->checkPoint);
	values[1] = LSNGetDatum(ControlFile->checkPointCopy.redo);
	values[2] = CStringGetTextDatum(xlogfilename);
	values[3] = Int32GetDatum(ControlFile->checkPointCopy.ThisTimeLineID);
	values[4] = Int32GetDatum(ControlFile->checkPointCopy.PrevTimeLineID);
	values[5] = BoolGetDatum(ControlFile->checkPointCopy.fullPageWrites);
	values[6] = CStringGetTextDatum(psprintf("%u:%u",
											 EpochFromFullTransactionId(ControlFile->checkPointCopy.nextXid),
											 XidFromFullTransactionId(ControlFile->checkPointCopy.nextXid)));
	values[7] = ObjectIdGetDatum(ControlFile->checkPointCopy.nextOid);
	values[8] = TransactionIdGetDatum(ControlFile->checkPointCopy.nextMulti);
	values[9] = TransactionIdGetDatum(ControlFile->checkPointCopy.nextMultiOffset);
	values[10] = TransactionIdGetDatum(ControlFile->checkPointCopy.oldestXid);
	values[11] = ObjectIdGetDatum(ControlFile->checkPointCopy.oldestXidDB);
	values[12] = TransactionIdGetDatum(ControlFile->checkPointCopy.oldestActiveXid);
	values[13] = TransactionIdGetDatum(ControlFile->checkPointCopy.oldestMulti);
	values[14] = ObjectIdGetDatum(ControlFile->checkPointCopy.oldestMultiDB);
	values[15] = TransactionIdGetDatum(ControlFile->checkPointCopy.oldestCommitTsXid);
	values[16] = TransactionIdGetDatum(ControlFile->checkPointCopy.newestCommitTsXid);
	values[17] = TimestampTzGetDatum(time_t_to_timestamptz(ControlFile->checkPointCopy.time));

	htup = heap_form_tuple(tupdesc, values, nulls);

	PG_RETURN_DATUM(HeapTupleGetDatum(htup));
}

Datum
pg_control_recovery(PG_FUNCTION_ARGS)
{
	Datum		values[5];
	bool		nulls[5];
	TupleDesc	tupdesc;
	HeapTuple	htup;
	ControlFileData *ControlFile;
	bool		crc_ok;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	/* read the control file */
	LWLockAcquire(ControlFileLock, LW_SHARED);
	ControlFile = get_controlfile(DataDir, &crc_ok);
	LWLockRelease(ControlFileLock);
	if (!crc_ok)
		ereport(ERROR,
				(errmsg("calculated CRC checksum does not match value stored in file")));

	memset(nulls, false, sizeof(nulls));
	values[0] = LSNGetDatum(ControlFile->minRecoveryPoint);
	values[1] = Int32GetDatum(ControlFile->minRecoveryPointTLI);
	values[2] = LSNGetDatum(ControlFile->backupStartPoint);
	values[3] = LSNGetDatum(ControlFile->backupEndPoint);
	values[4] = BoolGetDatum(ControlFile->backupEndRequired);

	htup = heap_form_tuple(tupdesc, values, nulls);

	PG_RETURN_DATUM(HeapTupleGetDatum(htup));
}

Datum
pg_control_init(PG_FUNCTION_ARGS)
{
	Datum		values[11];
	bool		nulls[11];
	TupleDesc	tupdesc;
	HeapTuple	htup;
	ControlFileData *ControlFile;
	bool		crc_ok;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	/* read the control file */
	LWLockAcquire(ControlFileLock, LW_SHARED);
	ControlFile = get_controlfile(DataDir, &crc_ok);
	LWLockRelease(ControlFileLock);
	if (!crc_ok)
		ereport(ERROR,
				(errmsg("calculated CRC checksum does not match value stored in file")));

	memset(nulls, false, sizeof(nulls));
	values[0] = Int32GetDatum(ControlFile->maxAlign);
	values[1] = Int32GetDatum(ControlFile->blcksz);
	values[2] = Int32GetDatum(ControlFile->relseg_size);
	values[3] = Int32GetDatum(ControlFile->xlog_blcksz);
	values[4] = Int32GetDatum(ControlFile->xlog_seg_size);
	values[5] = Int32GetDatum(ControlFile->nameDataLen);
	values[6] = Int32GetDatum(ControlFile->indexMaxKeys);
	values[7] = Int32GetDatum(ControlFile->toast_max_chunk_size);
	values[8] = Int32GetDatum(ControlFile->loblksize);
	values[9] = BoolGetDatum(ControlFile->float8ByVal);
	values[10] = Int32GetDatum(ControlFile->data_checksum_version);

	htup = heap_form_tuple(tupdesc, values, nulls);

	PG_RETURN_DATUM(HeapTupleGetDatum(htup));
}
