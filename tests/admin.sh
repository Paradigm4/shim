#!/bin/bash

## --
## -- - Admin Session - --
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

CURL="curl --digest --user $HTTP_AUTH --write-out %{http_code} --silent"
NO_OUT="--output /dev/null"
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


## Admin Session
## - Prep & Start #1
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

res=$($CURL                                                                                                           \
          $NO_OUT                                                                                                     \
          "$SHIM_URL/execute_query?id=$ID&query=store(build(<x:double>\[i=1:10000;j=1:10000\],random()),test_admin)")
test "$res" == "200"

$CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=op_count(sort(test_admin))" \
      > /dev/null &


## - Start #2..10
for i in `seq 2 10`
do
    echo -n "Overloading $i..."
    res=$(timeout 5 \
                  $CURL \
                  --output $SHIM_DIR/id \
                  "$SHIM_URL/new_session?$SCIDB_AUTH") \
        || true
    test "$res" == "200" -o "$res" == ""

    if [ "$res" == "200" ]
    then
        echo "PASS"
        ID=$(<$SHIM_DIR/id)
        $CURL                                                                   \
            $NO_OUT                                                             \
            "$SHIM_URL/execute_query?id=$ID&query=op_count(sort(test_admin))"   \
            > /dev/null &
    else
        echo "TIMEOUT"
        break
    fi
done


## - No Admin
echo -n "Cancel regular..."
res=1
timeout 5 $CURL "$SHIM_URL/new_session?$SCIDB_AUTH" > /dev/null \
    || res=0
test "$res" == "0"
echo "TIMEOUT"


## - Admin & Cancel All
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?admin=1&$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

while true
do
    res=$($CURL                                                                                                                                    \
              $NO_OUT                                                                                                                              \
              "$SHIM_URL/execute_query?id=$ID&query=project(filter(list('queries'),query_string='op_count(sort(test_admin))'),query_id)&save=csv")
    test "$res" == "200"

    res=$($CURL --output $SHIM_DIR/out "$SHIM_URL/read_lines?id=$ID")
    test "$res" == "200"

    while read QID
    do
        echo -n "Cancel admin $QID..."
        res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=cancel($QID)")
        test "$res" == "200" -o "$res" == "406"
        echo "PASS"
    done < $SHIM_DIR/out

    if [ `wc --lines $SHIM_DIR/out | cut --delimiter=" " --fields=1` -eq 0 ]
    then
        break
    fi

    > $SHIM_DIR/out
    sleep 5
done


## - Cleanup
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=remove(test_admin)")
test "$res" == "200"
res=$($CURL $NO_OUT "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"


echo "PASS"
exit 0
