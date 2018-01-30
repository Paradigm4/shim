/*
**
* BEGIN_COPYRIGHT
*
* Copyright (C) 2008-2018 Paradigm4, Inc.
*
* shim is free software: you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation version 3 of the License.
*
* This software is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
* INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, NON-INFRINGEMENT, OR
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for the
* complete license terms.
*
* You should have received a copy of the GNU General Public License
* along with shim.  If not, see <http://www.gnu.org/licenses/>.
*
* END_COPYRIGHT
*/

/* Minimal SciDB Client Interface */
#include <string>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <algorithm>

#define MAX_VARLEN 4096

extern "C"
{
#include "client.h"
}

#include "SciDBAPI.h"

using namespace std;
using namespace scidb;


/* Connect to a SciDB instance on the specified host and port.
 * Returns a pointer to the SciDB connection context, or NULL
 * if an error occurred.
 */
extern "C" void * scidbconnect(const char *host, int port, const char* username, const char* password, int* status)
{
  void* conn = NULL;
  scidb::SciDB& db = scidb::getSciDB();
  try
  {
      if(username == NULL || password == NULL)
      {
          conn = db.connect(host, port);
      }
      else
      {
          //TODO: add returned error code for auth failure
          scidb::SessionProperties props(username, password);
          conn = db.connect(props, host, port);
      }
      *status = SHIM_CONNECTION_SUCCESSFUL;
  }
  catch(const scidb::SystemException& se)
  {
      if(se.getLongErrorCode() == scidb::SCIDB_LE_AUTHENTICATION_ERROR)
      {
          *status = SHIM_ERROR_AUTHENTICATION;
      }
      else
      {
          *status = SHIM_ERROR_CANT_CONNECT;
      }
      conn = NULL;
  }
  catch(std::exception& e)
  {
      *status = SHIM_ERROR_CANT_CONNECT;
      conn = NULL;
  }
  return conn;
}

/* Disconnect a connected SciDB client connection,
 * the con pointer should be from the scidbconnect function above.
 * Does not return anything, but we should at least report errors XXX fix.
 */
extern "C" void scidbdisconnect(void * con)
{
  scidb::SciDB& db = scidb::getSciDB();
  try{
    db.disconnect((void *)con);
  } catch(std::exception& e)
  {
  }
}

/* Execute a query, using the indicated SciDB client connection.
 * con SciDB connection context
 * query A query string to execute
 * afl = 0 use AQL otherwise afl
 * err place any character string errors that occur here
 * Returns a SciDB query ID on success, or zero on error (and writes the
 *  text of the error to the character buffer err).
 */
extern "C" unsigned long long
executeQuery(void *con, char *query, int afl, char *err)
{
  unsigned long long id = 0;
  scidb::SciDB& db = scidb::getSciDB();
  const string &queryString = (const char *)query;
  scidb::QueryResult queryResult;
  try{
    db.executeQuery(queryString, bool(afl), queryResult, (void *)con);
    id = (unsigned long long)queryResult.queryID.getId();
  } catch(std::exception& e)
  {
    snprintf(err,MAX_VARLEN,"%s",e.what());
  }
  return id;
}

/* Prepare a query using the indicated SciDB client connection.
 * result: Pointer to a struct prep, filled in by this function on success
 * con: a scidb connection context
 * afl = 0 use AQL, otherwise AFL.
 * err: a buff of length MAX_VARLEN to hold  error string should one occur
 *
 * Upon returning, either the results structure is populated with a non-NULL
 * queryresult pointer and a valid queryid and the err string is NULL, or
 * ther err string is not NULL and the queryresult is NULL.
 */
extern "C" void
prepare_query(void *result, void *con, char *query, int afl, char *err)
{
  struct prep *p;
  unsigned long long id = -1;
  scidb::SciDB& db = scidb::getSciDB();
  const string &queryString = (const char *)query;
  scidb::QueryResult *q = new scidb::QueryResult();
  if(!q)
  {
    snprintf(err,MAX_VARLEN,"Unable to allocate query result\n");
    return;
  }
  p = (struct prep *)result;
  try
  {
    db.prepareQuery(queryString, bool(afl), "", *q, con);
    p->queryresult = (void *)q;
    p->queryid.queryid = (unsigned long long)q->queryID.getId();
    p->queryid.coordinatorid = (unsigned long long)q->queryID.getCoordinatorId();
  } catch(std::exception& e)
  {
    delete q;
    p->queryresult = NULL;
    snprintf(err,MAX_VARLEN,"%s",e.what());
  }
  return;
}

/* Execute a prepared requery stored in the prep structure pq on the scidb
 * connection context con. Set afl to 0 for AQL query, to 1 for AFL query. The
 * char buffer err is a buffer of length MAX_VARLEN on input that will hold an
 * error string on output, should one occur. The queryresult object pointed to
 * from within pq is de-allocated by this function.  Successful exit returns
 * a sQueryID struct.  Failure populates the err buffer with an error string and
 * returns a sQueryID struct with queryid set to 0.
 */
extern "C" ShimQueryID
execute_prepared_query(void *con, char *query, struct prep *pq, int afl, char *err)
{
  ShimQueryID qid;
  const string &queryString = (const char *)query;
  scidb::SciDB& db = scidb::getSciDB();
  qid.queryid = 0;
  qid.coordinatorid = 0;
  scidb::QueryResult *q = (scidb::QueryResult *)pq->queryresult;
  if(!q)
  {
    snprintf(err,MAX_VARLEN,"Invalid query result object.\n");
    return qid;
  }
  try{
    db.executeQuery(queryString, bool(afl), *q, (void *)con);
    qid.queryid = pq->queryid.queryid;
    qid.coordinatorid = pq->queryid.coordinatorid;
  } catch(std::exception& e)
  {
    qid.queryid = 0;
    snprintf(err,MAX_VARLEN,"%s",e.what());
  }
  delete q;
  pq->queryresult = NULL;
  return qid;
}


/* Complete a SciDB query, where char buffer err is a buffer of length
 * MAX_VARLEN on input that will hold an error message should one occur.
 */
extern "C" void completeQuery(ShimQueryID qid, void *con, char *err)
{
  scidb::SciDB& db = scidb::getSciDB();
  scidb::QueryID q = scidb::QueryID(qid.coordinatorid, qid.queryid);
  try{
    db.completeQuery(q, (void *)con);
  } catch(std::exception& e)
  {
    snprintf(err,MAX_VARLEN,"%s",e.what());
  }
}
