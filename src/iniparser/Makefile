#
# iniparser Makefile
#

# Compiler settings
CC      ?= gcc

CFLAGS  += -fPIC -Wall -Wextra -ansi -pedantic
ifndef DEBUG
ADDITIONAL_CFLAGS  ?= -O2
else
ADDITIONAL_CFLAGS  ?= -g
endif

CFLAGS += ${ADDITIONAL_CFLAGS}

# Ar settings to build the library
AR	    ?= ar
ARFLAGS = rcv

RM      ?= rm -f


# Implicit rules

SUFFIXES = .o .c .h .a

COMPILE.c	?= $(CC) $(CFLAGS) -c

.c.o:
	$(COMPILE.c) $(OUTPUT_OPTION) $<


SRCS = src/iniparser.c \
	   src/dictionary.c

OBJS = $(SRCS:.c=.o)


default:	libiniparser.a

libiniparser.a:	$(OBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	$(RM) $(OBJS)

veryclean:
	$(RM) $(OBJS) libiniparser.a

