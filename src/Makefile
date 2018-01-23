ifeq ($(SCIDB),)
  X := $(shell which scidb 2>/dev/null)
  ifneq ($(X),)
    X := $(shell dirname ${X})
    SCIDB := $(shell dirname ${X})
  endif
endif
VERSION := $(shell scidb --version | head -n 1 | cut -d ':' -f 2 |tr -d ' ')
COMMIT := $(shell git rev-list --abbrev-commit -1 HEAD)

CFLAGS=-std=gnu99 -fopenmp -g -pedantic -DVERSION=\"$(VERSION)\" -DCOMMIT=\"$(COMMIT)\"
INC=-I. -DPROJECT_ROOT="\"$(SCIDB)\"" -I"$(SCIDB)/3rdparty/boost/include/" -I"$(SCIDB)/include" -I"$(SCIDB)/3rdparty/boost/include/boost/container" -DSCIDB_CLIENT
LIBS=-lstdc++ -ldl -lz -pthread -L"$(SCIDB)/3rdparty/boost/lib" -L"$(SCIDB)/lib" -lscidbclient -lboost_system -Wl,--enable-new-dtags -Wl,-rpath,'$$ORIGIN:$$ORIGIN/../lib:$$ORIGIN/../../:$(SCIDB)/3rdparty/boost/lib:'

# Compiler settings for SciDB version >= 15.7
ifneq ("$(wildcard /usr/bin/g++-4.9)","")
  CC := "/usr/bin/gcc-4.9"
  CXX := "/usr/bin/g++-4.9"
  CXXFLAGS=-std=c++11
else
  ifneq ("$(wildcard /opt/rh/devtoolset-3/root/usr/bin/gcc)","")
    CC := "/opt/rh/devtoolset-3/root/usr/bin/gcc"
    CXX := "/opt/rh/devtoolset-3/root/usr/bin/g++"
    CXXFLAGS=-std=c++11
  endif
endif

# default: empty DESTDIR implicitly installs to /
DESTDIR=

shim: client.o sha512.o base64.o client.h
	$(CC) -Wall $(CFLAGS) -DUSE_WEBSOCKET $(INC) $(LDFLAGS) -fpic -g -o shim shim.c mongoose.c client.o sha512.o base64.o $(LIBS) -llog4cxx -lprotobuf

shim0: client.o sha512.o base64.o
	$(CC) -Wall $(CFLAGS) -O0 -DUSE_WEBSOCKET $(INC) $(LDFLAGS) -fpic -g -o shim shim.c mongoose.c client.o sha512.o base64.o $(LIBS)

client.o: client.h client.cpp
	$(CXX) $(CXXFLAGS) $(INC) -fpic -g -c client.cpp -o client.o

sha512.o:
	$(CC) -Wall $(CFLAGS) -fpic -g -c sha512.c -o sha512.o

base64.o:
	$(CC) -Wall $(CFLAGS) -fpic -g -c base64.c -o base64.o

help:
	@echo "make shim      (compile and link)"
	@echo
	@echo "The remaining options may require setting the SCIDB environment"
	@echo "variable to the path of the target SciDB installation. For example,"
	@echo "make SCIDB=/opt/scidb/13.3  install"
	@echo
	@echo "make install   (install program and files)"
	@echo "make uninstall (remove program and files)"
	@echo "make service   (install a Debian or RHEL init.d-style service, shimsvc)"
	@echo "make unservice (terminate and remove installed service)"
	@echo "make deb-pkg   (create a binary Ubuntu/Debian package, requires fpm)"
	@echo "make rpm-pkg   (create a binary RHEL package, requires fpm)"
	@echo "make test<n>   (build and run test number n)"
	@echo "make test      (build and run all but multiuser tests)"
	@echo
	@echo "Other tests are available. Read the contents of Makefile for details."

install: shim
	@if test ! -d "$(SCIDB)"; then echo  "Can't find scidb. Maybe try explicitly setting SCIDB variable, for example::\n\nmake SCIDB=/opt/scidb/13.3 install"; exit 1; fi
	@if test -x /etc/init.d/shimsvc; then /etc/init.d/shimsvc stop;fi
	mkdir -p "$(DESTDIR)$(SCIDB)/bin"
	cp shim "$(DESTDIR)/$(SCIDB)/bin"
	mkdir -p "$(DESTDIR)/var/lib/shim"
	cp -aR wwwroot "$(DESTDIR)/var/lib/shim/"
	chmod -R 755 "$(DESTDIR)/var/lib/shim"
	@if test -d $(DESTDIR)/usr/local/share/man/man1;then cp man/shim.1 $(DESTDIR)/usr/local/share/man/man1/;fi

