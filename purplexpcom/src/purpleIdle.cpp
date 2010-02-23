/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Instantbird messenging client, released
 * 2010.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "purpleIdle.h"
#include <purpleCoreService.h>
#include <nsCOMPtr.h>
#include <nsServiceManagerUtils.h>
#include <nsIIdleService.h>
#include <nsIObserverService.h>
#include <nsIPrefService.h>
#include <nsIPrefBranch2.h>
#include <nsIPrefLocalizedString.h>
#include <nsStringAPI.h>

#include <glib.h>
#pragma GCC visibility push(default)
#include <libpurple/idle.h>
#include <libpurple/savedstatuses.h>
#pragma GCC visibility pop

#include <prlog.h>
#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleIdle:5
//
static PRLogModuleInfo *gPurpleIdleLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleIdleLog, PR_LOG_DEBUG, args)

NS_IMPL_ISUPPORTS1(purpleIdleObserver, nsIObserver)

#define PREF_TIME_BEFORE_IDLE "messenger.status.timeBeforeIdle"
#define PREF_AWAY_WHEN_IDLE   "messenger.status.awayWhenIdle"
#define PREF_DEFAULT_MESSAGE  "messenger.status.defaultIdleAwayMessage"

purpleIdleObserver::purpleIdleObserver()
  :mTimeBeforeIdle(5000),
   mIdle(PR_FALSE),
   mInitialized(PR_FALSE)
{
#ifdef PR_LOGGING
  if (!gPurpleIdleLog)
    gPurpleIdleLog = PR_NewLogModule("purpleIdle");
#endif

  LOG(("new purpleIdleObserver"));
  nsCOMPtr<nsIPrefBranch2> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefBranch, );

  nsresult rv = prefBranch->GetIntPref(PREF_TIME_BEFORE_IDLE, &mTimeBeforeIdle);
  NS_ENSURE_SUCCESS(rv, );

  rv = prefBranch->AddObserver(PREF_TIME_BEFORE_IDLE, this, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, );

  nsCOMPtr<nsIIdleService> idleService =
    do_GetService("@mozilla.org/widget/idleservice;1");
  NS_ENSURE_TRUE(idleService, );
  idleService->AddIdleObserver(this, mTimeBeforeIdle);
  LOG((" added idle observer for %u s of inactivity", mTimeBeforeIdle));

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE(observerService, );
  observerService->AddObserver(this, "im-sent", PR_FALSE);

  mInitialized = PR_TRUE;
}

purpleIdleObserver::~purpleIdleObserver()
{
  LOG(("purpleIdleObserver destructor"));
  unInit();
}

// Called by purpleCoreService when idle reporting is disabled, or
// when libpurple is uninitialized.
void
purpleIdleObserver::unInit()
{
  LOG(("purpleIdleObserver::unInit"));
  if (!mInitialized)
    return;

  if (mTimeBeforeIdle) {
    nsCOMPtr<nsIIdleService> idleService =
      do_GetService("@mozilla.org/widget/idleservice;1");
    NS_ENSURE_TRUE(idleService, );
    idleService->RemoveIdleObserver(this, mTimeBeforeIdle);
  }

  nsCOMPtr<nsIPrefBranch2> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefBranch, );
  nsresult rv = prefBranch->RemoveObserver(PREF_TIME_BEFORE_IDLE, this);
  NS_ENSURE_SUCCESS(rv, );

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE(observerService, );
  observerService->RemoveObserver(this, "im-sent");

  mInitialized = PR_FALSE;
}

void
purpleIdleObserver::CheckIdle()
{
  LOG(("purpleIdleObserver::CheckIdle"));

  nsCOMPtr<nsIIdleService> idleService =
    do_GetService("@mozilla.org/widget/idleservice;1");
  NS_ENSURE_TRUE(idleService, );

  PRUint32 idleTime;
  nsresult rv = idleService->GetIdleTime(&idleTime);
  NS_ENSURE_SUCCESS(rv, );

  idleTime /= 1000; // idleTime is now in seconds.
  PRBool idle = idleTime >= PRUint32(mTimeBeforeIdle);
  LOG((" inactive for %u s, was %s, going %s", idleTime,
       mIdle ? "idle" : "unidle",
       idle  ? "idle" : "unidle"));
  if (idle == mIdle)
    return;

  mIdle = idle;
  purple_idle_set(idle ? idleTime : 0);

  PRBool changedStatus = PR_FALSE;
  if (!idle && purple_savedstatus_is_idleaway()) {
    purple_savedstatus_set_idleaway(FALSE, NULL);
    changedStatus = PR_TRUE;
  }
  else if (idle) {
    nsCOMPtr<nsIPrefBranch2> prefBranch =
      do_GetService(NS_PREFSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(prefBranch, );

    PRBool awayWhenIdle = PR_FALSE;
    nsresult rv = prefBranch->GetBoolPref(PREF_AWAY_WHEN_IDLE, &awayWhenIdle);
    NS_ENSURE_SUCCESS(rv, );

    if (awayWhenIdle) {
      nsCString message;
      nsCOMPtr<nsIPrefLocalizedString> messagePref;
      prefBranch->GetComplexValue(PREF_DEFAULT_MESSAGE,
                                  NS_GET_IID(nsIPrefLocalizedString),
                                  getter_AddRefs(messagePref));
      if (messagePref) {
        nsString value;
        messagePref->ToString(getter_Copies(value));
        if (!value.IsEmpty())
          CopyUTF16toUTF8(value, message);
      }

      purple_savedstatus_set_idleaway(TRUE, message.get());
      changedStatus = PR_TRUE;
    }
  }

  if (changedStatus) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    NS_ENSURE_TRUE(observerService, );

    // Get the purpleCoreService for aSubject parameter of the notification.
    nsCOMPtr<purpleICoreService> pcs =
      do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(pcs, );

    // Get the "new" current status message for the notification...
    const char *message =
      purple_savedstatus_get_message(purple_savedstatus_get_current());
    nsString wideMessage;
    NS_CStringToUTF16(nsDependentCString(message), NS_CSTRING_ENCODING_UTF8,
                      wideMessage);
    observerService->NotifyObservers(pcs, "status-changed", wideMessage.get());
  }
}

NS_IMETHODIMP
purpleIdleObserver::Observe(nsISupports *aSubject,
                            const char *aTopic,
                            const PRUnichar *aData)
{
  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    nsCOMPtr<nsIPrefBranch2> prefBranch = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(prefBranch, NS_ERROR_UNEXPECTED);

    PRInt32 idle;
    prefBranch->GetIntPref(PREF_TIME_BEFORE_IDLE, &idle);
    if (idle != mTimeBeforeIdle) {
      nsCOMPtr<nsIIdleService> idleService =
        do_GetService("@mozilla.org/widget/idleservice;1");
      NS_ENSURE_TRUE(idleService, NS_ERROR_UNEXPECTED);

      LOG(("purpleIdleObserver timeBeforeIdle changed: %i --> %i",
           mTimeBeforeIdle, idle));

      if (mTimeBeforeIdle)
        idleService->RemoveIdleObserver(this, mTimeBeforeIdle);
      mTimeBeforeIdle = idle;
      if (idle)
        return idleService->AddIdleObserver(this, mTimeBeforeIdle);
    }
    return NS_OK;
  }

  CheckIdle();
  return NS_OK;
}
