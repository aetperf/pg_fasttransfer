
# Supprimer fichiers et dossiers au même niveau que le Makefile
$(shell rm -f dirent.h)
$(shell rm -rf arpa netinet)


EXTENSION = pg_fasttransfer
DATA = pg_fasttransfer--1.0.sql

MODULE_big = pg_fasttransfer
OBJS = pg_fasttransfer.o tiny_aes.o

PG_CPPFLAGS = -I$(srcdir)/tiny-aes
PG_CONFIG = pg_config

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
