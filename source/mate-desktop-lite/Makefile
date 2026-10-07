PROJECT_ROOT := $(abspath ../..)
PREFIX ?= $(PROJECT_ROOT)/prefix/quartz
MATE_SRC ?= $(PROJECT_ROOT)/source/mate-desktop/libmate-desktop
SCHEMAS_SRC ?= $(PROJECT_ROOT)/source/mate-desktop/schemas
PKG_CONFIG ?= pkg-config
CC ?= cc
INSTALL ?= install
SED ?= sed
GLIB_COMPILE_SCHEMAS ?= glib-compile-schemas

VERSION = 1.28.2
LIBNAME = libmate-desktop-2.dylib
COMPAT_NAME = libmate-desktop-2.17.dylib

PKGS = gtk+-3.0 gio-2.0 gio-unix-2.0 glib-2.0 gobject-2.0 gdk-pixbuf-2.0
CFLAGS += -O2 -g -fPIC -DMATE_DESKTOP_USE_UNSTABLE_API \
	-DMATELOCALEDIR='"$(PREFIX)/share/locale"' \
	-I. -I$(MATE_SRC) -I$(dir $(MATE_SRC)) \
	$(shell $(PKG_CONFIG) --cflags $(PKGS))
LDFLAGS += -dynamiclib -install_name $(PREFIX)/lib/$(COMPAT_NAME) \
	-compatibility_version 17.0.0 -current_version 17.0.0
LIBS += $(shell $(PKG_CONFIG) --libs $(PKGS))

SOURCES = \
	$(MATE_SRC)/mate-desktop-utils.c \
	$(MATE_SRC)/mate-desktop-thumbnail.c \
	mate-bg-stubs.c \
	mate-bg-crossfade-stubs.c

OBJECTS = $(notdir $(SOURCES:.c=.o))

.PHONY: all install clean

all: $(LIBNAME)

$(LIBNAME): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

mate-desktop-utils.o: $(MATE_SRC)/mate-desktop-utils.c config.h
	$(CC) $(CFLAGS) -c -o $@ $<

mate-desktop-thumbnail.o: $(MATE_SRC)/mate-desktop-thumbnail.c config.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c config.h
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	$(INSTALL) -d $(PREFIX)/lib $(PREFIX)/lib/pkgconfig
	$(INSTALL) -d $(PREFIX)/include/mate-desktop-2.0/libmate-desktop
	$(INSTALL) -d $(PREFIX)/share/glib-2.0/schemas
	$(INSTALL) -m 755 $(LIBNAME) $(PREFIX)/lib/$(COMPAT_NAME)
	ln -sf $(COMPAT_NAME) $(PREFIX)/lib/$(LIBNAME)
	$(INSTALL) -m 644 $(MATE_SRC)/mate-desktop-utils.h $(PREFIX)/include/mate-desktop-2.0/libmate-desktop/
	$(INSTALL) -m 644 $(MATE_SRC)/mate-desktop-thumbnail.h $(PREFIX)/include/mate-desktop-2.0/libmate-desktop/
	$(INSTALL) -m 644 $(MATE_SRC)/mate-bg.h $(PREFIX)/include/mate-desktop-2.0/libmate-desktop/
	$(INSTALL) -m 644 $(MATE_SRC)/mate-bg-crossfade.h $(PREFIX)/include/mate-desktop-2.0/libmate-desktop/
	$(INSTALL) -m 644 $(SCHEMAS_SRC)/*.gschema.xml $(PREFIX)/share/glib-2.0/schemas/
	$(SED) 's|@datadir@|$(PREFIX)/share|g' $(SCHEMAS_SRC)/org.mate.background.gschema.xml.in > $(PREFIX)/share/glib-2.0/schemas/org.mate.background.gschema.xml
	printf '%s\n' \
		'prefix=$(PREFIX)' \
		'exec_prefix=$${prefix}' \
		'libdir=$${prefix}/lib' \
		'includedir=$${prefix}/include' \
		'' \
		'Name: mate-desktop-2.0' \
		'Description: Lightweight mate-desktop subset for MacMateCaja' \
		'Requires: gtk+-3.0' \
		'Version: $(VERSION)' \
		'Libs: -L$${libdir} -lmate-desktop-2' \
		'Cflags: -I$${includedir}/mate-desktop-2.0' \
		> $(PREFIX)/lib/pkgconfig/mate-desktop-2.0.pc
	$(GLIB_COMPILE_SCHEMAS) $(PREFIX)/share/glib-2.0/schemas

clean:
	rm -f $(OBJECTS) $(LIBNAME)
