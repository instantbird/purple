/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// based on netwerk/test/NetwerkTestLogging.h

#ifndef purpleLogging_h
#define purpleLogging_h

#include <mozilla/Logging.h>

// purplexpcom will just use PR_LogPrint as it doesn't
// have access to mozilla::detail::log_print. To support that MOZ_LOG is
// redefined.
#undef MOZ_LOG
#define MOZ_LOG(_module,_level,_args) \
  PR_BEGIN_MACRO \
    if (MOZ_LOG_TEST(_module,_level)) { \
      PR_LogPrint _args; \
    } \
  PR_END_MACRO

#endif
