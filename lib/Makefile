###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2006 Lintux ##
###########################

### DEFINITIONS

-include ../Makefile.settings
ifdef SRCDIR
SRCDIR := $(SRCDIR)lib/
endif

# [SH] Program variables
objects = arc.o base64.o $(DES) $(EVENT_HANDLER) ftutil.o http_client.o ini.o md5.o misc.o oauth.o oauth2.o proxy.o sha1.o $(SSL_CLIENT) url.o xmltree.o

LFLAGS += -r

# [SH] Phony targets
all: lib.o
check: all
lcov: check
gcov:
	gcov *.c

.PHONY: all clean distclean

clean: $(subdirs)
	rm -f *.o $(OUTFILE) core

distclean: clean $(subdirs)
	rm -rf .depend

### MAIN PROGRAM

lib.o: $(objects) $(subdirs)
	@echo '*' Linking lib.o
	@$(LD) $(LFLAGS) $(objects) -o lib.o

$(objects): ../Makefile.settings Makefile

$(objects): %.o: $(SRCDIR)%.c
	@echo '*' Compiling $<
	@$(CC) -c $(CFLAGS) $< -o $@

-include .depend/*.d
