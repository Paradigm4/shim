ifeq ($(SCIDB),)
  X := $(shell which scidb 2>/dev/null)
  ifneq ($(X),)
    X := $(shell dirname ${X})
    SCIDB := $(shell dirname ${X})
  endif
endif

SCIDB_VERSION := $(shell $(SCIDB)/bin/scidb --version |head -1|awk '{print $$3}'|awk -F. '{print $$1 "." $$2}')

# default: empty DESTDIR implicitly installs to /
DESTDIR=

.PHONY: shim
shim:
	$(MAKE) -C src shim
	@cp src/shim .

client:
	$(MAKE) -C src client

help:
	@if test ! -d "$(SCIDB)"; then echo  "Can't find the scidb executable. Try explicitly setting the SCIDB variable, for example: 'make SCIDB=/opt/scidb/19.3 $@'"; exit 1; fi
	@echo "make shim      (compile and link)"
	@echo
	@echo "The remaining options may require setting the SCIDB environment"
	@echo "variable to the path of the target SciDB installation. For example,"
	@echo "make SCIDB=$(SCIDB) install"
	@echo
	@echo "make install   (install program and files)"
	@echo "make uninstall (remove program and files)"
	@echo "make service   (install a Debian or RHEL shim service)"
	@echo "make unservice (terminate and remove installed service)"
	@echo "make deb-pkg   (create a binary Ubuntu/Debian package, requires fpm)"
	@echo "make rpm-pkg   (create a binary RHEL package, requires fpm)"
	@echo "make test<n>   (build and run test number n)"
	@echo "make test      (build and run all but multiuser tests)"
	@echo
	@echo "Other tests are available. Read the contents of Makefile for details."

install: shim stop-service
	@if test ! -d "$(SCIDB)"; then echo  "Can't find the scidb executable. Try explicitly setting the SCIDB variable, for example: 'make SCIDB=/opt/scidb/19.3 $@'"; exit 1; fi
	@mkdir -p "$(DESTDIR)$(SCIDB)/bin"
	cp shim "$(DESTDIR)$(SCIDB)/bin"
	mkdir -p "$(DESTDIR)/var/lib/shim"
	chmod -R 755 "$(DESTDIR)/var/lib/shim"
	cp -aR wwwroot "$(DESTDIR)/var/lib/shim/"
	init.d/setup-conf.sh
	@if test -d $(DESTDIR)/usr/local/share/man/man1;then cp man/shim.1 $(DESTDIR)/usr/local/share/man/man1/;fi

uninstall: stop-service
	@if test ! -d "$(SCIDB)"; then echo  "Can't find the scidb executable. Try explicitly setting the SCIDB variable, for example: 'make SCIDB=/opt/scidb/19.3 $@'"; exit 1; fi
	rm -f "$(SCIDB)/bin/shim"
	rm -rf /var/lib/shim
	rm -f /usr/local/share/man/man1/shim.1

stop-service:
	- @if test -n "$$(which systemctl 2>/dev/null)";   then systemctl stop shimsvc 2>/dev/null||true; fi
	- @if test -n "$$(which update-rc.d 2>/dev/null)"; then service shimsvc stop 2>/dev/null||true; fi
	- @if test -n "$$(which chkconfig 2>/dev/null)";   then service shimsvc stop 2>/dev/null||true; fi

service: install
	@if test ! -d "$(SCIDB)"; then echo  "Can't find the scidb executable. Try explicitly setting the SCIDB variable, for example: 'make SCIDB=/opt/scidb/19.3 $@'"; exit 1; fi
# systemctl
	- @if test -n "$$(which systemctl 2>/dev/null)"; then systemctl disable shimsvc 2>/dev/null||true; fi
	- @if test -n "$$(which systemctl 2>/dev/null)"; then sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/shimvc.service > /lib/systemd/system/shimsvc.service; fi
	- @if test -n "$$(which systemctl 2>/dev/null)"; then systemctl daemon-reload; systemctl enable shimsvc; systemctl start shimsvc; fi
