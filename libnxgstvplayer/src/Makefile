########################################################################
# Get Linux Build Enviornment:
include ../env.mk

######################################################################
# Includes
#INCLUDE		+= -I. -I../../prebuilt/include

######################################################################
# Library
LIBRARY += -lgstmpegts-1.0

######################################################################
# Target
COBJS  	:= \
	NX_GstDiscover.o \
	NX_GstLog.o	\
	NX_GstThumbnail.o \
	NX_TypeFind.o \
	NX_TSProgram.o \
	NX_OMXSemaphore.o

CPPOBJS	:=
CPPOBJS += \
	NX_GstMediaInfo.o \
	NX_GstMoviePlay.o

CFLAGS		+= -Wall -Wextra `pkg-config --cflags glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0 gdk-pixbuf-2.0`
CFLAGS		+= -fPIC
##########################
# For debug
CFLAGS		+= -O -DDEBUG -g
CPPFLAGS	+= -O -DDEBUG -g
CONFIG		+= debug

# For release
#CFLAGS		+= -O2 -DNDEBUG
#CPPFLAGS	+= -O2 -DNDEBUG
#CONFIG		+= release
###########################

LDFLAGS		= `pkg-config --libs glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0 gdk-pixbuf-2.0`

CPPFLAGS	+= -fPIC -Wall -Wextra `pkg-config --cflags glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0 gdk-pixbuf-1.0`

LIBNAME	:= libnxgstvplayer
TARGET	:= $(LIBNAME).so

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET): depend $(OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(TARGET) -o $@ $(OBJS) $(LIBRARY)

install:
	mkdir -p ../lib/32bit
	cp -av $(TARGET) ../lib/32bit/
	cp -av NX_GstIface.h ../include/
	cp -av NX_GstTypes.h ../include/
	@echo "done to copy libnxgstvplayer.so to ../lib/32bit/"

clean:
	rm -rf $(TARGET) *.o .depend
	rm -rf ../lib/$(TARGET)
	rm -rf ../include/NX_GstIface.h
	rm -rf ../include/NX_GstTypes.h

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CFLAGS)   $(INCS) $(SRCS) > .depend
