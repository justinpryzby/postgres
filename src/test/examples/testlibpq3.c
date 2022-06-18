/*
 * src/test/examples/testlibpq3.c
 *
 *
 * testlibpq3.c
 *		Test out-of-line parameters and binary I/O.
 *
 * Before running this, populate a database with the following commands
 * (provided in src/test/examples/testlibpq3.sql):
 *
 * CREATE SCHEMA testlibpq3;
 * SET search_path = testlibpq3;
 * SET standard_conforming_strings = ON;
 * CREATE SCHEMA testlibpq3;
 * SET search_path = testlibpq3;
 * SET standard_conforming_strings = ON;
 * CREATE TABLE test1 (
 *     i int4
 *   , r real
 *   , bo boolean
 *   , ts timestamp
 *   , t text
 *   , b bytea
 * );
 * INSERT INTO test1
 * VALUES (
 *     1
 *   , 3.141593
 *   , true
 *   , '2000-01-01 00:00:02.414213'
 *   , 'joe''s place'
 *   , '\000\001\002\003\004'
 * );
 * INSERT INTO test1
 * VALUES (
 *     2
 *   , 1.618033
 *   , false
 *   , '2000-01-01 00:00:01.465571'
 *   , 'ho there'
 *   , '\004\003\002\001\000'
 * );
 *
 * The expected output is:
 *
 * tuple 0: got
 *	i = (4 bytes) 1
 *	r = (4 bytes) 3.141593
 *	bo = (1 bytes) 1
 *	t = (11 bytes) 'joe's place'
 *	b = (5 bytes) \000\001\002\003\004
 *
 * tuple 0: got
 *	i = (4 bytes) 2
 *	r = (4 bytes) 1.618033
 *	bo = (1 bytes) 0
 *	t = (8 bytes) 'ho there'
 *	b = (5 bytes) \004\003\002\001\000
 *
 * General notes about this example:
 *
 * Use PQfnumber to avoid assumptions about field order in result but when
 * getting the field values we ignore possibility they are null!
 */

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "libpq-fe.h"

/* for ntohl/htonl */
#include <netinet/in.h>
#include <arpa/inet.h>


/* These macros hopefully make reading calculations for timestamps easier. */
#define POSTGRES_EPOCH_JDATE 2451545 /* == date2j(2000, 1, 1) */
#define UNIX_EPOCH_JDATE 2440588 /* == date2j(1970, 1, 1) */
#define SECS_PER_DAY 86400

uint64_t ntohll(uint64_t);

static void
exit_nicely(PGconn *conn)
{
	PQfinish(conn);
	exit(1);
}

static void
handle_boolean(PGresult *res, int i)
{
	int			fnum;
	char	   *ptr;
	int			val;

	fnum = PQfnumber(res, "bo");
	ptr = PQgetvalue(res, i, fnum);
	val = (int) *ptr;
	printf(" bo = (%d bytes) %d\n", PQgetlength(res, i, fnum), val);
}

static void
handle_bytea(PGresult *res, int i)
{
	int			j;
	int			fnum;
	char	   *ptr;
	int			len;

	fnum = PQfnumber(res, "b");
	ptr = PQgetvalue(res, i, fnum);

	/*
	 * The binary representation of BYTEA is a bunch of bytes, which could
	 * include embedded nulls so we have to pay attention to field length.
	 */
	len = PQgetlength(res, i, fnum);
	printf(" b = (%d bytes) ", len);
	for (j = 0; j < len; j++) printf("\\%03o", ptr[j]);
}

static void
handle_integer(PGresult *res, int i)
{
	int			fnum;
	char	   *ptr;
	int			val;

	fnum = PQfnumber(res, "i");
	ptr = PQgetvalue(res, i, fnum);

	/*
	 * The binary representation of INT4 is in network byte order, which
	 * we'd better coerce to the local byte order.
	 */
	val = ntohl(*((uint32_t *) ptr));

	printf(" i = (%d bytes) %d\n", PQgetlength(res, i, fnum), val);
}

static void
handle_real(PGresult *res, int i)
{
	int			fnum;
	char	   *ptr;
	union {
		int		i;
		float	f;
	}			val;

	fnum = PQfnumber(res, "r");
	ptr = PQgetvalue(res, i, fnum);

	/*
	 * The binary representation of INT4 is in network byte order, which
	 * we'd better coerce to the local byte order.
	 */
	val.i = ntohl(*((uint32_t *) ptr));

	printf(" r = (%d bytes) %f\n", PQgetlength(res, i, fnum), val.f);
}

static void
handle_text(PGresult *res, int i)
{
	int			fnum;
	char	   *ptr;

	fnum = PQfnumber(res, "t");
	ptr = PQgetvalue(res, i, fnum);

	/*
	 * The binary representation of TEXT is, well, text, and since libpq was
	 * nice enough to append a zero byte to it, it'll work just fine as a C
	 * string.
	 */
	printf(" t = (%d bytes) '%s'\n", PQgetlength(res, i, fnum), ptr);
}

