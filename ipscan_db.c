//    ipscan - an http-initiated IPv6 port scanner.
//
//    Copyright (C) 2011 Tim Chappell.
//
//    This file is part of ipscan.
//
//    ipscan is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with ipscan.  If not, see <http://www.gnu.org/licenses/>.

// ipscan_db.c version
// 0.01 - initial version
// 0.02 - added MySQL support
// 0.03 - added syslog support
// 0.04 - improved HTML (transition to styles, general compliance)
// 0.05 - addition of ping functionality (doc tidyup only)
// 0.06 - addition of storage for indirect host responses
// 0.07 - fix potential db query-string buffer overflow
// 0.08 - fix potential sscanf buffer overflow
// 0.09 - remove sqlite3 support
// 0.10 - tidy up comparisons and correct debug logging

#include "ipscan.h"
//
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <inttypes.h>


// MySQL Database includes
#include <my_global.h>
#include <mysql.h>

// Logging with syslog requires additional include
#if (LOGMODE == 1)
	#include <syslog.h>
#endif

// String comparison
#include <string.h>
// Directory creation
#include <sys/stat.h>
// Error number handling
#include <errno.h>

// ----------------------------------------------------------------------------------------
//
// Functions to write to the database, creating it first, if required
//
// ----------------------------------------------------------------------------------------

int write_db(uint64_t host_msb, uint64_t host_lsb, uint64_t timestamp, uint64_t session, uint32_t port, int32_t result, char *indirecthost )
{
	// ID HOSTADDRESS DATE TIME SESSIONID PORT RESULT

	// ID                 BIGINT UNSIGNED
	//
	// HOSTADDRESS MSB    BIGINT UNSIGNED
	//	           LSB    BIGINT UNSIGNED
	//
	// DATE-TIME          BIGINT UNSIGNED
	//
	// SESSIONID          BIGINT UNSIGNED
	//
	// PORT               BIGINT UNSIGNED
	//				      [15:0]  16-bits Port number (0-65535)
	//				      [31:16] 16-bits Protocol
	//				      [63:32] 32-bits Reserved (0)
	//
	// RESULT             BIGINT UNSIGNED
	//
	// INDHOST            VARCHAR(INET6_ADDRSTRLEN+1)

	int rc;
	unsigned int qrylen;
	int retval = -1;
	char query[MAXDBQUERYSIZE];
	MYSQL *connection;
	MYSQL *mysqlrc;

	connection = mysql_init(NULL);
	if (NULL == connection)
	{
		IPSCAN_LOG( LOGPREFIX "write_db: Failed to initialise MySQL\n");
		retval = 1;
	}
	else
	{
		// By using mysql_options() the MySQL library reads the [client] and [ipscan] sections
		// in the my.cnf file which ensures that your program works, even if someone has set
		// up MySQL in some nonstandard way.
		rc = mysql_options(connection, MYSQL_READ_DEFAULT_GROUP,"ipscan");
		if (0 == rc)
		{

			mysqlrc = mysql_real_connect(connection, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWD, MYSQL_DBNAME, 0, NULL, 0);
			if (NULL == mysqlrc)
			{
				IPSCAN_LOG( LOGPREFIX "write_db: Failed to connect to MySQL database (%s) : %s\n", MYSQL_DBNAME, mysql_error(connection));
				IPSCAN_LOG( LOGPREFIX "write_db: HOST %s, USER %s, PASSWD %s\n", MYSQL_HOST, MYSQL_USER, MYSQL_PASSWD);
				retval = 3;
			}
			else
			{
				qrylen = snprintf(query, MAXDBQUERYSIZE, "CREATE TABLE IF NOT EXISTS %s(id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, hostmsb BIGINT UNSIGNED , hostlsb BIGINT UNSIGNED, createdate BIGINT UNSIGNED, session BIGINT UNSIGNED, portnum BIGINT UNSIGNED, portresult BIGINT UNSIGNED, indhost VARCHAR(%d), PRIMARY KEY (id) )",MYSQL_TBLNAME, (INET6_ADDRSTRLEN+1) );
				if (qrylen > 0 && qrylen < MAXDBQUERYSIZE)
				{
					#ifdef DBDEBUG
					IPSCAN_LOG( LOGPREFIX "write_db: MySQL Query is : %s\n", query);
					#endif
					rc = mysql_real_query(connection, query, qrylen);
					if (0 == rc)
					{
						qrylen = snprintf(query, MAXDBQUERYSIZE, "INSERT INTO `%s` (hostmsb, hostlsb, createdate, session, portnum, portresult, indhost) VALUES ( '%"PRIu64"', '%"PRIu64"', '%"PRIu64"', '%"PRIu64"', '%u', '%d', '%s' )", MYSQL_TBLNAME, host_msb, host_lsb, timestamp, session, port, result, indirecthost);
						if (qrylen > 0 && qrylen < MAXDBQUERYSIZE)
						{
							#ifdef DBDEBUG
							IPSCAN_LOG( LOGPREFIX "write_db: MySQL Query is : %s\n", query);
							#endif
							rc = mysql_real_query(connection, query, qrylen);
							if (0 == rc)
							{
								retval = 0;
							}
							else
							{
								IPSCAN_LOG( LOGPREFIX "write_db: Failed to execute insert query %d (%s)\n",\
										mysql_errno(connection), mysql_error(connection) );
								retval = 7;
							}
						}
						else
						{
							IPSCAN_LOG( LOGPREFIX "write_db: Failed to create insert query, length returned was %d, max was %d\n", qrylen, MAXDBQUERYSIZE);
							retval = 8;
						}
					}
					else
					{
						IPSCAN_LOG( LOGPREFIX "write_db: Failed to execute create_table query %d (%s)\n",\
								mysql_errno(connection), mysql_error(connection) );
						retval = 6;
					}
				}
				else
				{
					IPSCAN_LOG( LOGPREFIX "write_db: Failed to create create_table query, length returned was %d, max was %d\n", qrylen, MAXDBQUERYSIZE);
					retval = 5;
				}
			}
		}
		else
		{
			IPSCAN_LOG( LOGPREFIX "write_db: mysql_options() failed - check your my.cnf file\n");
			retval = 2;
		}
		// Tidy up
		mysql_commit(connection);
		mysql_close(connection);
	}
#ifdef DBDEBUG
IPSCAN_LOG( LOGPREFIX "write_db: returning with retval = %d\n",retval);
#endif
return (retval);
}


