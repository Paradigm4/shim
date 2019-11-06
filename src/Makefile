ifeq ($(SCIDB),)
  X := $(shell which scidb 2>/dev/null)
  ifneq ($(X),)
    X := $(shell dirname ${X})
    SCIDB := $(shell dirname ${X})
  endif
endif

VERSION := $(shell $(SCIDB)/bin/scidb --version | head -n 1 | cut -d ':' -f 2 |tr -d ' ')
COMMIT := $(shell git rev-list --abbrev-commit -1 HEAD)

ifndef VERSION
  $(error SciDB VERSION is not set)
endif

# A way to set the 3rdparty prefix path that is convenient
# for SciDB developers.
ifeq ($(SCIDB_VER),)
  SCIDB_3RDPARTY = $(SCIDB)
else
  SCIDB_3RDPARTY = /opt/scidb/$(SCIDB_VER)
endif

# A better way to set the 3rdparty prefix path that does
# not assume an absolute path. You can still use the above
# method if you prefer.
ifeq ($(SCIDB_THIRDPARTY_PREFIX),)
  SCIDB_THIRDPARTY_PREFIX := $(SCIDB_3RDPARTY)
endif

CFLAGS=-std=gnu99 -fopenmp -g -pedantic -DVERSION=\"$(VERSION)\" -DCOMMIT=\"$(COMMIT)\"
INC=-I. -DPROJECT_ROOT="\"$(SCIDB)\"" -I"$(SCIDB_THIRDPARTY_PREFIX)/3rdparty/boost/include/" -I"$(SCIDB)/include" -I"$(SCIDB)/3rdparty/boost/include/boost/container" -DSCIDB_CLIENT
LIBS=-lstdc++ -ldl -lz -pthread -L"$(SCIDB_THIRDPARTY_PREFIX)/3rdparty/boost/lib" -L"$(SCIDB)/lib" -lscidbclient -lboost_system -Wl,--enable-new-dtags -Wl,-rpath,'$$ORIGIN:$$ORIGIN/../lib:$$ORIGIN/../../:$(SCIDB)/3rdparty/boost/lib:'

# Compiler settings for SciDB version >= 15.7
ifneq ("$(wildcard /usr/bin/g++-4.9)","")
  CC := "/usr/bin/gcc-4.9"
  CXX := "/usr/bin/g++-4.9"
  CXXFLAGS=-std=c++14
else
  ifneq ("$(wildcard /opt/rh/devtoolset-3/root/usr/bin/gcc)","")
    CC := "/opt/rh/devtoolset-3/root/usr/bin/gcc"
    CXX := "/opt/rh/devtoolset-3/root/usr/bin/g++"
    CXXFLAGS=-std=c++14
  endif
endif

# default: empty DESTDIR implicitly installs to /
DESTDIR=

shim: shim.c client.o sha512.o base64.o iniparser/libiniparser.a client.h
	$(CC) -Wall $(CFLAGS) -DUSE_WEBSOCKET $(INC) $(LDFLAGS) -fpic -g -o shim shim.c mongoose.c client.o sha512.o base64.o iniparser/libiniparser.a $(LIBS) -llog4cxx -lprotobuf

client.o: client.h client.cpp
	$(CXX) $(CXXFLAGS) $(INC) -fpic -g -c client.cpp -o client.o

sha512.o:
	$(CC) -Wall $(CFLAGS) -fpic -g -c sha512.c -o sha512.o

base64.o:
	$(CC) -Wall $(CFLAGS) -fpic -g -c base64.c -o base64.o

iniparser/libiniparser.a:
	@(cd iniparser ; $(MAKE))

clean:
	$(MAKE) -C iniparser veryclean
	rm -fr *.o *.so shim
