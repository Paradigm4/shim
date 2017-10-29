#!/bin/bash

set -o xtrace -o errexit

./scidb_auth_example.sh
./scidb_plain_auth_example.sh
./scidb_session_auth_example.sh
./scidb_session_auth_multi_example.sh
