#!/bin/bash

set -o xtrace -o errexit

./scidb_auth.sh
./scidb_plain_auth.sh
./scidb_session_auth.sh
./scidb_session_auth_multi.sh