static void
handle_timestamp(PGresult *res, int i)
{
	int			fnum;
	char	   *ptr;
	uint64_t	val;

	struct tm  *tm;
	time_t		timep;
	uint32_t	mantissa;

	fnum = PQfnumber(res, "ts");
	ptr = PQgetvalue(res, i, fnum);
	val = ntohll(*((uint64_t *) ptr));

	/*
	 * The binary representation of a timestamp is in microseconds
	 * from 2000-01-01.
	 */
	timep = val / (uint64_t) 1000000 +
			(uint64_t) (POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) *
			(uint64_t) SECS_PER_DAY;
	mantissa = val - (uint64_t) (timep -
			(POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY) *
			(uint64_t) 1000000;

	/* For ease of testing, assume and print timestamps in GMT. */
	tm = gmtime(&timep);

	printf(" ts = (%d bytes) %04d-%02d-%02d %02d:%02d:%02d.%06d\n",
			PQgetlength(res, i, fnum), tm->tm_year + 1900, tm->tm_mon + 1,
			tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, mantissa);
}

/* This is a uint64_t version of ntohl from arpa/inet.h. */
uint64_t
ntohll(uint64_t netlonglong)
{
	if (ntohl(1) == 1)
		return netlonglong;
	else
		return (uint64_t) (ntohl((int) ((netlonglong << 32) >> 32))) << 32 |
				(uint64_t) ntohl(((int) (netlonglong >> 32)));
}

/*
 * This function prints a query result that is a binary-format fetch from
 * a table defined as in the comment above.  We split it out because the
 * main() function uses it twice.
 */
static void
show_binary_results(PGresult *res)
{
	int			i;

	for (i = 0; i < PQntuples(res); i++)
	{
		printf("tuple %d: got\n", i);
		handle_integer(res, i);
		handle_real(res, i);
		handle_boolean(res, i);
		handle_timestamp(res, i);
		handle_text(res, i);
		handle_bytea(res, i);
		printf("\n\n");
	}
}

int
main(int argc, char **argv)
{
	const char *conninfo;
	PGconn	   *conn;
	PGresult   *res;
	const char *paramValues[1];
	int			paramLengths[1];
	int			paramFormats[1];
	uint32_t	binaryIntVal;

	/*
	 * If the user supplies a parameter on the command line, use it as the
	 * conninfo string; otherwise default to setting dbname=postgres and using
	 * environment variables or defaults for all other connection parameters.
	 */
	if (argc > 1)
		conninfo = argv[1];
	else
		conninfo = "dbname = postgres";

	/* Make a connection to the database */
	conn = PQconnectdb(conninfo);

	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "%s", PQerrorMessage(conn));
		exit_nicely(conn);
	}

	/* Set always-secure search path, so malicious users can't take control. */
	res = PQexec(conn, "SET search_path = testlibpq3");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
		PQclear(res);
		exit_nicely(conn);
	}
	PQclear(res);

	/*
	 * The point of this program is to illustrate use of PQexecParams() with
	 * out-of-line parameters, as well as binary transmission of data.
	 *
	 * This first example transmits the parameters as text, but receives the
	 * results in binary format.  By using out-of-line parameters we can avoid
	 * a lot of tedious mucking about with quoting and escaping, even though
	 * the data is text.  Notice how we don't have to do anything special with
	 * the quote mark in the parameter value.
	 */

	/* Here is our out-of-line parameter value */
	paramValues[0] = "joe's place";

	res = PQexecParams(conn,
					   "SELECT * FROM test1 WHERE t = $1",
					   1,		/* one param */
					   NULL,	/* let the backend deduce param type */
					   paramValues,
					   NULL,	/* don't need param lengths since text */
					   NULL,	/* default to all text params */
					   1);		/* ask for binary results */

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
		PQclear(res);
		exit_nicely(conn);
	}

	show_binary_results(res);

	PQclear(res);

	/*
	 * In this second example we transmit an integer parameter in binary form,
	 * and again retrieve the results in binary form.
	 *
	 * Although we tell PQexecParams we are letting the backend deduce
	 * parameter type, we really force the decision by casting the parameter
	 * symbol in the query text.  This is a good safety measure when sending
	 * binary parameters.
	 */

	/* Convert integer value "2" to network byte order */
	binaryIntVal = htonl((uint32_t) 2);

	/* Set up parameter arrays for PQexecParams */
	paramValues[0] = (char *) &binaryIntVal;
	paramLengths[0] = sizeof(binaryIntVal);
	paramFormats[0] = 1;		/* binary */

	res = PQexecParams(conn,
					   "SELECT * FROM test1 WHERE i = $1::int4",
					   1,		/* one param */
					   NULL,	/* let the backend deduce param type */
					   paramValues,
					   paramLengths,
					   paramFormats,
					   1);		/* ask for binary results */

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
		PQclear(res);
		exit_nicely(conn);
	}

	show_binary_results(res);

	PQclear(res);

	/* close the connection to the database and cleanup */
	PQfinish(conn);

	return 0;
}