uninstall: unservice
	@if test ! -d "$(SCIDB)"; then echo  "Can't find scidb. Maybe try explicitly setting SCIDB variable, for example:\n\nmake SCIDB=/opt/scidb/13.3 uninstall"; exit 1; fi
	- @if test -x /etc/init.d/shimsvc; then /etc/init.d/shimsvc stop;fi
	rm -f "$(SCIDB)/bin/shim"
	rm -rf /var/lib/shim
	rm -f /usr/local/share/man/man1/shim.1

service: install
	cp init.d/shimsvc /etc/init.d
	chmod 0755 /etc/init.d/shimsvc
	@if test -n "$$(which update-rc.d 2>/dev/null)"; then update-rc.d shimsvc defaults;fi
	@if test -n "$$(which chkconfig 2>/dev/null)"; then chkconfig --add shimsvc && chkconfig shimsvc on;fi
	/etc/init.d/shimsvc start

unservice:
	@if test -f /etc/init.d/shimsvc; then /etc/init.d/shimsvc stop; fi
	- @if test -n "$$(which update-rc.d 2>/dev/null)"; then sudo update-rc.d -f shimsvc remove;fi
	- @if test -n "$$(which chkconfig 2>/dev/null)"; then chkconfig --del shimsvc;fi
	rm -rf /etc/init.d/shimsvc

deb-pkg: shim
	@if test -z "$$(which fpm 2>/dev/null)"; then echo "Error: Package building requires fpm, try running gem install fpm."; exit 1;fi
	@if test ! -d "$(SCIDB)"; then echo  "Can't find scidb. Maybe try explicitly setting SCIDB variable, for example::\n\nmake SCIDB=/opt/scidb/13.3 install"; exit 1; fi
	mkdir -p pkgroot/$(SCIDB)/bin
	cp shim "pkgroot/$(SCIDB)/bin"
	mkdir -p pkgroot/etc/init.d
	cp init.d/shimsvc pkgroot/etc/init.d
	mkdir -p pkgroot/var/lib/shim
	cp -aR wwwroot pkgroot/var/lib/shim/
	chmod -R 755 pkgroot/var/lib/shim
	mkdir -p pkgroot/usr/local/share/man/man1
	@if test -d /usr/local/share/man/man1;then cp man/shim.1 pkgroot/usr/local/share/man/man1/;fi
	fpm -s dir -t deb -n shim --vendor Paradigm4 -d libssl-dev --license AGPLv3 -m "<blewis@paradigm4.com>" --url "https://github.com/Paradigm4/shim" --description "Unofficial SciDB HTTP service" --provides "shim" -v $$(basename $(SCIDB)) --after-install init.d/after-install.sh --before-remove init.d/before-remove.sh -C pkgroot opt usr var etc/init.d

rpm-pkg: shim
	@if test -z "$$(which fpm 2>/dev/null)"; then echo "Error: Package building requires fpm, try running gem install fpm."; exit 1;fi
	@if test ! -d "$(SCIDB)"; then echo  "Can't find scidb. Maybe try explicitly setting SCIDB variable, for example::\n\nmake SCIDB=/opt/scidb/13.3 install"; exit 1; fi
	mkdir -p pkgroot/$(SCIDB)/bin
	cp shim "pkgroot/$(SCIDB)/bin"
	mkdir -p pkgroot/etc/init.d
	cp init.d/shimsvc pkgroot/etc/init.d
	mkdir -p pkgroot/var/lib/shim
	cp -aR wwwroot pkgroot/var/lib/shim/
	chmod -R 755 pkgroot/var/lib/shim
	mkdir -p pkgroot/usr/local/share/man/man1
	@if test -d /usr/local/share/man/man1;then cp man/shim.1 pkgroot/usr/local/share/man/man1/;fi
	fpm -s dir -t rpm -n shim -d "openssl-devel" --vendor Paradigm4 --license AGPLv3 -m "<blewis@paradigm4.com>" --url "https://github.com/Paradigm4/shim" --description "Unofficial SciDB HTTP service" --provides "shim" -v $$(basename $(SCIDB)) --after-install init.d/after-install.sh --before-remove init.d/before-remove.sh -C pkgroot opt usr var etc/init.d

clean:
	rm -fr *.o *.so shim pkgroot *.rpm *.deb test
