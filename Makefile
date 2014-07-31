#
INSTALL=install
INSTALL_PREFIX=/home/vassilux/Projects/ESI/asterisk-src/asterisk-11/live
ASTLIBDIR:=$(shell awk '/moddir/{print $$3}' $(INSTALL_PREFIX)/etc/asterisk/asterisk.conf)

ASTERISKINCLUDE=-I$(INSTALL_PREFIX)/usr/include

#
ifeq ($(strip $(ASTLIBDIR)),)
	MODULES_DIR=$(INSTALL_PREFIX)/usr/lib/asterisk/modules
else
	MODULES_DIR=$(ASTLIBDIR)
endif
ASTETCDIR=$(INSTALL_PREFIX)/etc/asterisk
SAMPLENAME=xoip.conf
CONFNAME=$(basename $(SAMPLENAME))

CC=gcc
OPTIMIZE=-O2
DEBUG=-g
INCLUDE=${ASTERISKINCLUDE} -Iinclude
SOURCES=.
APPS=app_xoip.so

AST_DEVMODE=yes

FILES   = $(shell find ./src -name "*.c")
HEADERS = $(shell find ./include -name "*.h")
OBJS    = $(FILES:.c=.o)
DEPS    := $(FILES:.c=.d)

CFLAGS=

ifeq ($(AST_DEVMODE),yes) 
    CFLAGS=-Wunused -Wundef -Wmissing-format-attribute -D_REENTRANT -D_GNU_SOURCE
else
    CFLAGS=-pipe -fPIC -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations ${DEBUG} ${INCLUDE} -D_REENTRANT -D_GNU_SOURCE

endif

LIBS+=

all: _all
    ifeq ($(AST_DEVMODE),yes) 
	@echo "Developpement mode generation"
    else
	@echo "Production mode generation"
    endif

	@echo " +--------- app_xoip Build Complete ---------+"
	@echo " + app_xoip has successfully been built,     +"
	@echo " + and can be installed by running:          +"
	@echo " +                                           +"
	@echo " +               make install                +"
	@echo " +-------------------------------------------+"

_all: ${APPS}

-include Makefile.deps

%.o: %.c
	@echo Compiling $<...
	$(CC) $(CFLAGS) $(INCLUDE) $(DEBUG) $(OPTIMIZE) -c -o $@ $<

${APPS}: $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDE) $(OBJS) -shared -Xlinker -x -o ./bin/$(APPS)

clean:
	rm -f src/*.o *.so .*.d *~

install: _all
	$(INSTALL_PREFIX)/asterisk -rx 'module unload ${APPS}'
	$(INSTALL) -m 755 ./bin/${APPS} $(MODULES_DIR)
	$(INSTALL_PREFIX)/asterisk -rx 'module load ${APPS}'

	@echo " +---- app_xoip Installation Complete -------+"
	@echo " +                                           +"
	@echo " + app_xoip has successfully been installed. +"
	@echo " + If you would like to install the sample   +"
	@echo " + configuration file run:                   +"
	@echo " +                                           +"
	@echo " +              make samples                 +"
	@echo " +-------------------------------------------+"

samples:
	@mkdir -p $(DESTDIR)$(ASTETCDIR)
	@if [ -f $(DESTDIR)$(ASTETCDIR)/$(CONFNAME) ]; then \
		echo "Backing up previous config file as $(CONFNAME).old";\
		mv -f $(DESTDIR)$(ASTETCDIR)/$(CONFNAME) $(DESTDIR)$(ASTETCDIR)/$(CONFNAME).old ; \
	fi ;
	$(INSTALL) -m 644 $(SAMPLENAME) $(DESTDIR)$(ASTETCDIR)/$(CONFNAME)
	@echo " ------- app_xoip confing Installed ---------"

reload: install
	$(INSTALL_PREFIX)/asterisk -rx 'module unload ${APPS}'
	$(INSTALL_PREFIX)/asterisk -rx 'module load ${APPS}'

-include ${DEPS}
