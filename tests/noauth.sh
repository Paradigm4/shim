#!/bin/bash

HOST=localHOST
PORT=8088
SHIM_DIR=/tmp/shim
MYDIR=$(dirname $0)

# Get shim's absolute location
# This assumes it is one directory up from this script
pushd $MYDIR/../ > /dev/null 2>&1
SHIM=$(pwd)/shim
popd > /dev/null 2>&1

function fail {
  echo "FAIL"
  kill -s SIGKILL %1
  wait %1 2>/dev/null || true
  rm --recursive $SHIM_DIR
  exit 1
}

mkdir -p $SHIM_DIR/wwwroot
$SHIM -c $MYDIR/conf -f start 2>/dev/null &
sleep 1

id=$(curl -s "http://${HOST}:${PORT}/new_session" | tr -d '[\r\n]')
curl -f -s "http://${HOST}:${PORT}/execute_query?id=${id}&query=list('functions')&save=dcsv" >/dev/null || fail
curl -f -s "http://${HOST}:${PORT}/read_lines?id=${id}&n=0" >/dev/null  || fail
curl -f -s "http://${HOST}:${PORT}/release_session?id=${id}" >/dev/null  || fail

echo "PASS"
kill -s SIGKILL %1
wait %1 2>/dev/null || true
rm --recursive $SHIM_DIR
exit 0
