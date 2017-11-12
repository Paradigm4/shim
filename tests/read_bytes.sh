#!/bin/bash
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
./shim -p $port -r $td/wwwroot  -f  2>/dev/null &
sleep 1

id=$(curl -s "http://${host}:${port}/new_session" | tr -d '[\r\n]')
curl -f -s "http://${host}:${port}/execute_query?id=${id}&query=list('functions')&save=dcsv" >/dev/null || fail
x=1
while test $x -lt 40;do
  curl  -s "http://${host}:${port}/read_bytes?id=${id}&n=555" 2>/dev/null >/dev/null  || break
  x=$(($x + 1))
done
curl -f -s "http://${host}:${port}/release_session?id=${id}" >/dev/null  || fail
echo "OK"

rm -rf $td
kill -9 %1 >/dev/null 2>&1
