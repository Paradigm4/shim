#!/bin/bash

## --
## -- - HTTP Status Codes - --
## --

HOST=localhost
PORT=8088
SHIM_DIR=/tmp/shim
MYDIR=$(dirname $0)

# Get shim's absolute location
# This assumes it is one directory up from this script
pushd $MYDIR/../ > /dev/null 2>&1
SHIM=$(pwd)/shim
popd > /dev/null 2>&1

HTTP_AUTH=homer:elmo

CURL="curl --digest --user $HTTP_AUTH --write-out %{http_code} --silent"
NO_OUT="--output /dev/null"
SHIM_URL="http://$HOST:$PORT"
# SCIDB_AUTH="user=root&password=Paradigm4"

set -o errexit

function cleanup {
    ## Cleanup
    kill -s SIGKILL %1
    wait %1 2>/dev/null || true
    rm --recursive $SHIM_DIR
}

trap cleanup EXIT


## Setup
mkdir --parents $SHIM_DIR/wwwroot
echo $HTTP_AUTH > $SHIM_DIR/wwwroot/.htpasswd
$SHIM -c $MYDIR/conf -f start 2>/dev/null &
sleep 1


## 1. HTTP 400 Bad Request
err="HTTP arguments missing400"
# res=$($CURL "$SHIM_URL/upload_file")
# test "$res" == "$err"

res=$($CURL "$SHIM_URL/upload")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/execute_query")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/read_bytes")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/read_lines")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/cancel")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/release_session")
test "$res" == "$err"


## Special Cases
## - Prep
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

## Upload empty file
res=$($CURL --data-binary @- "$SHIM_URL/upload?id=$ID" \
            < /dev/null)
test "$res" == "Uploaded file is empty400"

## No query
res=$($CURL "$SHIM_URL/execute_query?id=$ID")
test "$res" == "$err"

## - Cleanup
res=$($CURL "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"


## 2. HTTP 401 Unauthorized
## - Shim
res=$(curl --write-out %{http_code} --silent "$SHIM_URL/version")
test "$res" == "401"

## - SciDB
if [ -n "$SCIDB_AUTH" ]
then
    cred="user=INVALID&password=INVALID"
    res=$($CURL "$SHIM_URL/new_session?$cred")
    test "$res" == "SciDB authentication failed401"
fi


## 3. HTTP 403 Forbidden
res=$($CURL "$SHIM_URL/.htpasswd")
test "$res" == "403"


## 4. HTTP 404 Not Found
err="Session not found404"
id_bad=INVALID

# res=$($CURL "$SHIM_URL/upload_file?id=$id_bad")
# test "$res" == "$err"

res=$($CURL "$SHIM_URL/upload?id=$id_bad")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/execute_query?id=$id_bad")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/read_bytes?id=$id_bad")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/read_lines?id=$id_bad")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/cancel?id=$id_bad")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/release_session?id=$id_bad")
test "$res" == "$err"

## Special Cases
## - Prep
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()")
test "$res" == "200"
res=$($CURL $NO_OUT "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"

res=$($CURL "$SHIM_URL/upload?id=$ID")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/execute_query?id=$ID")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/read_bytes?id=$ID")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/read_lines?id=$ID")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/cancel?id=$ID")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/release_session?id=$ID")
test "$res" == "$err"


## 5. HTTP 406 Not Acceptable
## - Prep
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)
que="INVALID"

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=$que")
test "$res" == "406"

res=$($CURL $NO_OUT \
            "$SHIM_URL/execute_query?id=$ID&prefix=$que&query=list()")
test "$res" == "406"

## - Cleanup
res=$($CURL "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"


## 6. HTTP 409 Conflict
## - Prep
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

res=$($CURL "$SHIM_URL/cancel?id=$ID")
test "$res" == "Session has no query409"

## - Cleanup
res=$($CURL "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"


## 7. HTTP 410 Gone
## - Prep
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()")
test "$res" == "200"
err="Output not saved410"

res=$($CURL "$SHIM_URL/read_bytes?id=$ID")
test "$res" == "$err"

res=$($CURL "$SHIM_URL/read_lines?id=$ID")
test "$res" == "$err"

## - Cleanup
res=$($CURL "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"


## 8. HTTP 416 Requested Range Not Satisfiable
## - Prep
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

err="EOF - range out of bounds416"
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()&save=csv")
test "$res" == "200"

res=$($CURL "$SHIM_URL/read_lines?id=$ID&n=10")
test "$res" == "$err"

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()&save=(string,int64,int64,string,bool,bool,string,string)")
test "$res" == "200"

res=$($CURL "$SHIM_URL/read_bytes?id=$ID&n=10")
test "$res" == "$err"

err="Output not saved in binary format416"
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()&save=csv")
test "$res" == "200"

res=$($CURL "$SHIM_URL/read_bytes?id=$ID")
test "$res" == "$err"

err="Output not saved in text format416"
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()&save=(string,int64,int64,string,bool,bool,string,string)")
test "$res" == "200"

res=$($CURL "$SHIM_URL/read_lines?id=$ID")
test "$res" == "$err"


## - Cleanup
res=$($CURL "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"


## 9. HTTP 503 Service Unavailable
## - Prep
for i in `seq 50`
do
    res=$($CURL $NO_OUT "$SHIM_URL/new_session?$SCIDB_AUTH")
    test "$res" == "200"
done

res=$($CURL "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "Out of resources503"


echo "PASS"
exit 0
