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

#ifndef SRC_CLIENT_H_
#define SRC_CLIENT_H_

// Minimalist SciDB client API
typedef struct
{
  unsigned long long coordinatorid;
  unsigned long long queryid;
} ShimQueryID;

struct prep
{
  ShimQueryID queryid;
  void *queryresult;
};

#define SHIM_CONNECTION_SUCCESSFUL  0
#define SHIM_ERROR_CANT_CONNECT    -1
#define SHIM_ERROR_AUTHENTICATION  -2

void *scidbconnect(
    const char *host,
    int port,
    const char* username,
    const char* password,
    int isAdmin,
    int* status);

void scidbdisconnect (void *con);

unsigned long long executeQuery (void *con, char *query, int afl, char *err);

void prepare_query (void *, void *, char *, int, char *);

ShimQueryID execute_prepared_query (void *, char *, struct prep *, int, char *, int);

void completeQuery (struct prep* pq, void *con, char *err);

#endif /* SRC_CLIENT_H_ */
