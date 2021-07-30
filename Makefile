################################################################################
lib.name = bassemu2~
cflags = 
class.sources = bassemu2~.c
sources = 
datafiles = \
bassemu2~-help.pd \
bassemu2~-meta.pd \
README.md \
LICENSE.txt

################################################################################
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
