# shim

[![SciDB 19.3](https://img.shields.io/badge/SciDB-19.3-blue.svg)](https://forum.paradigm4.com/t/scidb-release-19-3/2359)
[![Build Status](https://travis-ci.org/Paradigm4/shim.svg)](https://travis-ci.org/Paradigm4/shim)

Shim is a super-basic SciDB client that exposes limited SciDB functionality
through a simple HTTP API. It's based on the mongoose web server.  It's a shim
between the low-level SciDB C API and a higher-level and lightweight web
service API.

## API Documentation

See the [Help page](http://paradigm4.github.io/shim/help.html) for detailed nodes on release differences, configuration, authentication, encryption, limits and so on.

The shim program tracks SciDB releases because it uses the SciDB client API.
You need to use a version of shim that matches your SciDB release. You can checkout any previously released versions by git tag.

## Installation from binary packages

This is the fastest/easiest way to install shim as a system service. We provide some pre-built binary packages.

**SciDB on Ubuntu 14.04**

[Download](http://paradigm4.github.io/shim/#ubuntu)

```sh
# Install with:
gdebi shim_19.3_amd64.deb

# Uninstall with (be sure to uninstall any existing copy before re-installing shim):
apt-get remove shim
```

**SciDB on RHEL/Centos 6**

[Downloads](http://paradigm4.github.io/shim/#red-hat-enterprise-linus-and-centos)

```sh
# shim depends on a few libraries. If installation fails you may need to:
yum install libgomp openssl-devel
# Install with:
rpm -i shim-19.3-1.x86_64.rpm

# Uninstall with:
yum remove shim
```

## LD_LIBRARY_PATH issues

By default shim installs into `/opt/scidb/19.3/bin` and expects the sibling directory `lib` to contain the `libscidbclient.so` library. This may present a problem if SciDB is installed in a different location. One way to go around the issue is by creating a symlink. For example:
```bash
## Problem:
$ sudo service shimsvc start
Starting shim
/opt/scidb/19.3/bin/shim: error while loading shared libraries: libscidbclient.so: cannot open shared object file: No such file or directory

## Solution: supposing SciDB was installed at ~/scidb
$ sudo ln -s ~/scidb/lib /opt/scidb/19.3/lib
$ sudo service shimsvc start
Starting shim
```
You could also edit /etc/init.d/shimsvc, or /lib/systemd/system/shim.service, or use other environment/path tricks.

## Configuring shim

The `shim` service reads the `/var/lib/shim/conf` file for configuration options.
The default configuration options are shown commented out below.
Some of these options are configured if you install `shim` from a binary rpm or deb package,
taking their values from the running SciDB.
```
#ports=8080,8083s
#scidbhost=localhost
#scidbport=1239 (or configured by apt/yum to a local SciDB port)
#tmp=/tmp (or configured by apt/yum to the local SciDB storage)
#user=scidb (or configured by apt/yum to the local SciDB user)
#max_sessions=50
#timeout=60
#instance=0 (or configured by apt/yum to a local SciDB instance ID)
#aio=0


```
If an option is missing from the config file, the default value will be used.
The options are:

* `ports` A comma-delimited list of HTTP listening ports. Append the lowercase
letter 's' to indicate SSL encryption.
* `scidbhost` The host on which SciDB runs.
* `scidbport` The port to talk to SciDB on.
* `tmp` Temporary I/O directory used on the server.
* `user` The user that the shim service runs under. Shim can run as a non-root
user, but then SSL authenticated port logins are limited to the user that shim
is running under.
* `max_sessions` Maximum number of concurrent HTTP sessions.
* `timeout` Timeout after which an inactive HTTP session may be declared dead and reclaimed for use elsewhere.
* `instance` Which SciDB instance should save data to files or pipes? This instance must have write permission to the `tmp` directory.
* `aio` Enable fast save using the SciDB accelerated_io_tools (AIO) plugin.

Restart shim to effect option changes with either `service shimsvc restart` or `systemctl restart shimsvc`, whichever is appropriate.

## Note on the SSL Key Certificate Configuration

Shim uses a cryptographic key certificate for SSL encrypted web connections.
When you instal shim from a binary package, a new certificate key is
dynamically generated and stored in `/var/lib/shim/ssl_cert.pem`. Feel free to
replace the certificate with one of your own. You should then also set the
permissions of the `/var/lib/shim/ssl_cert.pem` file to restrict all read and
write access to the user that shim is running under.  Restricting access
permissions to the SSL certificate is particularly important for general
machines with many untrusted users (an unlikely setting for an installation of
SciDB).

## Usage
```
shim [-h] [-f]
```
where, -f means run in the foreground (defaults to background), -h means help.

If you installed the service version, then you can control when shim is running with the appropriate mechanism, for example:
```
service shimsvc stop
service shimsvc start

or

systemctl shimsvc stop
systemctl shimsvc start
```

## Log files
Shim prints messages to the system log. The syslog file location varies, but can usually be found in /var/log/syslog or /var/log/messages.

## Manual Building
Note that because shim is a SciDB client it needs the boost, zlib, log4cpp and log4cxx development libraries installed to compile. You also optionally need an SSL development library if you want to support TLS.

A good way to satisfy most of the dependencies is to install the SciDB Development Packages as described in the [dev_tools documentation](https://github.com/paradigm4/dev_tools#required-packages-scidb-181).

## Build and install
```
make
sudo make install

# Or, if SCIDB is not in the PATH, can set a Make variable SCIDB that points
# to the SCIDB home directory, for example for version 19.3:

make SCIDB=/opt/scidb/19.3
sudo make SCIDB=/opt/scidb/19.3 install

```
## Uninstall
We explicitly define our SCIDB home directory for Make in the example below:
```
sudo make SCIDB=/opt/scidb/19.3 uninstall
```

## Optionally install as a service
You can install shim as a system service so that it just runs all the time with:
```
sudo make SCIDB=/opt/scidb/19.3 service
```
If you install shim as a service and want to change its default options, for example the default HTTP port or port to talk to SciDB on, you'll need to edit the shim configuration file.
See the discussion of Configuring shim above.

### Optionally build deb or rpm packages
You can build the service version of shim into packages for Ubuntu or RHEL/CentOS with
```
make deb-pkg
make rpm-pkg
```
respectively. Building packages requires that certain extra packaging programs are available,
including rpmbuild for RHEL/CentOS and the Ruby-based fpm packaging utility on all systems.
