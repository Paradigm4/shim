#!/bin/bash
# /var/lib/shim/conf
#
# This script is run to setup the initial shim configuration file.

mkdir -p /var/lib/shim
# Set up config file defaults
PORT=1239
INS=0
TMP=/tmp
s=`ps aux | grep SciDB  | grep "dbname" | head -n 1`
if test -n "$s"; then
  PORT=`echo "$s" | sed -e "s/.*--port //;s/ .*//"`
  INS=`echo "$s" | sed -e "s/.*--storage //;s/ .*//" | sed -e "s@.*/[0-9]*/\([0-9]*\)/.*@\1@"`
  INS=$(( $INS ))
  TMP=`echo "$s" | sed -e "s/.*--storage //;s/ .*//"`
  TMP=`dirname $TMP`
  SCIDBUSER=`ps axfo user:64,cmd | grep SciDB | grep dbname | head -n 1 | cut -d ' ' -f 1`
# Write out an example config file to /var/lib/shim/conf using running scidb parameters
cat >/var/lib/shim/conf << EOF
# Shim configuration file
# Uncomment and change any of the following values. Restart shim for
# your changes to take effect (default values are shown). See
# man shim
# for more information on the options.

#ports=8080,8083s
#scidbhost=localhost
scidbport=$PORT
instance=$INS
tmp=$TMP
user=$SCIDBUSER
#max_sessions=50
#timeout=60
#aio=0
EOF
else
# Write out an example config file to /var/lib/shim/conf completely commented out
cat >/var/lib/shim/conf << EOF
# Shim configuration file
# Uncomment and change any of the following values. Restart shim for
# your changes to take effect (default values are shown). See
# man shim
# for more information on the options.

#ports=8080,8083s
#scidbhost=localhost
#scidbport=1239
#instance=0
#tmp=/storage/database/0/0/tmp
#user=scidb
#max_sessions=50
#timeout=60
#aio=0
EOF
fi

# Generate a certificate
openssl req -new -newkey rsa:4096 -days 3650 -nodes -x509 -subj "/C=US/ST=MA/L=Waltham/O=Paradigm4/CN=$(hostname)" -keyout /var/lib/shim/ssl_cert.pem 2>/dev/null >> /var/lib/shim/ssl_cert.pem
if test $? -ne 0; then
  echo "SSL certificate generation failed openssl not found: TLS disabled."
  rm -f /var/lib/shim/ssl_cert.pem
fi
