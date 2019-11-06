#!/bin/bash

## --
## -- - SciDB w/ authentication - --
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
ERR1=401
ERR2=$ERR1
ERR3=500
ERR4=$ERR3

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


## No credentials
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session")
test "$res" == "$ERR1"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()")
test "$res" == "$ERR3"


## Username only
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?user=root")
test "$res" == "$ERR1"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()")
test "$res" == "$ERR3"


## Password only
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?password=Paradigm4")
test "$res" == "$ERR1"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()")
test "$res" == "$ERR3"


## Incorrect username
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?user=rootWRONG&password=Paradigm4")
test "$res" == "$ERR2"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()")
test "$res" == "$ERR4"


## Incorrect password
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?user=root&password=Paradigm4WRONG")
test "$res" == "$ERR2"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()")
test "$res" == "$ERR4"


## Password then username
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?password=Paradigm4&user=root")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list()")
test "$res" == "200"


echo "PASS"
exit 0
