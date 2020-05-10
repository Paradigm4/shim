#!/bin/sh

set -o errexit

if [ `lsb_release --id | cut --fields=2` = "CentOS" ]
then
    # CentOS

    # TEST_BASIC=false
    if [ "$1" = "false" ]
    then
        cat <<EOF | tee /etc/yum.repos.d/scidb-extra.repo
[scidb-extra]
name=SciDB extra libs repository
baseurl=https://downloads.paradigm4.com/extra/$SCIDB_VER/centos7
gpgcheck=0
enabled=1
EOF

        for pkg in arrow-devel-$ARROW_VER.el6   \
                   libpqxx-devel
        do
            yum install --assumeyes $pkg
        done
    fi
else
    # Ubuntu/Debian
    echo "deb https://downloads.paradigm4.com/ extra/$SCIDB_VER/ubuntu16.04/" \
         >> /etc/apt/sources.list.d/scidb.list

    apt-get update
    apt-get install                             \
        --assume-yes                            \
        --no-install-recommends                 \
        bc                                      \
        curl

    # TEST_BASIC=false
    if [ "$1" = "false" ]
    then
        apt-get install                         \
            --assume-yes                        \
            --no-install-recommends             \
            libarrow-dev=$ARROW_VER             \
            libpcre3-dev                        \
            libpqxx-dev
    fi
fi

# TEST_BASIC=false
if [ "$1" = "false" ]
then
    make --directory=/shim/accelerated_io_tools-$SCIDB_VER.2
    cp /shim/accelerated_io_tools-$SCIDB_VER.2/libaccelerated_io_tools.so \
       /opt/scidb/$SCIDB_VER/lib/scidb/plugins/
    iquery --afl --query "load_library('accelerated_io_tools')"

    make --directory=/shim/superfunpack-$SCIDB_VER.1
    cp /shim/superfunpack-$SCIDB_VER.1/libsuperfunpack.so \
       /opt/scidb/$SCIDB_VER/lib/scidb/plugins/
    iquery --afl --query "load_library('superfunpack')"
fi

ln --symbolic /opt/scidb/$SCIDB_VER/lib/libscidbclient.so /lib/
