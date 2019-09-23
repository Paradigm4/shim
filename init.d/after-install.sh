#!/bin/bash
# shim
# package post installation script
# This script is run by the package management system after the shim
# package is installed.

chmod 0755 /etc/init.d/shimsvc
mkdir -p /var/lib/shim
# Set up config file defaults
PORT=1239
INS=0
TMP=/tmp
s=`ps aux | grep scidb  | grep "dbname" | head -n 1`
if test -n "$s"; then
  PORT=`echo "$s" | sed -e "s/.*-p //;s/ .*//"`
  INS=`echo "$s" | sed -e "s/.*-s //;s/ .*//" | sed -e "s@.*/[0-9]*/\([0-9]*\)/.*@\1@"`
  INS=$(( $INS ))
  TMP=`echo "$s" | sed -e "s/.*-s //;s/ .*//"`
  TMP=`dirname $TMP`
  SCIDBUSER=`ps axfo user:64,cmd | grep scidb | grep dbname | head -n 1 | cut -d ' ' -f 1`
fi
# Write out an example config file to /var/lib/shim/conf
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
#aio=1
EOF

# Generate a certificate
openssl req -new -newkey rsa:4096 -days 3650 -nodes -x509 -subj "/C=US/ST=MA/L=Waltham/O=Paradigm4/CN=$(hostname)" -keyout /var/lib/shim/ssl_cert.pem 2>/dev/null >> /var/lib/shim/ssl_cert.pem
if test $? -ne 0; then
  echo "SSL certificate generation failed openssl not found: TLS disabled."
  rm -f /var/lib/shim/ssl_cert.pem
fi
if test -n "$(which systemctl 2>/dev/null)"; then
# SystemD
  find /opt/scidb/ -name shim_systemd -exec {} \;
elif test -n "$(which update-rc.d 2>/dev/null)"; then
# Ubuntu
  update-rc.d shimsvc defaults
  /etc/init.d/shimsvc start
elif test -n "$(which chkconfig 2>/dev/null)"; then
# RHEL sysV
  chkconfig --add shimsvc && chkconfig shimsvc on
  /etc/init.d/shimsvc start
fi
