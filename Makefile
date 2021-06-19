#------------------------------------------------------------------------------#
.PHONY = 

#------------------------------------------------------------------------------#
LIBRARY_NAME = bassemu2~
LIBRARY_AUTHOR = "Christian Klippel"
LIBRARY_DESCRIPTION = "transistor bass emulation / slight modification"
LIBRARY_LICENSE = "GPL v2"
LIBRARY_VERSION = "0.1"
META_FILE = $(LIBRARY_NAME)-meta.pd
SOURCES = \
bassemu2~.c
LIBS =

#------------------------------------------------------------------------------#
UNAME := $(shell uname -s)
#------------------------------------------------------------------------------#
ifeq ($(UNAME),Linux)
  CPU := $(shell uname -m)
  EXTENSION = pd_linux
  SHARED_EXTENSION = so
  OS = linux
  PD_PATH ?= /usr
  PD_INCLUDE = $(PD_PATH)/include/pd
  CFLAGS = -I"$(PD_INCLUDE)" -Wall -W
  CFLAGS += -DPD -DVERSION='"$(LIBRARY_VERSION)"'
  CFLAGS += -fPIC
  CFLAGS += -O6 -funroll-loops -fomit-frame-pointer
  LDFLAGS = -rdynamic -shared -fPIC -Wl,-rpath,"\$$ORIGIN",--enable-new-dtags
  LIBS_linux =
  LIBS += -lc $(LIBS_linux)
endif

#------------------------------------------------------------------------------#
all: $(SOURCES:.c=.$(EXTENSION))
	@echo "done."

%.o: %.c
	$(CC) $(CFLAGS) -o "$*.o" -c "$*.c"

%.$(EXTENSION): %.o
	$(CC) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o"  $(LIBS)
	chmod a-x "$*.$(EXTENSION)"

#------------------------------------------------------------------------------#
clean:
	-rm -f -- $(SOURCES:.c=.o)
	-rm -f -- $(SOURCES:.c=.$(EXTENSION))

#------------------------------------------------------------------------------#
meta:
	@echo "#N canvas 100 100 360 360 10;" > $(META_FILE)
	@echo "#X text 10 10 META this is prototype of a libdir meta file;" >> $(META_FILE)
	@echo "#X text 10 30 NAME" $(LIBRARY_NAME) ";" >> $(META_FILE)
	@echo "#X text 10 50 AUTHOR" $(LIBRARY_AUTHOR) ";" >> $(META_FILE)
	@echo "#X text 10 70 DESCRIPTION" $(LIBRARY_DESCRIPTION) ";" >> $(META_FILE)
	@echo "#X text 10 90 LICENSE" $(LIBRARY_LICENSE) ";" >> $(META_FILE)
	@echo "#X text 10 110 VERSION" $(LIBRARY_VERSION) ";" >> $(META_FILE)
	@echo "meta done"

#------------------------------------------------------------------------------#
showsetup:
	@echo "UNAME               : $(UNAME)"
	@echo "CPU                 : $(CPU)"
	@echo "OS                  : $(OS)"
	@echo "EXTENSION           : $(EXTENSION)"
	@echo "SHARED_EXTENSION    : $(SHARED_EXTENSION)"
	@echo "PD_PATH             : $(PD_PATH)"
	@echo "PD_INCLUDE          : $(PD_INCLUDE)"
	@echo "CFLAGS              : $(CFLAGS)"
	@echo "LDFLAGS             : $(LDFLAGS)"
	@echo "LIBS                : $(LIBS)"
	@echo "LIBRARY_NAME        : $(LIBRARY_NAME)"
	@echo "LIBRARY_AUTHOR      : $(LIBRARY_AUTHOR)"
	@echo "LIBRARY_DESCRIPTION : $(LIBRARY_DESCRIPTION)"
	@echo "LIBRARY_LICENSE     : $(LIBRARY_LICENSE)"
	@echo "LIBRARY_VERSION     : $(LIBRARY_VERSION)"
	@echo "SOURCES             : $(SOURCES)"
