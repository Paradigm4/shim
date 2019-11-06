#!/bin/bash

HOST=localhost
PORT=8089
SHIM_DIR=/tmp/shim
MYDIR=$(dirname $0)

# Get shim's absolute location
# This assumes it is one directory up from this script
pushd $MYDIR/../ > /dev/null 2>&1
SHIM=$(pwd)/shim
popd > /dev/null 2>&1

HTTP_AUTH=homer:elmo

CURL="curl --insecure --digest --user $HTTP_AUTH --write-out %{http_code} --silent"
NO_OUT="--output /dev/null"
SHIM_URL="https://$HOST:$PORT"

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
openssl req                                                     \
    -new                                                        \
    -newkey rsa:4096                                            \
    -days 3650                                                  \
    -nodes                                                      \
    -x509                                                       \
    -subj "/C=US/ST=MA/L=Waltham/O=Paradigm4/CN=$(hostname)"    \
    -keyout $SHIM_DIR/ssl_cert.pem                              \
2> /dev/null                                                    \
>> $SHIM_DIR/ssl_cert.pem
$SHIM -c $MYDIR/conf -f start 2>/dev/null &
sleep 1


## 1. Version
res=$($CURL $NO_OUT "$SHIM_URL/version")
test "$res" == "200"


## 2. Execute query with save
## - Prep
res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)
res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=list('functions')&save=dcsv")
test "$res" == "200"

res=$($CURL $NO_OUT "$SHIM_URL/read_lines?id=$ID")
test "$res" == "200"

## - Cleanup
res=$($CURL "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"


## Extra
READ_MAX=250
source `dirname "${BASH_SOURCE[0]}"`/read-long.sh


echo "PASS"
exit 0
