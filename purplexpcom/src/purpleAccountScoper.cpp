/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleAccountScoper.h"
#include "purpleAccount.h"

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleAccountScoper:5
//
static PRLogModuleInfo *gPurpleAccountScoperLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleAccountScoperLog, PR_LOG_DEBUG, args)

PRUint32 purpleAccountScoper::sCurrentId = 0;

purpleAccountScoper::purpleAccountScoper(purpleAccount *aAccount)
{
  if (aAccount) {
    enterScope(aAccount->ScopeId());
    return;
  }
  mPreviousId = sCurrentId;
}

purpleAccountScoper::purpleAccountScoper(PRUint32 aId)
{
  enterScope(aId);
}

purpleAccountScoper::~purpleAccountScoper()
{
  LOG(("~purpleAccountScoper %u <- %u\n", mPreviousId, sCurrentId));
  sCurrentId = mPreviousId;
}

void purpleAccountScoper::enterScope(PRUint32 aId)
{
#ifdef PR_LOGGING
  if (!gPurpleAccountScoperLog)
    gPurpleAccountScoperLog = PR_NewLogModule("purpleAccountScoper");
#endif

  mPreviousId = sCurrentId;
  LOG(("purpleAccountScoper  %u -> %u\n", sCurrentId, aId));
  sCurrentId = aId;
}
