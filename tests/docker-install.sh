#!/bin/sh

set -o errexit

if [ `lsb_release --id | cut --fields=2` = "CentOS" ]
then
    # CentOS

    # TEST_BASIC=false
    if [ "$1" = "false" ]
    then
        yum install --assumeyes https://apache.bintray.com/arrow/centos/$(
            cut --delimiter : --fields 5 /etc/system-release-cpe
            )/apache-arrow-release-latest.rpm

        for pkg in arrow-devel-$ARROW_VER.el6   \
                   libpqxx-devel
        do
            yum install --assumeyes $pkg
        done
    fi
else
    apt-get update
    apt-get install                             \
        --assume-yes                            \
        --no-install-recommends                 \
        bc                                      \
        curl

    # TEST_BASIC=false
    if [ "$1" = "false" ]
    then
        id=`lsb_release --id --short`
        codename=`lsb_release --codename --short`
        wget https://apache.bintray.com/arrow/$(
            echo $id | tr 'A-Z' 'a-z'
             )/apache-arrow-archive-keyring-latest-$codename.deb
        apt install --assume-yes ./apache-arrow-archive-keyring-latest-$codename.deb

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
    make --directory=/shim/accelerated_io_tools-$SCIDB_VER.$AIO_VER
    cp /shim/accelerated_io_tools-$SCIDB_VER.$AIO_VER/libaccelerated_io_tools.so \
       /opt/scidb/$SCIDB_VER/lib/scidb/plugins/
    iquery --afl --query "load_library('accelerated_io_tools')"

    make --directory=/shim/superfunpack-$SCIDB_VER.1
    cp /shim/superfunpack-$SCIDB_VER.1/libsuperfunpack.so \
       /opt/scidb/$SCIDB_VER/lib/scidb/plugins/
    iquery --afl --query "load_library('superfunpack')"
fi

ln --symbolic /opt/scidb/$SCIDB_VER/lib/libscidbclient.so /lib/
