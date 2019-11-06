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

#define _GNU_SOURCE
//#include <stdio.h>
//#include <limits.h>
//#include <inttypes.h>
//#include <stdlib.h>
//#include <string.h>
#include <ctype.h>
#include <libgen.h>
//#include <sys/types.h>
//#include <sys/resource.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <poll.h>
//#include <unistd.h>
#include <time.h>
#include <omp.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>

#include "mongoose.h"
#include "mbedtls/sha512.h"
#include "base64.h"
#include "client.h"
#include "iniparser/src/iniparser.h"

#define DEFAULT_MAX_SESSIONS 50      // Maximum number of concurrent http sessions
#define MAX_VARLEN           4096    // Static buffer length
#define LCSV_MAX             16384
#define SESSIONID_LEN        32 + 1  // Length of a session ID
#define SESSIONID_SHOW_LEN   6       // Session ID prefix to show in the log
#define SAVE_BIN             1       // Saved in binary format
#define SAVE_TXT             2       // Saved in text format
#ifndef PATH_MAX
#define PATH_MAX             4096
#endif
#define MAX_RETURN_BYTES     INT_MAX

#ifndef DEFAULT_HTTP_PORT
#define DEFAULT_HTTP_PORT "8080,8083s"
#endif

#define DEFAULT_SAVE_INSTANCE_ID 0  // default instance that does the saving
#define DEFAULT_TMPDIR "/tmp"       // Temporary location for I/O buffers
#define DEFAULT_PIDFILE             "/var/run/shim.pid"
#define DEFAULT_CONFFILE            "/var/lib/shim/conf"

#define WEEK 604800             // One week in seconds
#define DEFAULT_TIMEOUT 60      // Timeout before a session is declared
                                // orphaned and available to reap (seconds)

// -- - HTTP Status Codes - --
// https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
// -- - HTTP 4xx
#define HTTP_400_BAD_REQUEST     400
#define HTTP_401_UNAUTHORIZED    401
#define HTTP_403_FORBIDDEN       403
#define HTTP_404_NOT_FOUND       404
#define HTTP_406_NOT_ACCEPTABLE  406
#define HTTP_409_CONFLICT        409
#define HTTP_410_GONE            410
#define HTTP_416_NOT_SATISFIABLE 416 // Requested Range Not Satisfiable
// -- - HTTP 5xx
#define HTTP_500_SERVER_ERROR    500 // Internal Server Error
#define HTTP_502_BAD_GATEWAY     502
#define HTTP_503_UNAVAILABLE     503 // Service Unavailable

// -- - HTTP 4xx Error Messages
#define MSG_ERR_HTTP_400_ARG "HTTP arguments missing"
#define MSG_ERR_HTTP_400_EFL "Uploaded file is empty"
#define MSG_ERR_HTTP_401     "SciDB authentication failed"
#define MSG_ERR_HTTP_404     "Session not found"
#define MSG_ERR_HTTP_409     "Session has no query"
#define MSG_ERR_HTTP_410     "Output not saved"
#define MSG_ERR_HTTP_416_BIN "Output not saved in binary format"
#define MSG_ERR_HTTP_416_TXT "Output not saved in text format"
#define MSG_ERR_HTTP_416_EOF "EOF - range out of bounds"
// -- - HTTP 5xx Error Messages
#define MSG_ERR_HTTP_500_OOM "Out of memory"
#define MSG_ERR_HTTP_500_BUF "Open output buffer failed"
#define MSG_ERR_HTTP_500_FST "Get file status failed"
#define MSG_ERR_HTTP_502     "SciDB connection failed"
#define MSG_ERR_HTTP_503     "Out of resources"
// -- -

// SciDB errors which trigger a 5xx Shim error
char *SCIDB_CONNECTION_ERR[] = {
    "SCIDB_LE_CANT_SEND_RECEIVE",
    "SCIDB_LE_CONNECTION_ERROR",
    "SCIDB_LE_NO_QUORUM"};

