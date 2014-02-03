MODULE_big = pg_syntax_checker
OBJS	= pg_syntax_checker.o
PG_CPPFLAGS = -I$(libpq_srcdir)
EXTENSION = pg_syntax_checker
DATA = pg_syntax_checker--1.0.sql


# the db name is hard-coded in the tests
override USE_MODULE_DB =

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_syntax_checker
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
