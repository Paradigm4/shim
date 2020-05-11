#!/bin/bash

## --
## -- - /cancel - --
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
SCIDB_AUTH="user=root&password=Paradigm4"
DELAY="build(<x:int64>\\[i=0:0\\],sleep(20))"

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
$SHIM -c $MYDIR/conf$AIO -f start 2>/dev/null &
sleep 1


## 1. Cancel: no session id
res=$($CURL "$SHIM_URL/cancel")
test "$res" == "HTTP arguments missing400"


## 2. Cancel: invalid session id
res=$($CURL "$SHIM_URL/cancel?id=INVALID")
test "$res" == "Session not found404"


## Get session
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)


## Run query
$CURL "$SHIM_URL/execute_query?id=$ID&query=$DELAY&save=csv" \
    || true                                                  \
    &
sleep 2


## 3. Cancel: OK
res=$($CURL "$SHIM_URL/cancel?id=$ID")
test "$res" == "200"
sleep 2


## 4. Query Not Present in List Queries
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=filter(list('queries'),substr(query_string,0,7)='cancel(')&save=csv")
test "$res" == "200"

> $SHIM_DIR/out
res=$($CURL --output $SHIM_DIR/out "$SHIM_URL/read_lines?id=$ID")
test "$res" == "200"

test `wc --lines $SHIM_DIR/out | cut --delimiter=" " --fields=1` -eq 0

echo ""
echo "PASS"
exit 0