// characters allowed in session IDs
char SESSIONID_CHARSET[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/* A session consists of client I/O buffers, and an optional SciDB query ID. */
typedef enum
{
  SESSION_UNAVAILABLE,
  SESSION_AVAILABLE
} available_t;
typedef struct
{
  omp_lock_t lock;
  char sessionid[SESSIONID_LEN]; // session identifier
  ShimQueryID qid;               // SciDB query identifier
  int pd;                        // output buffer file descrptor
  FILE *pf;                      //   and FILE pointer
  int save;                      // non-zero if output is to be saved
                                 // set to SAVE_BIN for binary
                                 // set to SAVE_TXT for text
  char *ibuf;                    // input buffer name
  char *obuf;                    // output (file) buffer name ////
  void *scidb[2];                // SciDB context
  time_t time;                   // Time value to help decide on orphan sessions
  available_t available;         // 1 -> available, 0 -> not available
} session;

/*
 * Orphan session detection process
 * Shim limits the number of simultaneous open sessions. Absent-minded or
 * malicious clients must be prevented from opening new sessions repeatedly
 * resulting in denial of service. Shim uses a lazy timeout mechanism to
 * detect unused sessions and reclaim them. It works like this:
 *
 * 1. The session time value is set to the current time when an API event
 *    finishes.
 * 2. If a new_session request fails to find any available session slots,
 *    it inspects the existing session time values for all the sessions,
 *    computing the difference between current time and the time value.
 *    If a session time difference exceeds TIMEOUT, then that session is
 *    cleaned up (cleanup_session), re-initialized, and returned as a
 *    new session. Queries are not canceled though.
 *
 * Operations that are in-flight but may take an indeterminate amount of
 * time, for example PUT file uploads or execute_query statements, set their
 * time value to a point far in the future to protect them from harvesting.
 * Their time values are set to the current time when such operations complete.
 *
 */
enum mimetype
{ html, plain, binary };

char *SCIDB_HOST = "localhost";
int SCIDB_PORT = 1239;
session *sessions;              // Fixed pool of web client sessions
char *docroot;
/* Big common lock used to serialize global operations.
 * Each session also has a separate session lock. All the locks support
 * nesting/recursion.
 */
omp_lock_t biglock;
/* Global variables
 */
char *PIDFILE;                  // pid file
char *CONFFILE;                 // shim configuration file
char *TMPDIR;                   // temporary files go here
char *AS_USER;                  // user name to run shim under
int MAX_SESSIONS;               // configurable maximum number of concurrent sessions
int SAVE_INSTANCE_ID;           // which instance ID should run save commands?
time_t TIMEOUT;                 // session timeout
int USE_AIO;                    // use accelerated io for some saves: 0/1
char *COMMAND;                  // command line directive

/* copy input string to already-allocated output string, omitting incidences
 * of dot characters. output must have allocated size sufficient to hold the
 * result, a copy of input will do since its size will not exceed that.
 */
int
nodots (const char *input, char *output)
{
  size_t k, i = 0;
  for (k = 0; k < strlen (input); ++k)
    {
      if (input[k] != '.')
        {
          output[i] = input[k];
          i++;
        }
    }
  return 0;
}


/*
 * conn: A mongoose client connection
 * type: response mime type using the mimetype enumeration defined above
 * code: HTML integer response code (200=OK)
 * length: length of data buffer
 * data: character buffer to send
 * Write an HTTP response to client connection, it's OK for length=0 and
 * data=NULL.  This routine generates the http header.
 *
 * Response messages are http 1.1 with OK/ERROR header, content length, data.
 * example:
 * HTTP/1.1 <code> OK\r\n
 * Content-Length: <length>\r\n
 * Content-Type: text/html\r\n\r\n
 * <DATA>
 */
void
respond (struct mg_connection *conn,
         const enum mimetype type,
         const int code,
         const size_t length,
         const char *data)
{
  if (code != 200)              // error
    {
      if (data)                 // error with data payload (always presented as text/html here)
        {
          mg_printf (conn, "HTTP/1.1 %d ERROR\r\n"
                     "Content-Length: %lu\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Content-Type: text/html\r\n\r\n", code, strlen (data));
          mg_write (conn, data, strlen (data));
        }
      else                      // error without any payload
        mg_printf (conn,
                   "HTTP/1.1 %d ERROR\r\nCache-Control: no-cache\r\nAccess-Control-Allow-Origin: *\r\n\r\n",
                   code);
      return;
    }
  switch (type)
    {
    case html:
      mg_printf (conn, "HTTP/1.1 200 OK\r\n"
                 "Content-Length: %lu\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Content-Type: text/html\r\n\r\n", length);
      break;
    case plain:
      mg_printf (conn, "HTTP/1.1 200 OK\r\n"
                 "Content-Length: %lu\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Content-Type: text/plain\r\n\r\n", length);
      break;
    case binary:
      mg_printf (conn, "HTTP/1.1 200 OK\r\n"
                 "Content-Length: %lu\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Content-Type: application/octet-stream\r\n\r\n", length);
      break;
    }
  if (data)
    mg_write (conn, data, (size_t) length);
}


// Retrieve a session pointer from an id, return NULL if not found.
session *
find_session (char * id)
{
  int j;
  session *ans = NULL;
  for (j = 0; j < MAX_SESSIONS; ++j)
    {
      if (strcmp(sessions[j].sessionid, id) != 0)
        {
          continue;
        }
      ans = sessions[j].available == SESSION_UNAVAILABLE ? &sessions[j] : NULL;
    }
  return ans;
}


/* Cleanup a shim session and reset it to available.
 * Acquire the session lock before invoking this routine.
 */
void
cleanup_session (session * s)
{
  syslog (LOG_INFO,
          "cleanup_session[%.*s]: releasing",
          SESSIONID_SHOW_LEN,
          s->sessionid);
  s->available = SESSION_AVAILABLE;
  s->qid.queryid = 0;
  s->time = 0;
  if (s->pf != NULL)
    fclose (s->pf);
  if (s->pd > 0)
    close (s->pd);
  if (s->ibuf)
    {
      syslog (LOG_INFO,
              "cleanup_session[%.*s]: unlinking ibuf %p",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              s->ibuf);
      unlink (s->ibuf);
      free (s->ibuf);
      s->ibuf = NULL;
    }
  if (s->obuf)
    {
      syslog (LOG_INFO,
              "cleanup_session[%.*s]: unlinking obuf %p",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              s->obuf);
      unlink (s->obuf);
      free (s->obuf);
      s->obuf = NULL;
    }
  strcpy(s->sessionid, "NA");
}


/* Release a session defined in the client mg_request_info object 'id'
 * variable. Respond to the client connection conn.  Set resp to zero to not
 * respond to client conn, otherwise send http 200 response.
 *
 * Respond to the client connection as follows:
 * 200 success
 * 400 missing arguments
 * 404 session not found
 */
void
release_session (struct mg_connection *conn, const struct mg_request_info *ri,
                 int resp)
{
  int k;
  char ID[SESSIONID_LEN];
  if (!ri->query_string)
    {
      syslog (LOG_ERR, "release_session: ERROR %s", MSG_ERR_HTTP_400_ARG);
      respond (conn,
               plain,
               HTTP_400_BAD_REQUEST,
               strlen (MSG_ERR_HTTP_400_ARG),
               MSG_ERR_HTTP_400_ARG);
      return;
    }
  k = strlen (ri->query_string);
  mg_get_var (ri->query_string, k, "id", ID, SESSIONID_LEN);
  session *s = find_session (ID);
  if (s)
    {
      syslog (LOG_INFO,
              "release_session[%.*s]: disconnecting",
              SESSIONID_SHOW_LEN,
              s->sessionid);
      for (int i = 0; i < 2; i++)
        {
          if (s->scidb[i])
            {
              scidbdisconnect (s->scidb[i]);
              s->scidb[i] = NULL;
            }
        }

      omp_set_lock (&s->lock);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      if (resp)
        respond (conn, plain, 200, 0, NULL);
    }
  else if (resp)
    {
      // not found
      syslog (LOG_INFO, "release_session: ERROR %s", MSG_ERR_HTTP_404);
      respond (conn,
               plain,
               HTTP_404_NOT_FOUND,
               strlen (MSG_ERR_HTTP_404),
               MSG_ERR_HTTP_404);
    }
}


void respond_to_query_error(struct mg_connection *conn,
                            session *session,
                            const char *scidb_error)
{
  int is_critical = 0;
  for (int i = 0;
       i < sizeof(SCIDB_CONNECTION_ERR) / sizeof(SCIDB_CONNECTION_ERR[0]);
       i++)
    if (strstr(scidb_error, SCIDB_CONNECTION_ERR[i]) != NULL)
      is_critical = 1;

  if (is_critical == 1)
    {
      respond (conn,
               plain,
               HTTP_502_BAD_GATEWAY,
               strlen (scidb_error),
               scidb_error);
      cleanup_session(session);
    }
  else
    {
      respond (conn,
               plain,
               HTTP_406_NOT_ACCEPTABLE,
               strlen (scidb_error),
               scidb_error);
    }
}


/* Note: cancel does not trigger a cleanup_session for the session
 * corresponding to the query. The client that initiated the original query is
 * still responsible for session cleanup.
 *
 * A 502 error invalidates and releases the session.
 *
 * Respond to the client connection as follows:
 * 200 success
 * 400 missing arguments
 * 404 session not found
 * 409 session has no query
 * 502 SciDB connection failure
 */
void
cancel (struct mg_connection *conn, const struct mg_request_info *ri)
{
  int k;
  char var1[MAX_VARLEN];
  char ID[SESSIONID_LEN];
  char SERR[MAX_VARLEN];
  if (!ri->query_string)
    {
      syslog (LOG_ERR, "cancel: ERROR %s", MSG_ERR_HTTP_400_ARG);
      respond (conn,
               plain,
               HTTP_400_BAD_REQUEST,
               strlen (MSG_ERR_HTTP_400_ARG),
               MSG_ERR_HTTP_400_ARG);
      return;
    }
  k = strlen (ri->query_string);
  mg_get_var (ri->query_string, k, "id", ID, SESSIONID_LEN);
  session *s = find_session (ID);
  if (s == NULL)
    {
      // not found
      syslog (LOG_INFO, "cancel: ERROR %s", MSG_ERR_HTTP_404);
      respond (conn,
               plain,
               HTTP_404_NOT_FOUND,
               strlen (MSG_ERR_HTTP_404),
               MSG_ERR_HTTP_404);
      return;
    }
  if (s->qid.queryid <= 0)
    {
      syslog (LOG_INFO, "cancel[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_409);
      respond (conn,
               plain,
               HTTP_409_CONFLICT,
               strlen (MSG_ERR_HTTP_409),
               MSG_ERR_HTTP_409);
      return;
    }

  memset (var1, 0, MAX_VARLEN);
  snprintf (var1, MAX_VARLEN, "cancel(\'%llu.%llu\')",
            s->qid.coordinatorid, s->qid.queryid);
  syslog (LOG_INFO,
          "cancel[%.*s]: execute, qid %llu.%llu, scidb[1] %p, query %s",
          SESSIONID_SHOW_LEN,
          s->sessionid,
          s->qid.coordinatorid,
          s->qid.queryid,
          s->scidb[1],
          var1);
  memset (SERR, 0, MAX_VARLEN);

  if (s->scidb[1])
    {
      executeQuery (s->scidb[1], var1, 1, SERR);
    }
  syslog (LOG_INFO,
          "cancel[%.*s]: result %s",
          SESSIONID_SHOW_LEN,
          s->sessionid,
          SERR);
  time (&s->time);
  respond (conn, plain, 200, 0, NULL);
}


/* Generate random and unique session ID.
 */
void
gen_sessionid (char * sessionid)
{
  int duplicate = 1;
  while (duplicate)
    {
      int length = SESSIONID_LEN - 1;
      char * id = sessionid;
      while (length-- > 0)
        {
          size_t index = rand() % (sizeof(SESSIONID_CHARSET) - 1);
          *id++ = SESSIONID_CHARSET[index];
        }
      *id = '\0';

      duplicate = 0;
      for (int i = 0; i < MAX_SESSIONS; ++i)
        {
          if (sessions[i].sessionid != sessionid
              && strcmp(sessions[i].sessionid, sessionid) == 0)
            {
              duplicate = 1;
              break;
            }
        }
    }
}


/* Initialize a session. Obtain the big lock before calling this.
 * returns 1 on success, 0 otherwise.
 */
int
init_session (session * s)
{
  int fd;
  omp_set_lock (&s->lock);
  gen_sessionid(s->sessionid);

  s->ibuf = (char *) malloc (PATH_MAX);
  if (!s->ibuf)
    {
      syslog (LOG_ERR,
              "init_session[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return 0;
    }

  s->obuf = (char *) malloc (PATH_MAX);
  if (!s->obuf)
    {
      syslog (LOG_ERR,
              "init_session[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return 0;
    }

  snprintf (s->ibuf, PATH_MAX, "%s/shim_input_buf_XXXXXX", TMPDIR);
  snprintf (s->obuf, PATH_MAX, "%s/shim_output_buf_XXXXXX", TMPDIR);
  for (int i = 0; i < 2; i++)
    {
      s->scidb[i] = NULL;
    }
// Set up the input buffer
  fd = mkstemp (s->ibuf);
// XXX We need to make it so that whoever runs scidb can R/W to this file.
// Since, in general, we don't know who is running scidb (anybody can), and, in
// general, shim runs as a service we have a mismatch. Right now, we set it so
// that anyone can write to these to work around this. But really, these files
// should be owned by the scidb process user. Hmmm.  (See also below for output
// buffer and pipe.)
  chmod (s->ibuf, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fd > 0)
    close (fd);
  else
    {
      syslog (LOG_ERR,
              "init_session[%.*s]: ERROR input buffer",
              SESSIONID_SHOW_LEN,
              s->sessionid);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return 0;
    }
// Set up the output buffer
  s->pd = 0;
  s->pf = NULL;
  s->save = 0;
  fd = mkstemp (s->obuf);
  chmod (s->obuf, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fd > 0)
    close (fd);
  else
    {
      syslog (LOG_ERR,
              "init_session[%.*s]: ERROR output buffer",
              SESSIONID_SHOW_LEN,
              s->sessionid);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return 0;
    }
  time (&s->time);
  s->available = SESSION_UNAVAILABLE;
  omp_unset_lock (&s->lock);
  return 1;
}


/* Find an available session.  If no sessions are available, return -1.
 * Otherwise, initialize I/O buffers and return the session array index.
 * We acquire the big lock on the session list here--only one thread at
 * a time is allowed to run this.
 */
int
get_session ()
{
  int k, j, id = -1;
  time_t t;
  omp_set_lock (&biglock);
  for (j = 0; j < MAX_SESSIONS; ++j)
    {
      if (sessions[j].available == SESSION_AVAILABLE)
        {
          k = init_session (&sessions[j]);
          if (k > 0)
            {
              id = j;
              break;
            }
        }
    }
  if (id < 0)
    {
      time (&t);
      /* Couldn't find any available sessions. Check for orphans. */
      for (j = 0; j < MAX_SESSIONS; ++j)
        {
          if (t - sessions[j].time > TIMEOUT)
            {
              syslog (LOG_INFO, "get_session: reaping session %d", j);
              omp_set_lock (&sessions[j].lock);
              cleanup_session (&sessions[j]);
              omp_unset_lock (&sessions[j].lock);
              if (init_session (&sessions[j]) > 0)
                {
                  id = j;
                  break;
                }
            }
        }
    }
  omp_unset_lock (&biglock);
  return id;
}


/*
 * 200 is returned with no message.
 */
/*
*** DISCONTINUED ***
void
login (struct mg_connection *conn)
{
  respond (conn, plain, 200, 0, NULL);
  return;
}
*/


/*
 * 200 is returned with no message.
 */
/*
*** DISCONTINUED ***
void
logout (struct mg_connection *conn)
{
  respond (conn, plain, 200, 0, NULL);
  return;
}
*/


/* Client data upload
 * POST data upload to server-side file defined in the session
 * identified by the 'id' variable in the mg_request_info query string.
 *
 * Respond to the client connection as follows:
 * 200 success, <uploaded filename>\r\n returned in body
 * 400 missing arguments, empty file
 * 404 session not found
 */
void
upload (struct mg_connection *conn, const struct mg_request_info *ri)
{
  int k;
  session *s;
  char ID[SESSIONID_LEN];
  char buf[MAX_VARLEN];
  if (!ri->query_string)
    {
      syslog (LOG_ERR, "upload: ERROR %s", MSG_ERR_HTTP_400_ARG);
      respond (conn,
               plain,
               HTTP_400_BAD_REQUEST,
               strlen (MSG_ERR_HTTP_400_ARG),
               MSG_ERR_HTTP_400_ARG);
      return;
    }
  k = strlen (ri->query_string);
  mg_get_var (ri->query_string, k, "id", ID, SESSIONID_LEN);
  s = find_session (ID);
  if (s)
    {
      omp_set_lock (&s->lock);
      s->time = time (NULL) + WEEK;     // Upload should take less than a week!
      k = mg_post_upload (conn, s->ibuf, 0, 0);
      if (k < 1)                        // Upload size is less that 1 byte
        {
          time (&s->time);
          omp_unset_lock (&s->lock);
          syslog (LOG_INFO, "upload[%.*s]: ERROR %s",
                  SESSIONID_SHOW_LEN,
                  s->sessionid,
                  MSG_ERR_HTTP_400_EFL);
          respond (conn,
                   plain,
                   HTTP_400_BAD_REQUEST,
                   strlen (MSG_ERR_HTTP_400_EFL),
                   MSG_ERR_HTTP_400_EFL);
          return;
        }
      time (&s->time);
      snprintf (buf, MAX_VARLEN, "%s", s->ibuf);
// XXX if fails, report server error too
      respond (conn, plain, 200, strlen (buf), buf);    // XXX report bytes uploaded
      omp_unset_lock (&s->lock);
    }
  else
    {
      // not found
      syslog (LOG_INFO, "upload: ERROR %s", MSG_ERR_HTTP_404);
      respond (conn,
               plain,
               HTTP_404_NOT_FOUND,
               strlen (MSG_ERR_HTTP_404),
               MSG_ERR_HTTP_404);
    }
  return;
}


/* Client file upload
 * POST multipart/file upload to server-side file defined in the session
 * identified by the 'id' variable in the mg_request_info query string.
 *
 * Respond to the client connection as follows:
 * 200 success, <uploaded filename>\r\n returned in body
 * 400 missing arguments
 * 404 session not found
 */
/*
*** DISCONTINUED ***
void
upload_file (struct mg_connection *conn, const struct mg_request_info *ri)
{
  int k;
  session *s;
  char ID[SESSIONID_LEN];
  char buf[MAX_VARLEN];
  if (!ri->query_string)
    {
      syslog (LOG_ERR, "upload_file: ERROR %s", MSG_ERR_HTTP_400_ARG);
      respond (conn,
               plain,
               HTTP_400_BAD_REQUEST,
               strlen (MSG_ERR_HTTP_400_ARG),
               MSG_ERR_HTTP_400_ARG);
      return;
    }
  k = strlen (ri->query_string);
  mg_get_var (ri->query_string, k, "id", ID, SESSIONID_LEN);
  s = find_session (ID);
  if (s)
    {
      omp_set_lock (&s->lock);
      s->time = time (NULL) + WEEK;     // Upload should take less than a week!
      mg_append (conn, s->ibuf);
      time (&s->time);
      snprintf (buf, MAX_VARLEN, "%s\r\n", s->ibuf);
// XXX if mg_append fails, report server error too
      respond (conn, plain, 200, strlen (buf), buf);    // XXX report bytes uploaded
      omp_unset_lock (&s->lock);
    }
  else
    {
      // not found
      syslog (LOG_INFO, "upload_file: ERROR %s", MSG_ERR_HTTP_404);
      respond (conn,
               plain,
               HTTP_404_NOT_FOUND,
               strlen (MSG_ERR_HTTP_404),
               MSG_ERR_HTTP_404);
    }
  return;
}
*/


/* Obtain a new session for the mongoose client in conn.
 *
 * Parameters:
 * user=<user name> (optional)
 * password=<password> (optional)
 * admin={0 or 1} (optional, default 0) if set to 1, open a
 *   higher-priority session. This is identical to the --admin flag
 *   for the iquery client (see SciDB Documentation for details). The
 *   default value is 0.  user optional SciDB authentication user name
 *
 * Respond to the client connection as follows:
 * 200 success
 * 401 authentication failure
 * 502 SciDB connection failed
 * 503 out of resources
 */
void
new_session (struct mg_connection *conn, const struct mg_request_info *ri)
{
  char USER[MAX_VARLEN];
  char PASS[MAX_VARLEN];
  char var[MAX_VARLEN];
  char buf[MAX_VARLEN];
  int is_admin = 0;

  memset (USER, 0, MAX_VARLEN);
  memset (PASS, 0, MAX_VARLEN);

  if (ri->query_string)
    {
      int k = strlen (ri->query_string);

      mg_get_var (ri->query_string, k, "user", USER, MAX_VARLEN);
      mg_get_var (ri->query_string, k, "password", PASS, MAX_VARLEN);

      mg_get_var (ri->query_string, k, "admin", var, MAX_VARLEN);
      if (strlen (var) > 0)
        {
          is_admin = atoi (var);
        }
    }

  int j = get_session ();
  if (j > -1)
    {
      session *s = &sessions[j];

      for (int i = 0; i < 2; i++)
        {
          syslog (LOG_INFO,
                  "new_session[%.*s]: scidbconnect [%d], user %s, is_admin %d",
                  SESSIONID_SHOW_LEN,
                  s->sessionid,
                  i,
                  USER,
                  is_admin);
          int status;
          s->scidb[i] = scidbconnect (SCIDB_HOST,
                                      SCIDB_PORT,
                                      strlen (USER) > 0 ? USER : NULL,
                                      strlen (PASS) > 0 ? PASS : NULL,
                                      is_admin,
                                      &status);
          if (!s->scidb[i])
            {
              if(status == SHIM_ERROR_AUTHENTICATION)
                {
                  syslog (LOG_ERR, "ERROR %s", MSG_ERR_HTTP_401);
                  respond (conn,
                           plain,
                           HTTP_401_UNAUTHORIZED,
                           strlen (MSG_ERR_HTTP_401),
                           MSG_ERR_HTTP_401);
                }
              else
                {
                  syslog (LOG_ERR, "ERROR %s", MSG_ERR_HTTP_502);
                  respond (conn,
                           plain,
                           HTTP_502_BAD_GATEWAY,
                           strlen (MSG_ERR_HTTP_502),
                           MSG_ERR_HTTP_502);
                }

	      // If the connection to SciDB failed for index 1,
	      // then cleanup the successful connection made on
	      // index 0, if it exists (which it should at this
	      // point, but be paranoid).
	      if (i > 0 && s->scidb[0]) {
		scidbdisconnect (s->scidb[0]);
	      }

	      // Cleanup session memory after the disconnect.
              cleanup_session (s);

              return;
            }
        }

      syslog (LOG_INFO,
              "new_session[%.*s]: ready, ibuf %s, obuf %s, "
              "scidb[0] %p, scidb[1] %p",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              s->ibuf,
              s->obuf,
              s->scidb[0],
              s->scidb[1]);
      snprintf (buf, MAX_VARLEN, "%s", s->sessionid);
      respond (conn, plain, 200, strlen (buf), buf);
    }
  else
    {
      // out of resources
      syslog (LOG_ERR, "new_session: ERROR %s", MSG_ERR_HTTP_503);
      respond (conn,
               plain,
               HTTP_503_UNAVAILABLE,
               strlen (MSG_ERR_HTTP_503),
               MSG_ERR_HTTP_503);
    }
}


/* Return shim's version build
 *
 * Respond to the client connection as follows:
 * 200 success
 */
void
version (struct mg_connection *conn)
{
  char buf[MAX_VARLEN];
  snprintf (buf, MAX_VARLEN, "%s", VERSION);
  respond (conn, plain, 200, strlen (buf), buf);
}

#ifdef DEBUG
void
debug (struct mg_connection *conn)
{
  int j;
  char buf[MAX_VARLEN];
  char *p = buf;
  size_t l, k = MAX_VARLEN;
  omp_set_lock (&biglock);
  for (j = 0; j < MAX_SESSIONS; ++j)
    {
      l =
        snprintf (p,
                  k,
                  "slot %d, sid %.*s, avail %d\n",
                  j,
                  SESSIONID_SHOW_LEN,
                  sessions[j].sessionid,
                  sessions[j].available);
      k = k - l;
      if (k <= 0)
        break;
      p = p + l;
    }
  omp_unset_lock (&biglock);
  respond (conn, plain, 200, strlen (buf), buf);
}
#endif


/* Read bytes from a query result output buffer.
 * The mg_request_info must contain the keys:
 * n=<max number of bytes to read -- signed int32>
 * id=<session id>
 * Writes at most n bytes back to the client or a 416 error if something
 * went wrong.
 *
 * A 500 error invalidates and releases the session.
 *
 * Respond to the client connection as follows:
 * 200 success
 * 400 missing arguments
 * 404 session not found
 * 410 output not saved
 * 416 range out of bounds
 * 416 output not saved in binary format
 * 500 out of memory, failed to open output buffer, get file status failed
 */
void
read_bytes (struct mg_connection *conn, const struct mg_request_info *ri)
{
  int k, n, pl, l;
  session *s;
  struct pollfd pfd;
  char *buf;
  char var[MAX_VARLEN];
  char ID[SESSIONID_LEN];
  struct stat st;
  if (!ri->query_string)
    {
      syslog (LOG_ERR, "read_bytes: ERROR %s", MSG_ERR_HTTP_400_ARG);
      respond (conn,
               plain,
               HTTP_400_BAD_REQUEST,
               strlen (MSG_ERR_HTTP_400_ARG),
               MSG_ERR_HTTP_400_ARG);
      return;
    }
  k = strlen (ri->query_string);
  mg_get_var (ri->query_string, k, "id", ID, SESSIONID_LEN);
  s = find_session (ID);
  if (!s)
    {
      // not found
      syslog (LOG_INFO, "read_bytes: ERROR %s", MSG_ERR_HTTP_404);
      respond (conn,
               plain,
               HTTP_404_NOT_FOUND,
               strlen (MSG_ERR_HTTP_404),
               MSG_ERR_HTTP_404);
      return;
    }
  if (!s->save)
    {
      syslog (LOG_ERR,
              "read_bytes[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_410);
      respond (conn,
               plain,
               HTTP_410_GONE,
               strlen (MSG_ERR_HTTP_410),
               MSG_ERR_HTTP_410);
      return;
    }
  if (s->save != SAVE_BIN)
    {
      syslog (LOG_ERR,
              "read_bytes[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_416_BIN);
      respond (conn,
               plain,
               HTTP_416_NOT_SATISFIABLE,
               strlen (MSG_ERR_HTTP_416_BIN),
               MSG_ERR_HTTP_416_BIN);
      return;
    }
  omp_set_lock (&s->lock);
// Check to see if the output buffer is open for reading, if not do so.
  if (s->pd < 1)
    {
      s->pd = open (s->obuf, O_RDONLY | O_NONBLOCK);

      if (s->pd < 1)
        {
          syslog (LOG_ERR,
                  "read_bytes[%.*s]: ERROR %s",
                  SESSIONID_SHOW_LEN,
                  s->sessionid,
                  MSG_ERR_HTTP_500_BUF);
          respond (conn,
                   plain,
                   HTTP_500_SERVER_ERROR,
                   strlen (MSG_ERR_HTTP_500_BUF),
                   MSG_ERR_HTTP_500_BUF);
          cleanup_session (s);
          omp_unset_lock (&s->lock);
          return;
        }
    }
  memset (var, 0, MAX_VARLEN);
// Retrieve max number of bytes to read
  mg_get_var (ri->query_string, k, "n", var, MAX_VARLEN);
  n = atoi (var);
  if (n < 1)
    {
      syslog (LOG_INFO,
              "read_bytes[%.*s]: return entire buffer",
              SESSIONID_SHOW_LEN,
              s->sessionid);
      mg_send_file (conn, s->obuf);
      omp_unset_lock (&s->lock);
      syslog (LOG_INFO,
              "read_bytes[%.*s]: done",
              SESSIONID_SHOW_LEN,
              s->sessionid);
      return;
    }
  if (n > MAX_RETURN_BYTES) {
    n = MAX_RETURN_BYTES;
  }
  if (fstat (s->pd, &st) < 0)
    {
      syslog (LOG_ERR,
              "read_bytes[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_FST);
      respond (conn,
               plain,
               HTTP_500_SERVER_ERROR,
               strlen (MSG_ERR_HTTP_500_FST),
               MSG_ERR_HTTP_500_FST);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return;
    }
  if ((off_t) n > st.st_size)
    n = (int) st.st_size;

  buf = (char *) malloc (n);
  if (!buf)
    {
      syslog (LOG_ERR,
              "read_bytes[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      respond (conn,
               plain,
               HTTP_500_SERVER_ERROR,
               strlen (MSG_ERR_HTTP_500_OOM),
               MSG_ERR_HTTP_500_OOM);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return;
    }

  pfd.fd = s->pd;
  pfd.events = POLLIN;
  pl = 0;
  while (pl < 1)
    {
      pl = poll (&pfd, 1, 250);
      if (pl < 0)
        break;
    }

  l = (int) read (s->pd, buf, n);
  syslog (LOG_INFO, "read_bytes[%.*s]: read, requested %d, read %d",
          SESSIONID_SHOW_LEN,
          s->sessionid,
          l,
          n);
  if (l < 1)                    // EOF or error
    {
      free (buf);
      syslog (LOG_ERR,
              "read_bytes[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_416_EOF);
      respond (conn,
               plain,
               HTTP_416_NOT_SATISFIABLE,
               strlen (MSG_ERR_HTTP_416_EOF),
               MSG_ERR_HTTP_416_EOF);
      omp_unset_lock (&s->lock);
      return;
    }
  respond (conn, binary, 200, l, buf);
  free (buf);
  time (&s->time);
  omp_unset_lock (&s->lock);
}


/* Read ascii lines from a query result on the query pipe.
 * The mg_request_info must contain the keys:
 * n = <max number of lines to read>
 *     Set n to zero to just return all the data. n>0 allows
 *     repeat calls to this function to iterate through data n lines at a time
 * id = <session id>
 * Writes at most n lines back to the client or a 419 error if something
 * went wrong or end of file.
 *
 * A 500 error invalidates and releases the session.
 *
 * Respond to the client connection as follows:
 * 200 success
 * 400 missing arguments
 * 404 session not found
 * 410 output not saved
 * 416 range out of bounds
 * 416 output not saved in text format
 * 500 out of memory, failed to open output buffer
 */
void
read_lines (struct mg_connection *conn, const struct mg_request_info *ri)
{
  int k, n, pl;
  ssize_t l;
  size_t m, t, v;
  session *s;
  char *lbuf, *buf, *p, *tmp;
  struct pollfd pfd;
  char var[MAX_VARLEN];
  char ID[SESSIONID_LEN];
  if (!ri->query_string)
    {
      syslog (LOG_ERR, "read_lines: ERROR %s", MSG_ERR_HTTP_400_ARG);
      respond (conn,
               plain,
               HTTP_400_BAD_REQUEST,
               strlen (MSG_ERR_HTTP_400_ARG),
               MSG_ERR_HTTP_400_ARG);
      return;
    }
  k = strlen (ri->query_string);
  mg_get_var (ri->query_string, k, "id", ID, SESSIONID_LEN);
  s = find_session (ID);
  if (!s)
    {
      // not found
      syslog (LOG_INFO, "read_lines: ERROR %s", MSG_ERR_HTTP_404);
      respond (conn,
               plain,
               HTTP_404_NOT_FOUND,
               strlen (MSG_ERR_HTTP_404),
               MSG_ERR_HTTP_404);
      return;
    }
  if (!s->save)
    {
      syslog (LOG_ERR,
              "read_lines[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_410);
      respond (conn,
               plain,
               HTTP_410_GONE,
               strlen (MSG_ERR_HTTP_410),
               MSG_ERR_HTTP_410);
      return;
    }
  if (s->save != SAVE_TXT)
    {
      syslog (LOG_ERR,
              "read_bytes[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_416_TXT);
      respond (conn,
               plain,
               HTTP_416_NOT_SATISFIABLE,
               strlen (MSG_ERR_HTTP_416_TXT),
               MSG_ERR_HTTP_416_TXT);
      return;
    }
// Retrieve max number of lines to read
  mg_get_var (ri->query_string, k, "n", var, MAX_VARLEN);
  n = atoi (var);
// Check to see if client wants the whole file at once, if so return it.
  omp_set_lock (&s->lock);
  if (n < 1)
    {
      syslog (LOG_INFO,
              "read_lines[%.*s]: return entire buffer",
              SESSIONID_SHOW_LEN,
              s->sessionid);
      mg_send_file (conn, s->obuf);
      omp_unset_lock (&s->lock);
      return;
    }
// Check to see if output buffer is open for reading
  syslog (LOG_INFO,
          "read_lines[%.*s]: opening buffer",
          SESSIONID_SHOW_LEN,
          s->sessionid);
  if (s->pd < 1)
    {
      s->pd = open (s->obuf, O_RDONLY | O_NONBLOCK);
      if (s->pd < 1)
        {
          syslog (LOG_ERR,
                  "read_lines[%.*s]: ERROR %s",
                  SESSIONID_SHOW_LEN,
                  s->sessionid,
                  MSG_ERR_HTTP_500_BUF);
          respond (conn,
                   plain,
                   HTTP_500_SERVER_ERROR,
                   strlen (MSG_ERR_HTTP_500_BUF),
                   MSG_ERR_HTTP_500_BUF);
          cleanup_session (s);
          omp_unset_lock (&s->lock);
          return;
        }
    }
  if (s->pf == NULL)
    {
      s->pf = fdopen (s->pd, "r");
      if (s->pf == NULL)
        {
          syslog (LOG_ERR,
                  "read_lines[%.*s]: ERROR %s",
                  SESSIONID_SHOW_LEN,
                  s->sessionid,
                  MSG_ERR_HTTP_500_BUF);
          respond (conn,
                   plain,
                   HTTP_500_SERVER_ERROR,
                   strlen (MSG_ERR_HTTP_500_BUF),
                   MSG_ERR_HTTP_500_BUF);
          cleanup_session (s);
          omp_unset_lock (&s->lock);
          return;
        }
    }
  memset (var, 0, MAX_VARLEN);
  if (n * MAX_VARLEN > MAX_RETURN_BYTES)
    {
      n = MAX_RETURN_BYTES / MAX_VARLEN;
    }

  k = 0;
  t = 0;                        // Total bytes read so far
  m = MAX_VARLEN;
  v = m * n;                    // Output buffer size
  lbuf = (char *) malloc (m);
  if (!lbuf)
    {
      syslog (LOG_ERR,
              "read_lines[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      respond (conn,
               plain,
               HTTP_500_SERVER_ERROR,
               strlen (MSG_ERR_HTTP_500_OOM),
               MSG_ERR_HTTP_500_OOM);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return;
    }
  buf = (char *) malloc (v);
  if (!buf)
    {
      free (lbuf);
      syslog (LOG_ERR,
              "read_lines[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      respond (conn,
               plain,
               HTTP_500_SERVER_ERROR,
               strlen (MSG_ERR_HTTP_500_OOM),
               MSG_ERR_HTTP_500_OOM);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return;
    }
  while (k < n)
    {
      memset (lbuf, 0, m);
      pfd.fd = s->pd;
      pfd.events = POLLIN;
      pl = 0;
      while (pl < 1)
        {
          pl = poll (&pfd, 1, 250);
          if (pl < 0)
            break;
        }

      l = getline (&lbuf, &m, s->pf);
      if (l < 0)
        {
          break;
        }
// Check if buffer is big enough to contain next line
      if (t + l > v)
        {
          v = 2 * v;
          tmp = realloc (buf, v);
          if (!tmp)
            {
              free (lbuf);
              free (buf);
              syslog (LOG_ERR,
                      "read_lines[%.*s]: ERROR %s",
                      SESSIONID_SHOW_LEN,
                      s->sessionid,
                      MSG_ERR_HTTP_500_OOM);
              respond (conn,
                       plain,
                       HTTP_500_SERVER_ERROR,
                       strlen (MSG_ERR_HTTP_500_OOM),
                       MSG_ERR_HTTP_500_OOM);
              cleanup_session (s);
              omp_unset_lock (&s->lock);
              return;
            }
          else
            {
              buf = tmp;
            }
        }
// Copy line into buffer
      p = buf + t;
      t += l;
      memcpy (p, lbuf, l);
      k += 1;
    }
  free (lbuf);
  if (t == 0)                   // EOF
    {
      syslog (LOG_ERR,
              "read_lines[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_416_EOF);
      respond (conn,
               plain,
               HTTP_416_NOT_SATISFIABLE,
               strlen (MSG_ERR_HTTP_416_EOF),
               MSG_ERR_HTTP_416_EOF);
    }
  else
    {
      respond (conn, plain, 200, t, buf);
    }
  free (buf);
  time (&s->time);
  omp_unset_lock (&s->lock);
}


/* execute_query blocks until the query is complete.
 * This function always disconnects from SciDB after completeQuery finishes.
 * query string variables:
 * id=<session id> (required)
 * query=<query string> (required)
 * prefix=<query string> (optional) a statement to execute first, if supplied
 * save=<format string> (optional, default 0-length string)
 *   set the save format to something to wrap the query in a save
 * atts_only={0 or 1} (optional, default 1) specify whether the output
 *   should only include attribute values or include attribute as well
 *   as dimension values. If atts_only=0 is specified the dimension
 *   values are appended for each cell after the attribute values. The
 *   type used for the dimension values is int64. This setting is only
 *   applicable when the "save" with either binary or the "arrow"
 *   formats is used. For the binary format, the format specification
 *   has to include an int64 type specifications (appended at the end)
 *   for each of the input array dimensions.
 *
 * DISABLED
 * release={0 or 1}  (optional, default 0)
 *   release > 0 invokes release_session after completeQuery.
 *
 *
 * A 500 or 502 error invalidates and releases the session.
 *
 * Respond to the client connection as follows:
 * 200 success
 * 400 missing arguments
 * 404 session not found
 * 406 SciDB query error
 * 500 out of memory
 * 502 SciDB connection failed
 */
void
execute_query (struct mg_connection *conn, const struct mg_request_info *ri)
{
  int k;
  ShimQueryID q;
  session *s;
  char buf[MAX_VARLEN];
  char var[MAX_VARLEN];
  char result_size_limit[MAX_VARLEN];
  char SERR[MAX_VARLEN];
  char ID[SESSIONID_LEN];
  int atts_only = 1;

  char *qrybuf, *qry, *prefix, *save;
  struct prep pq;               // prepared query storage

  if (!ri->query_string)
    {
      syslog (LOG_ERR, "execute_query: ERROR %s", MSG_ERR_HTTP_400_ARG);
      respond (conn,
               plain,
               HTTP_400_BAD_REQUEST,
               strlen (MSG_ERR_HTTP_400_ARG),
               MSG_ERR_HTTP_400_ARG);
      return;
    }
  k = strlen (ri->query_string);
  mg_get_var (ri->query_string, k, "id", ID, SESSIONID_LEN);
  memset (var, 0, MAX_VARLEN);

  // Disable `release` in https://github.com/Paradigm4/shim/issues/88
  /*
  int rel = 0;
  mg_get_var (ri->query_string, k, "release", var, MAX_VARLEN);
  if (strlen (var) > 0)
    {
      rel = atoi (var);
    }
  */

  memset (var, 0, MAX_VARLEN);
  s = find_session (ID);
  if (!s)
    {
      // not found
      syslog (LOG_INFO, "execute_query: ERROR %s", MSG_ERR_HTTP_404);
      respond (conn,
               plain,
               HTTP_404_NOT_FOUND,
               strlen (MSG_ERR_HTTP_404),
               MSG_ERR_HTTP_404);
      return;
    }
  qrybuf = (char *) malloc (k);
  if (!qrybuf)
    {
      syslog (LOG_ERR,
              "execute_query[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      respond (conn,
               plain,
               HTTP_500_SERVER_ERROR,
               strlen (MSG_ERR_HTTP_500_OOM),
               MSG_ERR_HTTP_500_OOM);
      omp_set_lock (&s->lock);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return;
    }
  mg_get_var (ri->query_string, k, "query", qrybuf, k);
  if (strlen (qrybuf) == 0)
    {
      free(qrybuf);
      syslog (LOG_ERR,
              "execute_query[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_400_ARG);
      respond (conn,
               plain,
               HTTP_400_BAD_REQUEST,
               strlen (MSG_ERR_HTTP_400_ARG),
               MSG_ERR_HTTP_400_ARG);
      return;
    }
  qry = (char *) malloc (k + MAX_VARLEN);
  if (!qry)
    {
      free (qrybuf);
      syslog (LOG_ERR,
              "execute_query[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      respond (conn,
               plain,
               HTTP_500_SERVER_ERROR,
               strlen (MSG_ERR_HTTP_500_OOM),
               MSG_ERR_HTTP_500_OOM);
      omp_set_lock (&s->lock);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return;
    }
  prefix = (char*) malloc (k);
  if (!prefix)
    {
      free (qrybuf);
      free (qry);
      syslog (LOG_ERR,
              "execute_query[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      respond (conn,
               plain,
               HTTP_500_SERVER_ERROR,
               strlen (MSG_ERR_HTTP_500_OOM),
               MSG_ERR_HTTP_500_OOM);
      omp_set_lock (&s->lock);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return;
    }
  mg_get_var (ri->query_string, k, "prefix", prefix, k);
  if (strlen (prefix) == 0)
    {
      free (prefix);
      prefix = 0;
    }
  memset (var, 0, MAX_VARLEN);
  save = (char*) malloc (k);
  if (!save)
    {
      free (qrybuf);
      free (qry);
      free (prefix);
      syslog (LOG_ERR,
              "execute_query[%.*s]: ERROR %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              MSG_ERR_HTTP_500_OOM);
      respond (conn,
               plain,
               HTTP_500_SERVER_ERROR,
               strlen (MSG_ERR_HTTP_500_OOM),
               MSG_ERR_HTTP_500_OOM);
      omp_set_lock (&s->lock);
      cleanup_session (s);
      omp_unset_lock (&s->lock);
      return;
    }
  mg_get_var (ri->query_string, k, "save", save, k);
  if (strlen (save) == 0)
    {
      free (save);
      save = NULL;
    }
  mg_get_var (ri->query_string, k, "result_size_limit", result_size_limit, MAX_VARLEN);
  omp_set_lock (&s->lock);
// If save is indicated, modify query
  if (save != NULL && strlen (save) > 0)
    {
      if (save[0] == '('
          || strcmp (save, "arrow") == 0)
        {
          s->save = SAVE_BIN;
        }
      else
        {
          s->save = SAVE_TXT;
        }
      if (USE_AIO == 1
          && (save[0] == '('
              || strcmp (save, "csv+") == 0
              || strcmp (save, "lcsv+") == 0
              || strcmp (save, "arrow") == 0))
        {
           mg_get_var (ri->query_string, k, "atts_only", var, MAX_VARLEN);
           if (strlen (var) > 0)
             {
                atts_only = atoi (var);
             }

		if (strcmp(result_size_limit, "") == 0)
		{
			snprintf (qry, k + MAX_VARLEN,
				  "aio_save(%s,paths:('%s'),instances:(%d),format:'%s',atts_only:%d)",
				  qrybuf, s->obuf, SAVE_INSTANCE_ID,
				  save,
				  atts_only);
		}
		else
		{
			snprintf (qry, k + MAX_VARLEN,
				  "aio_save(%s,paths:('%s'),instances:(%d),format:'%s',atts_only:%d, result_size_limit:%s)",
				  qrybuf, s->obuf, SAVE_INSTANCE_ID,
				  save,
				  atts_only,
				  result_size_limit);
		}
        }
      else
        {
          snprintf (qry, k + MAX_VARLEN, "save(%s,'%s',%d,'%s')", qrybuf,
                    s->obuf, SAVE_INSTANCE_ID, save);
        }
    }
  else
    {
      /* s->save is initalized with 0. Do no reset it to 0 here. If it
         was set to non zero by a previous execute_query, let it stay
         set to non zero. This way the prevously saved output is still
         available even if other queries were executed since then. */
      // s->save = 0;
      snprintf (qry, k + MAX_VARLEN, "%s", qrybuf);
    }

  syslog (LOG_INFO,
          "execute_query[%.*s]: execute, scidb[0] %p, scidb[1] %p, query %s",
          SESSIONID_SHOW_LEN,
          s->sessionid,
          s->scidb[0],
          s->scidb[1],
          qry);

  if (prefix) // 1 or more statements to run first
    {
       char *qstart = prefix, *qend = qstart, last =0; //split on ';' (yes I think the internal AFL parser should do this too)
       while (last!=1)
         {
           qend = qstart;
           while (*qend != ';' && *qend != 0)
             ++qend;
           if (*qend == 0)
             last = 1;
           else
             *qend = 0;      //simulate null-termination
           syslog (LOG_INFO,
                   "execute_query[%.*s]: prepare prefix",
                   SESSIONID_SHOW_LEN,
                   s->sessionid);
           prepare_query (&pq, s->scidb[0], qstart, 1, SERR);
           q = pq.queryid;
           if (q.queryid < 1 || !pq.queryresult)
             {
               free (qry);
               free (qrybuf);
               free (prefix);
               free (save);
               syslog (LOG_ERR,
                       "execute_query: ERROR prepare prefix, %.*s: %s",
                       SESSIONID_SHOW_LEN,
                       s->sessionid,
                       SERR);
               respond_to_query_error(conn, s, SERR);
               omp_unset_lock (&s->lock);
               return;
             }
           s->qid = q;                    //not sure if I need these
           s->time = time (NULL) + WEEK;
           if (s->scidb[0])
             {
               q = execute_prepared_query (s->scidb[0], qstart, &pq, 1, SERR);
             }
           if (q.queryid < 1)            // something went wrong
             {
               free (qry);
               free (qrybuf);
               free (prefix);
               free (save);
               syslog (LOG_ERR,
                       "execute_query: ERROR execute prefix, %.*s: %s",
                       SESSIONID_SHOW_LEN,
                       s->sessionid,
                       SERR);
               respond_to_query_error(conn, s, SERR);
	       if (strstr(SERR, "result size limit") != NULL) {
		       syslog (LOG_ERR,
			       "removing, %s",
			       s->obuf);
		       remove(s->obuf);
	       }
               omp_unset_lock (&s->lock);
               return;
            }
           if (s->scidb[0])
             {
               completeQuery (q, s->scidb[0], SERR);
             }
           qstart = qend + 1;
         }
    }

  prepare_query (&pq, s->scidb[0], qry, 1, SERR);
  q = pq.queryid;
  if (q.queryid < 1 || !pq.queryresult)
    {
      free (qry);
      free (qrybuf);
      free (prefix);
      free (save);
      syslog (LOG_ERR,
              "execute_query: ERROR prepare, %.*s: %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              SERR);
      respond_to_query_error(conn, s, SERR);
      omp_unset_lock (&s->lock);
      return;
    }
  syslog (LOG_INFO,
          "execute_query[%.*s]: execute, qid %llu.%llu",
          SESSIONID_SHOW_LEN,
          s->sessionid,
          q.coordinatorid,
          q.queryid);
/* Set the queryID for potential future cancel event.
 * The time flag is set to a future value to prevent get_session from
 * declaring this session orphaned while a query is running. This
 * session cannot be reclaimed until the query finishes, since the
 * lock is held.
 */
  s->qid = q;
  s->time = time (NULL) + WEEK;
  if (s->scidb[0])
    {
      q = execute_prepared_query (s->scidb[0], qry, &pq, 1, SERR);
    }
  if (q.queryid < 1)            // something went wrong
    {
      free (qry);
      free (qrybuf);
      free (prefix);
      free (save);
      syslog (LOG_ERR,
              "execute_query: ERROR execute, %.*s: %s",
              SESSIONID_SHOW_LEN,
              s->sessionid,
              SERR);
      respond_to_query_error(conn, s, SERR);
      if (strstr(SERR, "result size limit") != NULL) {
	      syslog (LOG_ERR,
		      "removing, %s",
		      s->obuf);
	      remove(s->obuf);
      }

      omp_unset_lock (&s->lock);
      return;
    }
  if (s->scidb[0])
    {
      completeQuery (q, s->scidb[0], SERR);
    }

  free (qry);
  free (qrybuf);
  free (prefix);
  free (save);
  syslog (LOG_INFO,
          "execute_query[%.*s]: done",
          SESSIONID_SHOW_LEN,
          s->sessionid);

  // Disable `release` in https://github.com/Paradigm4/shim/issues/88
  /*
  if (rel > 0)
    {
      syslog (LOG_INFO,
              "execute_query[%.*s]: disconnecting",
              SESSIONID_SHOW_LEN,
              s->sessionid);
      for (int i = 0; i < 2; i++)
        {
          if (s->scidb[i])
            {
              scidbdisconnect (s->scidb[i]);
              s->scidb[i] = NULL;
            }
        }
      syslog (LOG_INFO, "execute_query[%.*s]: releasing",
              SESSIONID_SHOW_LEN,
              s->sessionid);
      cleanup_session (s);
    }
  */

  time (&s->time);
  omp_unset_lock (&s->lock);
  // Respond to the client (the query ID)
  snprintf (buf, MAX_VARLEN, "%llu", q.queryid);        // Return the query ID
  respond (conn, plain, 200, strlen (buf), buf);
}


void
get_log (struct mg_connection *conn)
{
  char path[MAX_VARLEN];
  snprintf(path,
           MAX_VARLEN,
           "%s/.scidb.log",
           TMPDIR);

  char cmd[MAX_VARLEN];
  snprintf(cmd,
           MAX_VARLEN,
           "tail -n 1555 "
           "  `ps axu "
           "   | grep SciDB "
           "   | grep \"\\/0\\/0\" "
           "   | head -n 1 "
           "   | sed -e \"s/SciDB-0-0.*//\" "
           "   | sed -e \"s/.* \\//\\//\"`/scidb.log "
           " > %s", path);
  system (cmd);

  mg_send_file (conn, path);
}


/* Mongoose generic begin_request callback; we dispatch URIs to their
 * appropriate handlers.
 */
static int
begin_request_handler (struct mg_connection *conn)
{
  char buf[MAX_VARLEN];
  const struct mg_request_info *ri = mg_get_request_info (conn);

  syslog (LOG_INFO, "%s", ri->uri);

// CLIENT API
  if (!strcmp (ri->uri, "/new_session"))
    new_session (conn, ri);
  else if (!strcmp (ri->uri, "/version"))
    version (conn);
#ifdef DEBUG
  else if (!strcmp (ri->uri, "/debug"))
    debug (conn);
#endif
  /*
  else if (!strcmp (ri->uri, "/login"))
    login (conn);
  else if (!strcmp (ri->uri, "/logout"))
    logout (conn);
  */
  else if (!strcmp (ri->uri, "/release_session"))
    release_session (conn, ri, 1);
  /*
  else if (!strcmp (ri->uri, "/upload_file"))
    upload_file (conn, ri);
  */
  else if (!strcmp (ri->uri, "/upload"))
    upload (conn, ri);
  else if (!strcmp (ri->uri, "/read_lines"))
    read_lines (conn, ri);
  else if (!strcmp (ri->uri, "/read_bytes"))
    read_bytes (conn, ri);
  else if (!strcmp (ri->uri, "/execute_query"))
    execute_query (conn, ri);
  else if (!strcmp (ri->uri, "/cancel"))
    cancel (conn, ri);
// CONTROL API
//      else if (!strcmp (ri->uri, "/stop_scidb"))
//        stopscidb (conn, ri);
//      else if (!strcmp (ri->uri, "/start_scidb"))
//        startscidb (conn, ri);
  else if (!strcmp (ri->uri, "/get_log"))
    get_log (conn);
  else
    {
// fallback to http file server
      if (strstr (ri->uri, ".htpasswd"))
        {
          syslog (LOG_ERR, "ERROR client trying to read password file");
          respond (conn, plain, HTTP_403_FORBIDDEN, 0, NULL);
          goto end;
        }
      if (!strcmp (ri->uri, "/"))
        snprintf (buf, MAX_VARLEN, "%s/index.html", docroot);
      else
        snprintf (buf, MAX_VARLEN, "%s/%s", docroot, ri->uri);
      mg_send_file (conn, buf);
    }

end:
// Mark as processed by returning non-null value.
  return 1;
}


/* Parse the command line options */
void
parse_args (int argc, char **argv, int *daemonize)
{
  int c;
  while ((c = getopt (argc, argv, "hvfc:p:")) != -1)
    {
      switch (c)
        {
        case 'h':
	  printf
	    ("Usage:\nshim [-h] [-v] [-f] [-c <shim configuration file>] [start|stop|restart|status|test]\n");
	  printf
	    ("  -h Prints this help message.\n");
	  printf
	    ("     Start up shim and view http://localhost:8080/help.html from a browser for help with the API.\n");
          printf
            ("  -v Prints the version build ID and exits.\n");
	  printf
	    ("  -f Runs shim in the foreground.\n");
	  printf
	    ("     Default is to run as a daemon.\n");
	  printf
	    ("  -c Overrides the default configuration file of '%s'.\n", CONFFILE);
	  printf
	    ("  -p Overrides the default pid file of '%s'.\n", PIDFILE);
	  printf
	    ("\n");
	  printf
	    ("  start   : start shim.\n");
	  printf
	    ("  stop    : stop currently running shim.\n");
	  printf
	    ("  restart : stop and then start shim.\n");
	  printf
	    ("  status  : status of shim. This is the default.\n");
	  printf
	    ("  test    : configuration shim would run with.\n");
          exit (0);
          break;
        case 'v':
          printf ("SciDB Version: %s\n", VERSION);
          printf ("Shim Commit: %s\n", COMMIT);
          exit (0);
          break;
        case 'f':
          *daemonize = 0;
          break;
	case 'c':
	  CONFFILE = optarg;
	  break;
	case 'p':
	  PIDFILE = optarg;
	  break;
        default:
          break;
        }
    }
  // Use the first command line non-option as the command word
  if (optind < argc) {
    COMMAND = argv[optind];
    // Check for valid commands
    for(int i = 0; COMMAND[i]; i++)
      COMMAND[i] = tolower(COMMAND[i]);
    if ( (strcmp(COMMAND,"start") &&
	  strcmp(COMMAND,"stop") &&
	  strcmp(COMMAND,"restart") &&
	  strcmp(COMMAND,"status") &&
	  strcmp(COMMAND,"test")) !=0 )
      {
	printf ("Unknown command: '%s'.\n", COMMAND);
	printf ("Should be one of: start, stop, restart, status, test.\n");
	exit (1);
      }
  }
}

/* SHIM CONFIG FILE

   This file specifies values for the following configurable parameters:
      ports:        http listening ports
      doc:          document root
      scidbhost:    SciDB host
      scidbport:    SciDB port
      tmp:          temporary I/O directory
      user:         run shim as this user
      max_sessions: maximum concurrent sessions
      timeout:      http session timeout
      instance:     instance id for save to file
      aio:          use (=1) or not use (=0) the aoi tools plugin
   Lines beginning with a '#' are commented out lines.
   Whitespace (blank or tab) are ignored.
*/
   
void
parse_conf (char **mg_options)
{
  dictionary  *   ini ;

  /* Some temporary variables to hold query results */
  int             b ;
  int             i ;
  const char  *   s ;

  ini = iniparser_load(CONFFILE);
  if (ini==NULL) {
    fprintf (stderr, "cannot parse file: %s\n", CONFFILE);
    exit(1);
  }
  // iniparser_dump(ini, stderr);
  /* ports:        http listening ports */
  s = iniparser_getstring(ini, ":ports", NULL);
  if (s != NULL) {
    mg_options[1] = (char *) calloc (PATH_MAX, 1);
    snprintf (mg_options[1], PATH_MAX, s);
  }
  /* doc:          document root */
  s = iniparser_getstring(ini, ":doc", NULL);
  if (s != NULL) {
    mg_options[3] = (char *) calloc (PATH_MAX, 1);
    snprintf (mg_options[3], PATH_MAX, s);
    memset (mg_options[5], 0, PATH_MAX);
    strncat(mg_options[5], s, PATH_MAX);
    strncat(mg_options[5], "/../ssl_cert.pem", PATH_MAX - 17);
  }
  /* scidbhost:    SciDB host */
  s = iniparser_getstring(ini, ":scidbhost", NULL);
  if (s != NULL) {
    SCIDB_HOST = (char *) calloc (PATH_MAX, 1);
    snprintf (SCIDB_HOST, PATH_MAX, s);
  }
  /* scidbport:    SciDB port */
  i = iniparser_getint(ini, ":scidbport", -1);
  if (i != -1) {
    SCIDB_PORT = i;
  }
  /* tmp:          temporary I/O directory */
  s = iniparser_getstring(ini, ":tmp", NULL);
  if (s != NULL) {
    TMPDIR = (char *) calloc (PATH_MAX, 1);
    snprintf (TMPDIR, PATH_MAX, s);
  }
  /* user:         run shim as this user */
  s = iniparser_getstring(ini, ":user", NULL);
  if (s != NULL) {
    AS_USER = (char *) calloc (PATH_MAX, 1);
    snprintf (AS_USER, PATH_MAX, s);
  }
  /* max_sessions: maximum concurrent sessions */
  i = iniparser_getint(ini, ":max_sessions", -1);
  if (i != -1) {
    MAX_SESSIONS = i;
    MAX_SESSIONS = (MAX_SESSIONS > 100 ? 100 : MAX_SESSIONS);
  }
  /* timeout:      http session timeout */
  i = iniparser_getint(ini, ":timeout", -1);
  if (i != -1) {
    TIMEOUT = i;
    TIMEOUT = (TIMEOUT < 60 ? 60 : TIMEOUT);
  }
  /* instance:     instance id for save to file */
  i = iniparser_getint(ini, ":instance", -1);
  if (i != -1) {
    SAVE_INSTANCE_ID = i;
    SAVE_INSTANCE_ID = (SAVE_INSTANCE_ID < 0 ? 0 : SAVE_INSTANCE_ID);
  }
  /* aio:          use (=1) or not use (=0) the aoi tools plugin */
  b = iniparser_getboolean(ini, ":aio", -1);
  if (b == 1) {
    USE_AIO = 1;
  }

  iniparser_freedict(ini);
}

void
debug_args (char **mg_options)
{
  printf ("ports='%s'\n", mg_options[1]);
  printf ("doc='%s'\n", mg_options[3]);
  printf ("cert='%s'\n", mg_options[5]);
  printf ("scidbhost='%s'\n", SCIDB_HOST);
  printf ("scidbport=%d\n", SCIDB_PORT);
  printf ("tmp='%s'\n", TMPDIR);
  printf ("user='%s'\n", AS_USER);
  printf ("max_sessions=%d\n", MAX_SESSIONS);
  printf ("timeout=%ju\n", (uintmax_t)TIMEOUT);
  printf ("instance=%d\n", SAVE_INSTANCE_ID);
  printf ("aio=%d\n", USE_AIO);
}

static void
signalHandler (int sig)
{
  /* catch termination signals and shut down gracefully */
  int j;
  signal (sig, signalHandler);
  omp_set_lock (&biglock);
  for (j = 0; j < MAX_SESSIONS; ++j)
    {
      syslog (LOG_INFO, "Terminating, reaping session %d", j);
/* note: we intentionally forget about acquiring locks here,
 * we are about to exit!
 */
      cleanup_session (&sessions[j]);
    }
  omp_unset_lock (&biglock);
  exit (0);
}


int
main (int argc, char **argv)
{
  int j, k, daemonize = 1;
  char *cp, *ports;
  struct mg_context *ctx;
  struct mg_callbacks callbacks;
  struct stat check_ssl;
  char pbuf[MAX_VARLEN];
  //  mg_options:
  //    NULL terminated list of option_name, option_value pairs that
  //    specify Mongoose configuration parameters.
  char *mg_options[9];
  mg_options[0] = "listening_ports";
  mg_options[1] = DEFAULT_HTTP_PORT;
  mg_options[2] = "document_root";
  mg_options[3] = "/var/lib/shim/wwwroot";
  mg_options[4] = "ssl_certificate";
  mg_options[5] = (char *) calloc (PATH_MAX, 1);
  snprintf (mg_options[5], PATH_MAX, "/var/lib/shim/ssl_cert.pem");
  mg_options[6] = "authentication_domain";
  mg_options[7] = "";
  mg_options[8] = NULL;

  CONFFILE = DEFAULT_CONFFILE;
  TMPDIR = DEFAULT_TMPDIR;
  TIMEOUT = DEFAULT_TIMEOUT;
  MAX_SESSIONS = DEFAULT_MAX_SESSIONS;
  SAVE_INSTANCE_ID = DEFAULT_SAVE_INSTANCE_ID;
  USE_AIO = 0;
  COMMAND = "status";
  PIDFILE = DEFAULT_PIDFILE;
  AS_USER = NULL;

  parse_args (argc, argv, &daemonize);
  // debug_args (mg_options);
  parse_conf (mg_options);
  // debug_args (mg_options);

  FILE *f;
  int pid;
  // Implement command
  if (strcmp (COMMAND,"status") == 0) {
    // if no pid file then no running shim
    f=fopen(PIDFILE,"r");
    if (f == NULL) {
      printf ("shim is stopped.\n");
      exit (0);
    }
    fscanf(f,"%d", &pid);
    fclose(f);
    // Existance of /proc/pid means its running
    snprintf (pbuf, MAX_VARLEN, "/proc/%d", pid);
    if( access( pbuf, F_OK ) == 0 ) {
      printf ("shim (pid %d) is running.\n", pid);
    } else {
      printf ("shim is stopped.\n");
    }
    exit (0);
  }
  if (strcmp (COMMAND,"test") == 0) {
    debug_args (mg_options);
    exit (0);
  }
  if (strcmp (COMMAND,"stop") == 0) {
      // if no pid file then no running shim
    f=fopen(PIDFILE,"r");
    if (f == NULL) {
      printf ("shim is already stopped.\n");
      exit (0);
    }
    fscanf(f,"%d", &pid);
    fclose(f);
    // non-existance of /proc/pid means its not running
    snprintf (pbuf, MAX_VARLEN, "/proc/%d", pid);
    if( access( pbuf, F_OK ) != 0 ) {
      printf ("shim is already stopped.\n");
      exit (0);
    }
    printf ("Stopping shim...\n");
    if (kill(pid,SIGKILL) == -1) {
      if (errno == EPERM) {
	printf ("Unable to stop shim (pid %d).\n", pid);
	printf ("This process does not have permission to kill the running shim.\n");
	exit (1);
      }
      printf ("Unable to stop shim (pid %d).\n", pid);
      exit (1);
    }
    exit (0);
  }
  if (strcmp (COMMAND,"restart") == 0) {
    // if pid file is present then shim maybe running
    f=fopen(PIDFILE,"r");
    if (f != NULL) {
      fscanf(f,"%d", &pid);
      fclose(f);
      // Existance of /proc/pid means shim is really running
      snprintf (pbuf, MAX_VARLEN, "/proc/%d", pid);
      if( access( pbuf, F_OK ) == 0 ) {
	// Now kill it
	if (kill(pid,SIGKILL) == -1) {
	  if (errno == EPERM) {
	    printf ("Unable to stop shim (pid %d).\n", pid);
	    printf ("This process does not have permission to kill the running shim.\n");
	    exit (1);
	  }
	  printf ("Unable to stop shim (pid %d).\n", pid);
	  exit (1);
	}
      }
    }
  // Now that shim is stopped
  // fall through to start
  }
  // Only command left is 'start'
  //
  // If daemonize is true
  // then check if pid file (actually the directory) is writeable by present process
  // dirname destroys the character array passed to it
  if (daemonize > 0)
    {
      char PIDFILEDIR[PATH_MAX];
      strcpy (PIDFILEDIR, PIDFILE);
      if( access( dirname(PIDFILEDIR), W_OK ) != 0 ) {
	printf ("ERROR: Unable to write pid file %s.\n", PIDFILE);
	exit (1);
      }
    }
  // START!
  if (stat (mg_options[5], &check_ssl) < 0)
    {
      /* Disable SSL  by removing any 's' port mg_options and getting rid of the ssl
       * mg_options.
       */
      syslog (LOG_ERR, "ERROR Disabling SSL, error reading %s", mg_options[5]);
      ports = cp = strdup (mg_options[1]);
      while ((cp = strchr (cp, 's')) != NULL)
        *cp++ = ',';
      mg_options[1] = ports;
      mg_options[4] = NULL;
      free (mg_options[5]);
      mg_options[5] = NULL;
    }
  docroot = mg_options[3];
  sessions = (session *) calloc (MAX_SESSIONS, sizeof (session));
  memset (&callbacks, 0, sizeof (callbacks));
  signal (SIGTERM, signalHandler);

  /* Daemonize */
  k = -1;
  if (daemonize > 0)
    {
      //
      struct passwd *pass;
      k = fork ();
      switch (k)
        {
        case -1:
          fprintf (stderr, "fork error: service terminated.\n");
          exit (1);
        case 0:
	  /* Close some open file descriptors */
          for (j = 0; j < 3; j++)
            (void) close (j);
          j = open ("/dev/null", O_RDWR);
          dup (j);
          dup (j);
	  // If the AS_USER parameter is not set then assume to run the daemon as the present user
	  if (AS_USER != NULL) {
	    // If the AS_USER parameter is set then will run daemon as the specified user.
	    // Set uid and gid to run child with.
	    pass = getpwnam(AS_USER);
	    if (!pass) {
	      printf("Unable to get user %s account information.", AS_USER);
	      exit (1);
	    }
	    setuid(pass->pw_uid);
	    setgid(pass->pw_gid);
	  }
          break;
        default:
	  /* Write out child PID */
	  j = open (PIDFILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	  if (j > 0)
	    {
	      sprintf (pbuf, "%d\n", (int)k);
	      write (j, pbuf, strlen (pbuf));
	      close (j);
	    }
          exit (0);
        }
    }

  openlog ("shim", LOG_CONS | LOG_NDELAY, LOG_USER);
  omp_init_lock (&biglock);
  for (j = 0; j < MAX_SESSIONS; ++j)
    {
      sessions[j].available = SESSION_AVAILABLE;
      strcpy(sessions[j].sessionid, "NA");
      omp_init_lock (&sessions[j].lock);
    }

  callbacks.begin_request = begin_request_handler;
  ctx = mg_start (&callbacks, NULL, (const char **) mg_options);
  if (!ctx)
    {
      syslog (LOG_ERR, "ERROR Failed to start web service");
      return -1;
    }
  syslog (LOG_INFO,
          "SciDB HTTP service started on port(s) %s with web root [%s], talking to SciDB on port %d",
          mg_get_option (ctx, "listening_ports"),
          mg_get_option (ctx, "document_root"), SCIDB_PORT);

  for (;;)
    sleep (100);
  omp_destroy_lock (&biglock);
  mg_stop (ctx);
  closelog ();
  if (mg_options[5])
    free (mg_options[5]);

  return 0;
}
