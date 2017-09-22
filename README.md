Shim is a super-basic SciDB client that exposes limited SciDB functionality
through a simple HTTP API. It's based on the mongoose web server.  It's a shim
between the low-level SciDB C API and a higher-level and lightweight web
service API.

## API Documentation 

See the [Documentation Pages](http://htmlpreview.github.io/?https://raw.github.com/Paradigm4/shim/master/wwwroot/help.html) for detailed nodes on release differences, configuration, authentication, encryption, limits and so on.

The shim program tracks SciDB releases because it uses the SciDB client API.
You need to use a version of shim that matches your SciDB release. You can checkout any previously released versions by git tag.

## Installation from binary packages

This is the fastest/easiest way to install shim as a system service. We provide some pre-built binary packages.

**SciDB on Ubuntu 14.04**

*  [Shim for SciDB 16.9](https://github.com/Paradigm4/shim/raw/gh-pages/shim_16.9_amd64.deb)
*  [Shim for SciDB 15.12](https://github.com/Paradigm4/shim/raw/gh-pages/shim_15.12_amd64.deb)
*  [Shim for SciDB 15.7](http://paradigm4.github.io/shim/ubuntu_14.04_shim_15.7_amd64.deb)

```sh
# Install with:
gdebi shim_16.9_amd64.deb

# Uninstall with (be sure to uninstall any existing copy before re-installing shim):
apt-get remove shim
```

**SciDB on RHEL/Centos 6**

* [Shim for SciDB 16.9](http://paradigm4.github.io/shim/shim-16.9-1.x86_64.rpm)
* [Shim for SciDB 15.12](http://paradigm4.github.io/shim/shim-15.12-1.x86_64.rpm)
* [Shim for SciDB 15.7](http://paradigm4.github.io/shim/shim-15.7-1.x86_64.rpm)

Packages for some older versions can be found at https://github.com/paradigm4/shim/tree/gh-pages

```sh
# shim depends on a few libraries. If installation fails you may need to:
yum install libgomp openssl-devel
# Install with:
rpm -i shim-16.9-1.x86_64.rpm

# Uninstall with:
yum remove shim
```

## LD_LIBRARY_PATH issues

By default shim installs into `/opt/scidb/16.9/bin` and expects the sibling directory `lib` to contain the `libscidbclient.so` library. This may present a problem if SciDB is installed in a different location. One way to go around the issue is by creating a symlink. For example:
```bash
## Problem:
$ sudo service shimsvc start
Starting shim
/opt/scidb/16.9/bin/shim: error while loading shared libraries: libscidbclient.so: cannot open shared object file: No such file or directory

## Solution: supposing SciDB was installed at ~/scidb
$ sudo ln -s ~/scidb/lib /opt/scidb/16.9/lib 
$ sudo service shimsvc start
Starting shim
```
You could also edit /etc/init.d/shimsvc or use other environment/path tricks.

## Configuring  shim

The `shim` service script consults the `/var/lib/shim/conf` file for
configuration options. The default configuration options are shown below,
and optional aut-configured values are indicated. Those values are set if
you install `shim` from a binary rpm or deb package.
```
auth=login
ports=8080,8083s
scidbhost=localhost
scidbport=1239 (or auto-configured by apt/yum to a local SciDB port)
user=root
tmp=/tmp  (or auto-configured by apt/yum to local SciDB storage directory)
auth=login
max_sessions=50
timeout=60
instance=0  (or auto-configured by apt/yum to a local SciDB instance ID)

```
If an option is missing from the config file, the default value will be used.
The options are:

* `auth` A PAM authentication method (limited to 'login' for now).
* `ports` A comma-delimited list of HTTP listening ports. Append the lowercase
letter 's' to indicate SSL encryption.
* `scidbhost` The host on which SciDB runs.
* `scidbport` The port to talk to SciDB on.
* `user` The user that the shim service runs under. Shim can run as a non-root
user, but then SSL authenticated port logins are limited to the user that shim
is running under.
* `tmp` Temporary I/O directory used on the server.
* `max_sessions` Maximum number of concurrent HTTP sessions.
* `timeout` Timeout after which an inactive HTTP session may be declared dead and reclaimed for use elsewhere.
* `instance` Which SciDB instance should save data to files or pipes? This instance must have write permission to the `tmp` directory.

Restart shim to effect option changes with `/etc/init.d/shimsvc restart`.

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

You can alternatively run `shim` from the command line and use command line
switches to set the configuration options. Run `shim -h` to see a full list
of options. When you run shim from a non-standard location, the program
expects to find the ssl_cert.pem file one directory above the wwwroot
directory.

## Usage
```
shim [-h] [-f] [-p <http port>] [-r <document root>] [-s <scidb port>]
```
where, -f means run in the foreground (defaults to background), -h means help.

If you installed the service version, then you can control when shim is running with the usual mechanism, for example:
```
/etc/init.d/shimsvc stop
/etc/init.d/shimsvc start
```

## Uninstall
We explicitly define our SCIDB home directory for Make in the example below:
```
sudo make SCIDB=/opt/scidb/16.9 uninstall
```

## Log files
Shim prints messages to the system log. The syslog file location varies, but can usually be found in /var/log/syslog or /var/log/messages.

## Manual Building
Note that because shim is a SciDB client it needs the boost, zlib, log4cpp and log4cxx development libraries installed to compile. And because shim now uses PAM authentication, you'll now need the PAM development libraries for your system installed too. You also optionally need an SSL development library if you want to support TLS. 

A good way to satisfy most of the dependencies is to install the SciDB Development Packages as described in the [dev_tools documentation](https://github.com/paradigm4/dev_tools#required-packages-scidb-169).

## Build and install
```
make
sudo make install

# Or, if SCIDB is not in the PATH, can set a Make variable SCIDB that points
# to the SCIDB home directory, for example for version 14.12:

make SCIDB=/opt/scidb/15.12
sudo make SCIDB=/opt/scidb/15.12 install

```
## Optionally install as a service
You can install shim as a system service so that it just runs all the time with:
```
sudo make SCIDB=/opt/scidb/16.9 service
```
If you install shim as a service and want to change its default options, for example the default HTTP port or port to talk to SciDB on, you'll need to edit the shim configuration file. See the discussion of command line parameters below.

### Optionally build deb or rpm packages
You can build the service version of shim into packages for Ubuntu 12.04 or RHEL/CentOS 6 with
```
make deb-pkg
make rpm-pkg
```
respectively. Building packages requires that certain extra packaging programs are available,
including rpmbuild for RHEL/CentOS and the Ruby-based fpm packaging utility on all systems.

