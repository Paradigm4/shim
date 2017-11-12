#!/bin/bash

## --
## -- - /get_log - --
## --

HOST=localhost
PORT=8088
HTTP_AUTH=homer:elmo

SHIM_DIR=$(mktemp --directory)
CURL="curl --digest --user $HTTP_AUTH --fail --silent"
SHIM_URL="http://$HOST:$PORT"

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


## Get Log
$CURL "$SHIM_URL/get_log" \
      > $SHIM_DIR/out
test `wc -l $SHIM_DIR/out \
      | cut --delimiter=" " --fields=1` -gt 10


echo "PASS"
exit 0
