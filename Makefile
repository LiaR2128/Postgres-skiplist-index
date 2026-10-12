MODULES = pg_skiplist
EXTENSION = pg_skiplist
DATA = pg_skiplist--1.0.sql
REGRESS = pg_skiplist

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
