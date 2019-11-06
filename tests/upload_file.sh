#!/bin/bash
# Test uploading binary data through shim using old multipart file
# API /upload_file

host=localhost
port=8088
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
echo "homer:elmo" > $SHIM_DIR/wwwroot/.htpasswd
$SHIM -c $MYDIR/conf -f start 2>/dev/null &
sleep 1

t1=$(date +"%s.%N")
id=$(curl -f -s --digest --user homer:elmo "http://${host}:${port}/new_session" | sed -e "s/.*//")
dd if=/dev/zero bs=1M count=50 2>/dev/null  | curl -f -s --digest --user homer:elmo --form "fileupload=@-;filename=data" "http://${host}:${port}/upload_file?id=${id}"  >/dev/null || fail
curl -f -s --digest --user homer:elmo "http://${host}:${port}/release_session?id=${id}" >/dev/null || fail
t2=$(date +"%s.%N")
x=$(echo "50 / ($t2 - $t1)" | bc)


echo "PASS (about ${x} MB/s)"
kill -s SIGKILL %1
wait %1 2>/dev/null || true
rm --recursive $SHIM_DIR
exit 0
