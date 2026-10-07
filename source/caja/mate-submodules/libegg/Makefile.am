NULL=

noinst_LTLIBRARIES = libegg.la

AM_CPPFLAGS = $(LIBEGG_CFLAGS)

EGG_TREE_DND_FILES = 		\
	eggtreemultidnd.c	\
	eggtreemultidnd.h	\
	$(NULL)

EGG_SMCLIENT_FILES = 		\
	eggdesktopfile.c	\
	eggdesktopfile.h	\
	eggsmclient.c		\
	eggsmclient.h		\
	eggsmclient-private.h	\
	$(EGG_SMCLIENT_XSMP_FILES) \
	$(NULL)

if QUARTZ_BROWSER_ONLY
EGG_SMCLIENT_XSMP_FILES =
EGG_SMCLIENT_BACKEND_CFLAGS =
EGG_SMCLIENT_BACKEND_LIBS =
else
EGG_SMCLIENT_XSMP_FILES = eggsmclient-xsmp.c
EGG_SMCLIENT_BACKEND_CFLAGS = -DEGG_SM_CLIENT_BACKEND_XSMP
EGG_SMCLIENT_BACKEND_LIBS = -lSM -lICE
endif

libegg_la_SOURCES = 		\
	$(EGG_TREE_DND_FILES)	\
	$(EGG_SMCLIENT_FILES)	\
	$(NULL)

libegg_la_CFLAGS =				\
	$(EGG_SMCLIENT_BACKEND_CFLAGS)		\
	-DG_LOG_DOMAIN=\""EggSMClient"\"	\
	$(LIBEGG_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(DISABLE_DEPRECATED)

libegg_la_LIBADD = 	\
	$(LIBEGG_LIBS)	\
	$(EGG_SMCLIENT_BACKEND_LIBS)

-include $(top_srcdir)/git.mk
