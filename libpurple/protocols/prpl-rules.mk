# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(topsrcdir)/config/rules.mk

ifeq (,$(filter $(PROTOCOL),$(STATIC_PRPLS)))
EXTRA_LIBS += $(NSPR_LIBS)

_RC_STRING += -SRCDIR .

PURPLEXPCOM_PATH = ../../../purplexpcom/src
PURPLEXPCOM_LIB = $(PURPLEXPCOM_PATH)/$(DLL_PREFIX)purplexpcom$(DLL_SUFFIX)

$(PURPLEXPCOM_LIB):
	$(MAKE) -C $(PURPLEXPCOM_PATH) compile

xpcomModule.cpp: $(srcdir)/../xpcomModule.cpp.in xpcomCid.h $(PURPLEXPCOM_LIB)
	sed 's/@PROTOCOL@/$(PROTOCOL)/g' $< > $@

xpcomCid.h:
	bash $(srcdir)/../create-cid.sh > $@

module.rc: module.ver

module.ver: $(srcdir)/../module.ver.in $(DEPTH)/config/autoconf.mk
	echo WIN32_MODULE_DESCRIPTION=$(PROTOCOL) PRotocol PLugin > $@
	sed 's/@PURPLE_MAJOR_VERSION@/$(PURPLE_MAJOR_VERSION)/;s/@PURPLE_MINOR_VERSION@/$(PURPLE_MINOR_VERSION)/;s/@PURPLE_MICRO_VERSION@/$(PURPLE_MICRO_VERSION)/' $< > $@

GARBAGE += xpcomModule.cpp module.ver xpcomCid.h
endif