// ----------------------------------------------------------------------------------------
//
// Functions to dump the database
//
// ----------------------------------------------------------------------------------------

int dump_db(uint64_t host_msb, uint64_t host_lsb, uint64_t timestamp, uint64_t session)
{

	int rc;
	int rcport, rcres, rchost;
	int retval = 0;
	unsigned int num_fields;
	unsigned int qrylen;
	int port, res;
	char hostind[INET6_ADDRSTRLEN+1];
	char query[MAXDBQUERYSIZE];
	MYSQL *connection;
	MYSQL *mysqlrc;
	MYSQL_RES *result;
	MYSQL_ROW row;

	connection = mysql_init(NULL);
	if (NULL == connection)
	{
		IPSCAN_LOG( LOGPREFIX "dump_db: Failed to initialise MySQL\n");
		retval = 1;
	}
	else
	{
		// By using mysql_options() the MySQL library reads the [client] and [ipscan] sections
		// in the my.cnf file which ensures that your program works, even if someone has set
		// up MySQL in some nonstandard way.
		rc = mysql_options(connection, MYSQL_READ_DEFAULT_GROUP,"ipscan");
		if (0 == rc)
		{

			mysqlrc = mysql_real_connect(connection, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWD, MYSQL_DBNAME, 0, NULL, 0);
			if (NULL == mysqlrc)
			{
				IPSCAN_LOG( LOGPREFIX "dump_db: Failed to connect to MySQL database (%s) : %s\n", MYSQL_DBNAME, mysql_error(connection) );
				IPSCAN_LOG( LOGPREFIX "dump_db: HOST %s, USER %s, PASSWD %s\n", MYSQL_HOST, MYSQL_USER, MYSQL_PASSWD);
				retval = 3;
			}
			else
			{
				// int write_db(uint64_t host_msb, uint64_t host_lsb, uint64_t timestamp, uint64_t session )
				// SELECT x FROM t1 WHERE a = b ORDER BY x;
				qrylen = snprintf(query, MAXDBQUERYSIZE, "SELECT * FROM `%s` WHERE ( hostmsb = '%"PRIu64"' AND hostlsb = '%"PRIu64"' AND createdate = '%"PRIu64"' AND session = '%"PRIu64"') ORDER BY id", MYSQL_TBLNAME, host_msb, host_lsb, timestamp, session);
				if (qrylen > 0 && qrylen < MAXDBQUERYSIZE)
				{

					#ifdef DBDEBUG
					IPSCAN_LOG( LOGPREFIX "dump_db: MySQL Query is : %s\n", query);
					#endif
					rc = mysql_real_query(connection, query, qrylen);
					if (0 == rc)
					{
						result = mysql_store_result(connection);
						if (result)
						{
							num_fields = mysql_num_fields(result);
							#ifdef DBDEBUG
							IPSCAN_LOG( LOGPREFIX "dump_db: MySQL returned num_fields : %d\n", num_fields);
							#endif
							printf("[ ");

							while ((row = mysql_fetch_row(result)))
							{
								if (num_fields == 8) // database includes indirect host field
								{
									rcport = sscanf(row[5], "%d", &port);
									rcres = sscanf(row[6], "%d", &res);
									rchost = sscanf(row[7], "%"TO_STR(INET6_ADDRSTRLEN)"s", &hostind[0]);
									if ( rcres == 1 && rchost == 1 && rcport == 1 && port == (0 + IPSCAN_PROTO_ICMPV6) )
									{
										printf("\"%s\", %d, ", hostind, res);
									}
									else if (rcres == 1 && rcport == 1 && (port != (0 + IPSCAN_PROTO_ICMPV6)) )
									{
										printf("%d, ", res);
									}
									else
									{
										IPSCAN_LOG( LOGPREFIX "dump_db: Unexpected row scan results - rcport = %d, rcres = %d, rchost = %d, port = %d\n", rcport, rcres, rchost, port);
									}
								}
								else // original approach
								{
									printf("%s, ", row[num_fields-1]);
									IPSCAN_LOG( LOGPREFIX "ERROR - you NEED to update to the new database format - please see the README for details!\n");
								}
							}
							printf(" -9999 ]\n");
							mysql_free_result(result);
						}
						else
						{
							// Didn't get any results, so check if we should have got some
							if (mysql_field_count(connection) == 0)
							{
								IPSCAN_LOG( LOGPREFIX "dump_db: surprisingly mysql_field_count() expected to return 0 fields\n");
							}
							else
							{
								IPSCAN_LOG( LOGPREFIX "dump_db: mysql_store_result() error : %s\n", mysql_error(connection));
								retval = 10;
							}
						}
					}
					else
					{
						IPSCAN_LOG( LOGPREFIX "dump_db: Failed to execute select query\n");
						retval = 5;
					}
				}
				else
				{
					IPSCAN_LOG( LOGPREFIX "dump_db: Failed to create select query\n");
					retval = 4;
				}
			}
			mysql_commit(connection);
			mysql_close(connection);
		}
		else
		{
			IPSCAN_LOG( LOGPREFIX "dump_db: mysql_options() failed - check your my.cnf file\n");
			retval = 9;
		}
	}
return (retval);
}

