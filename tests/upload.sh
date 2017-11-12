#!/bin/bash
# Test uploading binary data through shim using /upload

host=localhost
port=8088
td=$(mktemp -d)

function fail {
  echo "FAIL"
  rm -rf $td
  kill -9 %1
  exit 1
}

mkdir -p $td/wwwroot
echo "homer:elmo" > $td/wwwroot/.htpasswd
./shim -t /dev/shm/ -p $port -r $td/wwwroot  -f &
sleep 1

t1=$(date +"%s.%N")
id=$(curl -f -s --digest --user homer:elmo "http://${host}:${port}/new_session" | sed -e "s/.*//")
dd if=/dev/zero bs=100M count=5 2>/dev/null | curl -f -s --digest --user homer:elmo --data-binary @-  "http://${host}:${port}/upload?id=${id}"  >/dev/null || fail
curl -f -s --digest --user homer:elmo "http://${host}:${port}/release_session?id=${id}" >/dev/null || fail
t2=$(date +"%s.%N")


x=$(echo "500 / ($t2 - $t1)" | bc)
echo "OK (about ${x} MB/s)"
rm -rf $td
kill -9 %1 >/dev/null 2>&1
