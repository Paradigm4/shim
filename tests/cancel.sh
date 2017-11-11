#!/bin/bash

# Test /cancel endpoint

HOST=localhost
PORT=8088
HTTP_AUTH=homer:elmo

SHIM_DIR=$(mktemp --directory)
CURL_AUTH="--digest --user $HTTP_AUTH"
SHIM_URL="http://$HOST:$PORT"
# SCIDB_AUTH="username=root&password=Paradigm4"
DELAY="build(<x:int64>\\[i=0:0\\],sleep(2))"

set -o errexit

function cleanup {
    ## Cleanup
    kill -9 %1
    rm --recursive $SHIM_DIR
}

trap cleanup EXIT


## Setup

mkdir --parents $SHIM_DIR/wwwroot
echo $HTTP_AUTH > $SHIM_DIR/wwwroot/.htpasswd
./shim -p $PORT -r $SHIM_DIR/wwwroot -f &
sleep 1


## Get session
ID=$(curl --silent $CURL_AUTH                 \
          "$SHIM_URL/new_session?$SCIDB_AUTH" \
         | sed -e "s/.*//")


## Run query
curl $CURL_AUTH                                             \
     "$SHIM_URL/execute_query?id=$ID&query=$DELAY&save=csv" \
    || true                                                 \
    &
sleep 1


## Cancel query
curl $CURL_AUTH "$SHIM_URL/cancel?id=$ID"


echo "PASS"
exit 0