# update-rc
	- @if test -n "$$(which update-rc.d 2>/dev/null)"; then update-rc.d -f shimsvc remove 2>/dev/null||true; fi
	- @if test -n "$$(which update-rc.d 2>/dev/null)"; then sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/shimsvc.initd > /etc/init.d/shimsvc; fi
	- @if test -n "$$(which update-rc.d 2>/dev/null)"; then chmod 0755 /etc/init.d/shimsvc; update-rc.d shimsvc defaults; service shimsvc start; fi
# init.d
	- @if test -n "$$(which chkconfig 2>/dev/null)"; then chkconfig --del shimsvc 2>/dev/null||true; fi
	- @if test -n "$$(which chkconfig 2>/dev/null)"; then sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/shimsvc.initd > /etc/init.d/shimsvc; fi
	- @if test -n "$$(which chkconfig 2>/dev/null)"; then chmod 0755 /etc/init.d/shimsvc; chkconfig --add shimsvc; service shimsvc start; fi

unservice: stop-service
	- @if test -n "$$(which systemctl 2>/dev/null)";   then systemctl disable shimsvc 2>/dev/null||true; rm -f /lib/systemd/system/shimsvc.service; systemctl daemon-reload; fi
	- @if test -n "$$(which update-rc.d 2>/dev/null)"; then update-rc.d -f shimsvc remove 2>/dev/null||true; rm -f /etc/init.d/shimsvc; fi
	- @if test -n "$$(which chkconfig 2>/dev/null)";   then chkconfig --del shimsvc 2>/dev/null||true; rm -f /etc/init.d/shimsvc; fi
	$(MAKE) uninstall

deb-pkg: shim
	@if test -z "$$(which fpm 2>/dev/null)"; then echo "Error: Package building requires fpm, try running gem install fpm."; exit 1;fi
	@if test ! -d "$(SCIDB)"; then echo  "Can't find the scidb executable. Try explicitly setting the SCIDB variable, for example: 'make SCIDB=/opt/scidb/19.3 $@'"; exit 1; fi
	rm -rf pkgroot *.deb
	mkdir -p pkgroot/$(SCIDB)/bin
	cp shim "pkgroot/$(SCIDB)/bin"
	mkdir -p pkgroot/$(SCIDB)/shim
	sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/after-install.sh > pkgroot/$(SCIDB)/shim/after-install.sh
	cp init.d/before-remove.sh pkgroot/$(SCIDB)/shim
	cp init.d/setup-conf.sh pkgroot/$(SCIDB)/shim
	sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/shimsvc.service > pkgroot/$(SCIDB)/shim/shimsvc.service
	sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/shimsvc.initd > pkgroot/$(SCIDB)/shim/shimsvc.initd
	mkdir -p pkgroot/var/lib/shim
	cp -aR wwwroot pkgroot/var/lib/shim/
	chmod -R 755 pkgroot/var/lib/shim
	mkdir -p pkgroot/usr/local/share/man/man1
	@if test -d /usr/local/share/man/man1;then cp man/shim.1 pkgroot/usr/local/share/man/man1/;fi
	fpm -s dir -t deb -n shim -d libssl-dev --vendor Paradigm4 --license AGPLv3 -m "<blewis@paradigm4.com>" --url "https://github.com/Paradigm4/shim" --description "Unofficial SciDB HTTP service" --provides "shim" -v $$(basename $(SCIDB)) --after-install pkgroot/$(SCIDB)/shim/after-install.sh --before-remove init.d/before-remove.sh -C pkgroot opt usr var

