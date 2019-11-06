#!/bin/bash
# Test uploading binary data through shim using /upload

host=localhost
port=8088
SHIM_DIR=/tmp/shim
MYDIR=$(dirname $0)

# Get shim's absolute location
# This assumes it is one directory up from this script
pushd $MYDIR/../ > /dev/null 2>&1
SHIM=$(pwd)/shim
popd > /dev/null 2>&1

bs=10M
count=5
credentials=homer:elmo

if [ ! -z ${credentials+x} ]
then
    curl_auth="--digest --user $credentials"
else
    curl_auth=""
fi
curl_url="http://${host}:${port}"


function fail {
  echo "FAIL"
  kill -s SIGKILL %1
  wait %1 2>/dev/null || true
  rm --recursive $SHIM_DIR
  exit 1
}


mkdir -p $SHIM_DIR/wwwroot
if [ ! -z ${credentials+x} ]
then
    echo "$credentials" > $SHIM_DIR/wwwroot/.htpasswd
fi
$SHIM -c $MYDIR/conf -f start 2>/dev/null &
sleep 1


t1=$(date +"%s.%N")
id=$(curl -f -s $curl_auth "$curl_url/new_session" \
         | sed -e "s/.*//")


## Upload 1
fn=$(dd if=/dev/zero bs=$bs count=$count          \
        2> /dev/null                              \
         | curl -f -s $curl_auth --data-binary @- \
                "$curl_url/upload?id=${id}"       \
         | sed -e "s/.*//")
t2=$(date +"%s.%N")
curl -f -s $curl_auth                                                                                                \
     "$curl_url/execute_query?id=${id}&query=aggregate(input(<x:int64>\[i\],'${fn}',0,'(int64)'),count(*))&save=csv" \
     > /dev/null                                                                                                     \
    || fail
u1=$(curl -f -s $curl_auth                \
          "$curl_url/read_lines?id=${id}" \
         | sed -e "s/.*//")


## Upload 2
fn=$(dd if=/dev/zero bs=$bs count=$count          \
        2> /dev/null                              \
         | curl -f -s $curl_auth --data-binary @- \
                "$curl_url/upload?id=${id}"       \
         | sed -e "s/.*//")
curl -f -s $curl_auth                                                                                                \
     "$curl_url/execute_query?id=${id}&query=aggregate(input(<x:int64>\[i\],'${fn}',0,'(int64)'),count(*))&save=csv" \
     > /dev/null                                                                                                     \
    || fail
u2=$(curl -f -s $curl_auth                \
          "$curl_url/read_lines?id=${id}" \
         | sed -e "s/.*//")
test "$u1" -eq "$u2" || fail


curl -f -s $curl_auth "$curl_url/release_session?id=${id}" >/dev/null || fail
x=$(echo "${bs%?} * $count / ($t2 - $t1)" | bc)

echo "PASS (about ${x} MB/s)"
kill -s SIGKILL %1
wait %1 2>/dev/null || true
rm --recursive $SHIM_DIR
exit 0
