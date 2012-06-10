# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifeq (,$(filter $(PROTOCOL),$(STATIC_PRPLS)))
CPPSRCS		= \
		xpcomModule.cpp \
		$(NULL)

EXTRA_DSO_LDOPTS += $(XPCOM_GLUE_LDOPTS) $(MOZ_COMPONENT_NSPR_LIBS)
else
ifeq ($(OS_ARCH),Darwin)
OBJS		:= $(shell echo $(strip $(addprefix $(PROTOCOL)_,$(CSRCS:.c=.$(OBJ_SUFFIX)))) | sed 's@$(PROTOCOL)_\([^ ]*\)/\([^ ]*\)\.$(OBJ_SUFFIX)@\1/$(PROTOCOL)_\2.$(OBJ_SUFFIX)@g')
endif
endif

include $(topsrcdir)/config/rules.mk

ifeq (,$(filter $(PROTOCOL),$(STATIC_PRPLS)))
_RC_STRING += -SRCDIR .

xpcomModule.cpp: $(srcdir)/../xpcomModule.cpp.in xpcomCid.h
	sed 's/@PROTOCOL@/$(PROTOCOL)/g' $< > $@

xpcomCid.h:
	bash $(srcdir)/../create-cid.sh > $@

module.rc: module.ver

module.ver: $(srcdir)/../module.ver.in $(DEPTH)/config/autoconf.mk
	echo WIN32_MODULE_DESCRIPTION=$(PROTOCOL) PRotocol PLugin > $@
	sed 's/@PURPLE_MAJOR_VERSION@/$(PURPLE_MAJOR_VERSION)/;s/@PURPLE_MINOR_VERSION@/$(PURPLE_MINOR_VERSION)/;s/@PURPLE_MICRO_VERSION@/$(PURPLE_MICRO_VERSION)/' $< > $@

GARBAGE += xpcomModule.cpp module.ver xpcomCid.h
else
$(PROTOCOL)_%.$(OBJ_SUFFIX): %.c $(GLOBAL_DEPS)
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CC)
	$(ELOG) $(CC) $(OUTOPTION)$@ -c $(COMPILE_CFLAGS) $(_VPATH_SRCS)
endif
