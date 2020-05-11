#!/bin/bash

## --
## -- - /get_log - --
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

CURL="curl --digest --user $HTTP_AUTH --fail --silent"
SHIM_URL="http://$HOST:$PORT"

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


## Get Log
$CURL "$SHIM_URL/get_log" \
      > $SHIM_DIR/out
test `wc -l $SHIM_DIR/out \
      | cut --delimiter=" " --fields=1` -gt 10


echo "PASS"
exit 0
