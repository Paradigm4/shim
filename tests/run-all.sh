#!/bin/bash

set -o xtrace -o errexit

cd `dirname "${BASH_SOURCE[0]}"`/..

./tests/digest_auth.sh
./tests/noauth.sh
./tests/post_upload.sh
./tests/readbytes.sh
./tests/tls.sh
./tests/tls_digest.sh
./tests/tls_scidbauth.sh
./tests/upload.sh