// ----------------------------------------------------------------------------------------
//
// Functions to summarise the database
//
// ----------------------------------------------------------------------------------------


int summarise_db(void)
{


	int i,rc;
	unsigned int qrylen;
	int retval = 0;
	char query[MAXDBQUERYSIZE];
	MYSQL *connection;
	MYSQL *mysqlrc;
	MYSQL_RES *result;
	MYSQL_ROW row;

	uint64_t value, hostmsb, hostlsb ;
	time_t createdate;
	unsigned char remotehost[sizeof(struct in6_addr)];
	char hostname[INET6_ADDRSTRLEN];

	connection = mysql_init(NULL);
	if (NULL == connection)
	{
		IPSCAN_LOG( LOGPREFIX "summarise_db: Failed to initialise MySQL\n");
		retval = 1;
	}
	else
	{
		// By using mysql_options() the MySQL library reads the [client] and [ipscan] sections
		// in the my.cnf file which ensures that your program works, even if someone has set
		// up MySQL in some nonstandard way.
		rc = mysql_options(connection, MYSQL_READ_DEFAULT_GROUP,"ipscan");
		if (0 == rc)
		{

			mysqlrc = mysql_real_connect(connection, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWD, MYSQL_DBNAME, 0, NULL, 0);
			if (NULL == mysqlrc)
			{
				IPSCAN_LOG( LOGPREFIX "summarise_db: Failed to connect to MySQL database (%s) : %s\n", MYSQL_DBNAME, mysql_error(connection) );
				IPSCAN_LOG( LOGPREFIX "summarise_db: HOST %s, USER %s, PASSWD %s\n", MYSQL_HOST, MYSQL_USER, MYSQL_PASSWD);
				retval = 3;
			}
			else
			{

				qrylen = snprintf(query, MAXDBQUERYSIZE, "SELECT hostmsb, hostlsb, createdate FROM `%s` GROUP BY createdate ORDER BY createdate", MYSQL_TBLNAME);
				if (qrylen > 0 && qrylen < MAXDBQUERYSIZE)
				{
					#ifdef DBDEBUG
					IPSCAN_LOG( LOGPREFIX "summarise_db: MySQL Query is : %s\n", query);
					#endif
					rc = mysql_real_query(connection, query, qrylen);
					if (0 == rc)
					{
						result = mysql_store_result(connection);
						if (result)
						{

							printf("<TABLE border=\"1\">\n");
							printf("<TR style=\"text-align:center\"><TD width=\"50%%\">Host scanned:</TD><TD width=\"50%%\">Scan begin time:</TD></TR>\n");

							while ((row = mysql_fetch_row(result)))
							{
								rc=sscanf(row[0],"%"SCNu64, &value);
								if (rc == 1) hostmsb = value; else hostmsb = 0;
								rc=sscanf(row[1],"%"SCNu64, &value);
								if (rc == 1) hostlsb = value; else hostlsb = 0;
								rc=sscanf(row[2],"%"SCNu64, &value);
								if (rc == 1) createdate = (time_t)value; else createdate = 0;

								value = hostmsb;
								for (i=0 ; i<8 ; i++)
								{
									remotehost[7-i] = value & 0xFF;
									value = (value >> 8);
								}
								value = hostlsb;
								for (i=0 ; i<8 ; i++)
								{
									remotehost[15-i] = value & 0xFF;
									value = (value >> 8);
								}

								if (inet_ntop(AF_INET6, &remotehost, hostname, INET6_ADDRSTRLEN) != NULL)
								{
									printf("<TR style=\"text-align:center\">\n");
									printf("<TD width=\"50%%\">%s</TD>", hostname);
									printf("<TD width=\"50%%\">%s</TD>\n", asctime(localtime(&createdate)));
									printf("</TR>\n");
								}
							}

							printf("</TABLE>\n");

							mysql_free_result(result);
						}
						else
						{
							// Didn't get any results, so check if we should have got some
							if (mysql_field_count(connection) == 0)
							{
								IPSCAN_LOG( LOGPREFIX "summarise_db: surprisingly mysql_store_result() expected to return 0 fields\n");
							}
							else
							{
								IPSCAN_LOG( LOGPREFIX "summarise_db: mysql_store_result() error : %s\n", mysql_error(connection));
								retval = 10;
							}
						}
					}
					else
					{
						IPSCAN_LOG( LOGPREFIX "summarise_db: Failed to execute select query\n");
						retval = 5;
					}
				}
				else
				{
					IPSCAN_LOG( LOGPREFIX "summarise_db: Failed to create select query\n");
					retval = 4;
				}

				mysql_commit(connection);
				mysql_close(connection);
			}
		}
		else
		{
			IPSCAN_LOG( LOGPREFIX "summarise_db: mysql_options() failed - check your my.cnf file\n");
			retval = 9;
		}
	}

return (retval);
}