rpm-pkg: shim
	@if test -z "$$(which fpm 2>/dev/null)"; then echo "Error: Package building requires fpm, try running gem install fpm."; exit 1;fi
	@if test ! -d "$(SCIDB)"; then echo  "Can't find the scidb executable. Try explicitly setting the SCIDB variable, for example: 'make SCIDB=/opt/scidb/19.3 $@'"; exit 1; fi
	rm -rf pkgroot *.rpm
	mkdir -p pkgroot/$(SCIDB)/bin
	cp shim "pkgroot/$(SCIDB)/bin"
	mkdir -p pkgroot/$(SCIDB)/shim
	sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/after-install.sh > pkgroot/$(SCIDB)/shim/after-install.sh
	cp init.d/before-remove.sh pkgroot/$(SCIDB)/shim
	cp init.d/setup-conf.sh pkgroot/$(SCIDB)/shim
	sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/shimsvc.service > pkgroot/$(SCIDB)/shim/shimsvc.service
	sed "s!XXX_SCIDB_VER_XXX!$(SCIDB_VERSION)!g" init.d/shimsvc.initd > pkgroot/$(SCIDB)/shim/shimsvc.initd
	mkdir -p pkgroot/var/lib/shim
	cp -aR wwwroot pkgroot/var/lib/shim/
	chmod -R 755 pkgroot/var/lib/shim
	mkdir -p pkgroot/usr/local/share/man/man1
	@if test -d /usr/local/share/man/man1;then cp man/shim.1 pkgroot/usr/local/share/man/man1/;fi
	fpm -s dir -t rpm -n shim -d openssl-devel --vendor Paradigm4 --license AGPLv3 -m "<blewis@paradigm4.com>" --url "https://github.com/Paradigm4/shim" --description "Unofficial SciDB HTTP service" --provides "shim" -v $$(basename $(SCIDB)) --after-install pkgroot/$(SCIDB)/shim/after-install.sh --before-remove init.d/before-remove.sh -C pkgroot opt usr var

clean:
	$(MAKE) -C src clean
	rm -fr *.o *.so shim pkgroot *.rpm *.deb

test0:
	@echo "ALL TESTS REQUIRE A SCIDB SERVER ON 127.0.0.1:1239 WITH security=trust"

test1: shim
	@echo "Non-authenticated test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/noauth.sh

test2: shim
	@echo "Basic digest authentication"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/digest_auth.sh

test3: shim
	@echo "TLS without authentication"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/tls.sh

test4: shim
	@echo "TLS with digest authentication"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/tls_digest.sh

test5: shim
	@echo "TLS with SciDB authentication"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/tls_scidbauth.sh

test6: shim
	@echo "cancel test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/cancel.sh

# test7: shim
# 	@echo "multiuser streaming test"
# 	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/multiple_users_stream.sh

# test8: shim
# 	@echo "repeated multiuser streaming test"
# 	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/more_multiple_users_stream.sh

test9: shim
	@echo "read_bytes test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/read_bytes.sh

# test10: shim
# 	@echo "file upload test"
# 	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/upload_file.sh

# test11: shim0
# 	@echo "valgrind test"
# 	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/valgrind.sh
# 	@echo "Now carefully inspect the report in /tmp/valgrind.out"

test12: shim
	@echo "post upload test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/upload.sh

test13: shim
	@echo "post upload test session"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/upload_session.sh

test14: shim
	@echo "get_log test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/get_log.sh

test15: shim
	@echo "status code test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/status_code.sh

test16: shim
	@echo "read test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/read.sh

test17: shim
	@echo "crash test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/crash.sh

test18: shim
	@echo "auth test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/auth.sh

test19: shim
	@echo "admin test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/admin.sh

test20: shim
	@echo "atts_only test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/atts_only.sh

test: test0 test1 test2 test3 test4 test5 test6 test9 test12 test13 test14 test15 test16 test17 test19 test20

grinder: shim0
	@echo "multiuser valgrind test"
	@LD_LIBRARY_PATH="$(SCIDB)/lib:$(SCIDB)/3rdparty/boost/lib" ./tests/grinder.sh
	@echo "Now carefully inspect the report in /tmp/grinder.out"
