MODULE_big = pgsynck
OBJS	= pgsynck.o
PG_CPPFLAGS = -I$(libpq_srcdir)
EXTENSION = pgsynck
DATA = pgsynck--1.0.sql
REGRESS = pgsynck


# the db name is hard-coded in the tests
override USE_MODULE_DB =

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pgsynck
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
