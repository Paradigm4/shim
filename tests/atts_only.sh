#!/bin/bash

## --
## -- - atts_only - --
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

SHIM_OUT=$SHIM_DIR/out
SHIM_BIN=$SHIM_DIR/bin

CURL="curl --digest --user $HTTP_AUTH --write-out %{http_code} --silent"

NO_OUT="--output /dev/null"
FILE_OUT="--output $SHIM_OUT"
BIN_OUT="--output $SHIM_BIN"
BIN_UP="--data-binary @$SHIM_BIN"

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


## binary/arrow, atts_only
## - Prep
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)


## - binary
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build(<x:double>\[i=1:10;j=1:10\],random())&save=(double%20null)")
test "$res" == "200"

res=$($CURL $BIN_OUT "$SHIM_URL/read_bytes?id=$ID")
test "$res" == "200"

res=$($CURL $FILE_OUT $BIN_UP "$SHIM_URL/upload?id=$ID")
test "$res" == "200"

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=input(<x:double>\[i\],'$(cat $SHIM_OUT)',0,'(double%20null)')&save=(double%20null)")
test "$res" == "200"


## - binary, atts_only=1
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build(<x:double>\[i=1:10;j=1:10\],random())&save=(double%20null)&atts_only=1")
test "$res" == "200"

res=$($CURL $BIN_OUT "$SHIM_URL/read_bytes?id=$ID")
test "$res" == "200"

res=$($CURL $FILE_OUT $BIN_UP "$SHIM_URL/upload?id=$ID")
test "$res" == "200"

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=input(<x:double>\[i\],'$(cat $SHIM_OUT)',0,'(double%20null)')&save=(double%20null)")
test "$res" == "200"


## - binary, atts_only=0
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build(<x:double>\[i=1:10;j=1:10\],random())&save=(double%20null,int64,int64)&atts_only=0")
test "$res" == "200"

res=$($CURL $BIN_OUT "$SHIM_URL/read_bytes?id=$ID")
test "$res" == "200"

res=$($CURL $FILE_OUT $BIN_UP "$SHIM_URL/upload?id=$ID")
test "$res" == "200"

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=input(<x:double,y:int64%20not%20null,z:int64%20not%20null>\[i\],'$(cat $SHIM_OUT)',0,'(double%20null,int64,int64)')&save=(double%20null,int64,int64)")
test "$res" == "200"


## - arrow
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build(<x:double>\[i=1:10;j=1:10\],random())&save=arrow")
test "$res" == "200"


## - arrow, atts_only=1
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build(<x:double>\[i=1:10;j=1:10\],random())&save=arrow&atts_only=1")
test "$res" == "200"


## - arrow, atts_only=0
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build(<x:double>\[i=1:10;j=1:10\],random())&save=arrow&atts_only=0")
test "$res" == "200"

## - dcsv, atts_only=0
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build(<x:double>\[i=1:10;j=1:10\],random())&save=dcsv&atts_only=0")
test "$res" == "200"


## - Cleanup
res=$($CURL $NO_OUT "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"


echo "PASS"
exit 0
