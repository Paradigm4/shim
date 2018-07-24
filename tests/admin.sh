#!/bin/bash

## --
## -- - Admin Session - --
## --

HOST=localhost
PORT=8088
HTTP_AUTH=homer:elmo

SHIM_DIR=$(mktemp --directory)
CURL="curl --digest --user $HTTP_AUTH --write-out %{http_code} --silent"
NO_OUT="--output /dev/null"
SHIM_URL="http://$HOST:$PORT"
# SCIDB_AUTH="user=root&password=Paradigm4"

set -o errexit

function cleanup {
    ## Cleanup
    kill -s SIGKILL %1
    rm --recursive $SHIM_DIR
}

trap cleanup EXIT


## Setup
mkdir --parents $SHIM_DIR/wwwroot
echo $HTTP_AUTH > $SHIM_DIR/wwwroot/.htpasswd
./shim -p $PORT -r $SHIM_DIR/wwwroot -f &
sleep 1


## Admin Session
## - Prep & Start #1
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=store(build(<x:double>\[i=1:10000;j=1:10000\],random()),test_admin)")
test "$res" == "200"

$CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=op_count(sort(test_admin))&release=1" &


## - Start #2
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

$CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=op_count(sort(test_admin))&release=1" &


## - Start #3
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

$CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=op_count(sort(test_admin))&release=1" &


## - Start #4
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

$CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=op_count(sort(test_admin))&release=1" &


## - No Admin
($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH" && res=0) &
sleep 5 && res=1
test "$res" == "1"


## - Admin & Cancel All
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?admin=1&$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=project(list('queries'),query_id)&save=csv")
test "$res" == "200"

res=$($CURL --output $SHIM_DIR/out "$SHIM_URL/read_lines?id=$ID")
test "$res" == "200"

sort $SHIM_DIR/out | uniq | while read QID
do
    res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=cancel($QID)")
    test "$res" == "200" -o "$res" == "406"
done


## - Cleanup
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=remove(test_admin)&release=1")
test "$res" == "200"


echo "PASS"
exit 0
