# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(DEPTH)/config/autoconf.mk

MODULE		= purple/$(PROTOCOL)
LIBRARY_NAME	= $(PROTOCOL)
ifneq (,$(filter $(PROTOCOL),$(STATIC_PRPLS)))
FORCE_STATIC_LIB= 1
LIB_IS_C_ONLY   = 1
else
ifdef PURPLEXPCOM
FORCE_SHARED_LIB= 1
ifeq ($(OS_ARCH),WINNT)
EXTRA_LIBS	= ../../../purplexpcom/src/$(LIB_PREFIX)purplexpcom.$(LIB_SUFFIX)
else
EXTRA_LIBS	= ../../../purplexpcom/src/$(DLL_PREFIX)purplexpcom$(DLL_SUFFIX) -lz
endif
endif
endif
IS_COMPONENT	= 1
VISIBILITY_FLAGS=

LOCAL_INCLUDES = -I$(DIST)/include/libpurple

ifeq (,$(filter WINNT Darwin,$(OS_ARCH)))
LOCAL_INCLUDES	+= $(GLIB_CFLAGS)
EXTRA_LIBS	+= $(GLIB_LIBS) $(GLIB_GMODULE_LIBS) $(LIBXML2_LIBS)
else
LOCAL_INCLUDES  += -I$(DIST)/include/glib
endif

PURPLE_VERSION	= $(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION).$(PURPLE_MICRO_VERSION)
DEFINES		+= -DHAVE_CONFIG_H -DPACKAGE=\"$(PROTOCOL)\" -DVERSION=\"$(PURPLE_VERSION)\" -DDISPLAY_VERSION=\"$(PURPLE_VERSION)\"

ifneq (,$(filter $(PROTOCOL),$(STATIC_PRPLS)))
DEFINES += -DPURPLE_STATIC_PRPL
ifeq ($(OS_ARCH),WINNT)
DEFINES += -DGLIB_STATIC_COMPILATION
endif
endif

GLOBAL_DEPS += $(srcdir)/../prpl.mk $(srcdir)/../prpl-rules.mk
