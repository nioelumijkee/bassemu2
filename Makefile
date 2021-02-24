#------------------------------------------------------------------------------#
.PHONY = all meta clean showsetup

#------------------------------------------------------------------------------#
LIBRARY_NAME = bassemu2~
LIBRARY_AUTHOR = "Christian Klippel / NioNiu"
LIBRARY_DESCRIPTION = "transistor bass emulation / slight modification"
LIBRARY_LICENSE = "GPL v2"
LIBRARY_VERSION = "0.1"
META_FILE = $(LIBRARY_NAME)-meta.pd

#
SOURCESDIR = .
SOURCES = \
$(SOURCESDIR)/bassemu2~.c
LIBS =

#------------------------------------------------------------------------------#
CFLAGS = -DPD -I"$(PD_INCLUDE)" -Wall -W
CFLAGS += -DVERSION='"$(LIBRARY_VERSION)"'
LDFLAGS =

#------------------------------------------------------------------------------#
UNAME := $(shell uname -s)
# Linux -----------------------------------------------------------------------#
ifeq ($(UNAME),Linux)
	OS = linux
	EXTENSION = pd_linux
	CPU := $(shell uname -m)
	PD_PATH ?= /usr
	PD_INCLUDE = $(PD_PATH)/include
	OPT_CFLAGS = -O5 -funroll-loops -fomit-frame-pointer
	CFLAGS += -fPIC
	LDFLAGS += -Wl,--export-dynamic  -shared -fPIC
	LIBS += -lc
	STRIP = strip --strip-unneeded -R .note -R .comment
endif

#------------------------------------------------------------------------------#
ALL_CFLAGS := $(ALL_CFLAGS) $(CFLAGS) $(OPT_CFLAGS)
ALL_LDFLAGS := $(LDFLAGS) $(ALL_LDFLAGS)
ALL_LIBS := $(LIBS) $(ALL_LIBS)

#------------------------------------------------------------------------------#
all: $(SOURCES:.c=.$(EXTENSION))
	@echo "done"

#
$(SOURCES:.c=.$(EXTENSION)): %.$(EXTENSION): %.o
	$(CC) $(ALL_LDFLAGS) -o $@  $<  $(ALL_LIBS)
	chmod a-x $@

# obj
%.o: %.c
	$(CC) $(ALL_CFLAGS) -o "$*.o" -c "$*.c"

#------------------------------------------------------------------------------#
meta:
	echo "#N canvas 100 100 420 420 10;" > $(META_FILE)
	echo "#X text 20 20 META this is prototype of a libdir meta file;" >> $(META_FILE)
	echo "#X text 20 40 NAME" $(LIBRARY_NAME) ";" >> $(META_FILE)
	echo "#X text 20 60 AUTHOR" $(LIBRARY_AUTHOR) ";" >> $(META_FILE)
	echo "#X text 20 80 DESCRIPTION" $(LIBRARY_DESCRIPTION) ";" >> $(META_FILE)
	echo "#X text 20 100 LICENSE" $(LIBRARY_LICENSE) ";" >> $(META_FILE)
	echo "#X text 20 120 VERSION" $(LIBRARY_VERSION) ";" >> $(META_FILE)
	@echo "meta done"

#------------------------------------------------------------------------------#
clean:
	-rm -f -- $(SOURCES:.c=.o)
	-rm -f -- $(SOURCES:.c=.$(EXTENSION))
	@echo "clean done"

#------------------------------------------------------------------------------#
showsetup:
	@echo "UNAME               : $(UNAME)"
	@echo "OS                  : $(OS)"
	@echo "CPU                 : $(CPU)"
	@echo "EXTENSION           : $(EXTENSION)"
	@echo "PD_PATH             : $(PD_PATH)"
	@echo "PD_INCLUDE          : $(PD_INCLUDE)"
	@echo "CFLAGS              : $(CFLAGS)"
	@echo "OPT_CFLAGS          : $(OPT_CFLAGS)"
	@echo "ALL_CFLAGS          : $(ALL_CFLAGS)"
	@echo "LDFLAGS             : $(LDFLAGS)"
	@echo "ALL_LDFLAGS         : $(ALL_LDFLAGS)"
	@echo "LIBS                : $(LIBS)"
	@echo "ALL_LIBS            : $(ALL_LIBS)"
	@echo "STRIP               : $(STRIP)"
	@echo "LIBRARY_NAME        : $(LIBRARY_NAME)"
	@echo "LIBRARY_AUTHOR      : $(LIBRARY_AUTHOR)"
	@echo "LIBRARY_DESCRIPTION : $(LIBRARY_DESCRIPTION)"
	@echo "LIBRARY_LICENSE     : $(LIBRARY_LICENSE)"
	@echo "LIBRARY_VERSION     : $(LIBRARY_VERSION)"
	@echo "META_FILE           : $(META_FILE)"
	@echo "SOURCESDIR          : $(SOURCESDIR)"
	@echo "SOURCES             : $(SOURCES)"
