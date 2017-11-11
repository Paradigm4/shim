#!/bin/bash
# Test canceling a query
# API /cancel

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
./shim -p $port -r $td/wwwroot  -f &
sleep 1

id=$(curl -f -s --digest --user homer:elmo "http://${host}:${port}/new_session?username=root&password=Paradigm4" | sed -e "s/.*//")
curl -f -s --digest --user homer:elmo "http://${host}:${port}/execute_query?id=${id}&query=build(<x:int64>\\[i=0:0\\],sleep(2))&save=csv" &
sleep 1
curl -f -s --digest --user homer:elmo "http://${host}:${port}/cancel?id=${id}" >/dev/null || fail

echo "OK"
rm -rf $td
kill -9 %1 >/dev/null 2>&1
