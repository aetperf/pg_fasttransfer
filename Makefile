EXTENSION = pg_fasttransfer
MODULES = pg_fasttransfer
DATA = pg_fasttransfer--1.0.sql
PG_CONFIG = pg_config

$(shell rm -f dirent.h)
$(shell rm -rf arpa netinet)

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
