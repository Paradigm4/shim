#!/bin/bash
#
# This script is run to setup the initial shim configuration file and
# certificate file. If the configuration file or the certificate file
# already exist they are *not* re-created.

CONF_FILE=/var/lib/shim/conf
CERT_FILE=/var/lib/shim/ssl_cert.pem

if [ -e "$CONF_FILE" ]
then
    echo "$CONF_FILE file exists, skip re-creating it."
else
    mkdir -p /var/lib/shim
    # Set up config file defaults
    PORT=1239
    INS=0
    TMP=/tmp
    s=`ps aux | grep SciDB  | grep "dbname" | head -n 1`
    if test -n "$s"
    then
        PORT=`echo "$s" | sed -e "s/.*--port //;s/ .*//"`
        INS=`echo "$s" | sed -e "s/.*--storage //;s/ .*//" | sed -e "s@.*/[0-9]*/\([0-9]*\)/.*@\1@"`
        INS=$(( $INS ))
        TMP=`echo "$s" | sed -e "s/.*--storage //;s/ .*//"`
        TMP=`dirname $TMP`
        SCIDBUSER=`ps axfo user:64,cmd | grep SciDB | grep dbname | head -n 1 | cut -d ' ' -f 1`
        # Write out an example config file to $CONF_FILE using running SciDB parameters
        cat > "$CONF_FILE" << EOF
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
        # Write out an example config file to $CONF_FILE completely commented out
        cat > "$CONF_FILE" << EOF
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
fi

if [ -e "$CERT_FILE" ]
then
    echo "$CERT_FILE file exists, skip re-creating it."
else
    # Generate a certificate
    openssl req                                                         \
            -new                                                        \
            -newkey rsa:4096                                            \
            -days 3650                                                  \
            -nodes                                                      \
            -x509                                                       \
            -subj "/C=US/ST=MA/L=Waltham/O=Paradigm4/CN=$(hostname)"    \
            -keyout "$CERT_FILE"                                        \
            2> /dev/null                                                \
            >> "$CERT_FILE"
    if test $? -ne 0; then
        echo "SSL certificate generation failed openssl not found: TLS disabled."
        rm -f "$CERT_FILE"
    fi
fi
