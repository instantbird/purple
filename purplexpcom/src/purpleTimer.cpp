/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleTimer.h"
#include "purpleAccountScoper.h"
#include <prlog.h>
#include <nsComponentManagerUtils.h>
#include <nsServiceManagerUtils.h>
#include <nsIThreadManager.h>
#include <nsIEventTarget.h>
#include <nsThreadUtils.h>
#include <nsXPCOMCIDInternal.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleTimer:5
//
static PRLogModuleInfo *gPurpleTimerLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleTimerLog, PR_LOG_DEBUG, args)

/* Init static members */
PRUint32 purpleTimer::sLastTimer = 0;
nsTArray<purpleTimeout *> *purpleTimer::sTimeouts = nullptr;

void purpleTimer::init()
{
#ifdef PR_LOGGING
  if (!gPurpleTimerLog)
    gPurpleTimerLog = PR_NewLogModule("purpleTimer");
#endif
  sTimeouts = new nsTArray<purpleTimeout *>();
}

void purpleTimer::unInit()
{
  LOG(("purpleTimer::unInit: removing %i leftover timeouts",
       sTimeouts->Length()));

  for (PRUint32 i = 0; i < sTimeouts->Length(); ++i) {
    purpleTimeout *timeout = (*sTimeouts)[i];
    timeout->mTimer->Cancel();
    delete timeout;
  }

  delete sTimeouts;
  sTimeouts = nullptr;
}

NS_IMPL_ISUPPORTS1(purpleTimeout, nsIRunnable)

NS_IMETHODIMP purpleTimeout::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  NS_ENSURE_TRUE(purpleTimer::initialized(), NS_ERROR_UNEXPECTED);

  mFunction(mData);
  return NS_OK;
}

PRUint32 purpleTimer::AddTimeout(PRUint32 aInterval,
                                 GSourceFunc aFunction,
                                 gpointer aData)
{
  NS_ENSURE_TRUE(initialized(), 0);

  /* FIXME This is an horrible hack to prevent libpurple from writting
     prefs.xml, blist.xml, status.xml and pounces.xml

     Those files are all written with a callback with a timeout of 5
     secondes.

     The only other 5 seconds callback in libpurple is in
     oscar/peers.c and fortunately, it passes a non NULL data pointer.
  */
  if (aInterval == 5000 && aData == NULL) {
    /* assume this is a buddy list, accounts or prefs saving */
    LOG(("purpleTimer::AddTimeout : TIMEOUT REFUSED"));
    return 0;
  }

  purpleTimeout *timer = new purpleTimeout();
  timer->mFunction = aFunction;
  timer->mData = aData;

  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr = do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, 0);

  bool isMainThread;
  mgr->GetIsMainThread(&isMainThread);
  if (!isMainThread) {
    NS_ASSERTION(!aInterval, "attempting thread synchronisation with a non null timeout");

    nsCOMPtr<nsIThread> mainThread;
    rv = mgr->GetMainThread(getter_AddRefs(mainThread));
    NS_ENSURE_SUCCESS(rv, 0);

    mainThread->Dispatch(timer, NS_DISPATCH_NORMAL);
    // By the way, libpurple doesn't even seem to use the return value
    // when the timer is used for thread synchronisation
    return 0;
  }

  timer->mId = ++sLastTimer;
  timer->mAccountId = purpleAccountScoper::GetCurrentAccountId();
  timer->mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  timer->mTimer->InitWithFuncCallback(ExecTimer, timer,
                                      aInterval, nsITimer::TYPE_REPEATING_SLACK);
  sTimeouts->AppendElement(timer);
  LOG(("TIMEOUTADD id = %i; accountId = %u; interval = %ims\n",
       timer->mId, timer->mAccountId, aInterval));

  return timer->mId;
}

void purpleTimer::ExecTimer(nsITimer *aTimer, void *aTimerClosure)
{
  // just make sure we never call a callback of a timer after
  // libpurple has been uninitialized
  NS_ENSURE_TRUE(initialized(), );

  purpleTimeout *timeout = (purpleTimeout *)aTimerClosure;

  /* We need to cache the id before calling mFunction, otherwise if
     mFunction calls purple_timeout_remove, timeout will already be
     freed when we will try to call CancelTimer */
  PRUint32 id = timeout->mId;
  LOG(("TIMEOUT EXEC id = %i, accountId = %u", id, timeout->mAccountId));
  purpleAccountScoper scoper(timeout->mAccountId);
  gboolean shouldContinue = timeout->mFunction(timeout->mData);
  if (!shouldContinue) {
    CancelTimer(id);
  }
}

gboolean purpleTimer::CancelTimer(PRUint32 aId)
{
  NS_ENSURE_TRUE(initialized(), FALSE);

  LOG(("Trying to cancel timeout with id %i", aId));
  for (PRUint32 i = 0; i < sTimeouts->Length(); ++i) {
    if ((*sTimeouts)[i]->mId == aId) {
      purpleTimeout *timeout = (*sTimeouts)[i];
      timeout->mTimer->Cancel();
      delete timeout;
      sTimeouts->RemoveElementAt(i);
      LOG(("Timeout elt found at index %i; canceled and removed", i));
      return TRUE;
    }
  }

  LOG(("purpleTimer::CancelTimer : Timeout with id %i not found.", aId));
  return FALSE;
}

nsresult purpleTimer::CancelTimerFromAccountId(PRUint32 aAccountId)
{
  LOG(("Checking if timers from account %u remain", aAccountId));
  NS_ENSURE_TRUE(initialized(), NS_ERROR_FAILURE);

  for (PRInt32 i = sTimeouts->Length() - 1; i >= 0; --i) {
    if ((*sTimeouts)[i]->mAccountId == aAccountId) {
      purpleTimeout *timeout = (*sTimeouts)[i];
      timeout->mTimer->Cancel();
      delete timeout;
      sTimeouts->RemoveElementAt(i);
      LOG(("Timeout elt found at index %i; canceled and removed", i));
      NS_WARNING("Found timer for disconnected account");
    }
  }
  return NS_OK;
}
