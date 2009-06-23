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
 * 2007.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#include "purpleTimer.h"
#include <prlog.h>
#include <nsComponentManagerUtils.h>

#ifdef XP_WIN
#include <nsServiceManagerUtils.h>
#include <nsIThreadManager.h>
#include <nsIEventTarget.h>
#include <nsThreadUtils.h>
#include <nsXPCOMCIDInternal.h>
#endif

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleTimer:5
//
static PRLogModuleInfo *gPurpleTimerLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleTimerLog, PR_LOG_DEBUG, args)

/* Init static members */
PRUint32 purpleTimer::sLastTimer = 0;
nsTArray<purpleTimeout *> *purpleTimer::sTimeouts = nsnull;

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
  LOG(("purpleTimer::unInit: removing %s leftover timeouts",
       sTimeouts->Length()));

  for (PRUint32 i = 0; i < sTimeouts->Length(); ++i) {
    purpleTimeout *timeout = (*sTimeouts)[i];
    timeout->mTimer->Cancel();
    delete timeout;
  }

  delete sTimeouts;
  sTimeouts = nsnull;
}

#ifdef XP_WIN
NS_IMPL_THREADSAFE_ISUPPORTS1(purpleTimeout, nsIRunnable)

NS_IMETHODIMP purpleTimeout::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");

  mFunction(mData);
  return NS_OK;
}
#endif

PRUint32 purpleTimer::AddTimeout(PRUint32 aInterval,
                                 GSourceFunc aFunction,
                                 gpointer aData)
{
  NS_ENSURE_TRUE(sTimeouts, 0);

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

#ifdef XP_WIN
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr = do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, 0);

  PRBool isMainThread;
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
#endif

  timer->mId = ++sLastTimer;
  timer->mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  timer->mTimer->InitWithFuncCallback(ExecTimer, timer,
                                      aInterval, nsITimer::TYPE_REPEATING_SLACK);
  sTimeouts->AppendElement(timer);
  LOG(("TIMEOUTADD id = %i; interval = %ims\n", timer->mId, aInterval));

  return timer->mId;
}

void purpleTimer::ExecTimer(nsITimer *aTimer, void *aTimerClosure)
{
  // just make sure we never call a callback of a timer after
  // libpurple has been uninitialized
  NS_ENSURE_TRUE(sTimeouts, );

  purpleTimeout *timeout = (purpleTimeout *)aTimerClosure;

  /* We need to cache the id before calling mFunction, otherwise if
     mFunction calls purple_timeout_remove, timeout will already be
     freed when we will try to call CancelTimer */
  PRUint32 id = timeout->mId;
  LOG(("TIMEOUT EXEC id = %i", id));
  PRBool shouldContinue = timeout->mFunction(timeout->mData);
  if (!shouldContinue) {
    CancelTimer(id);
  }
}

PRBool purpleTimer::CancelTimer(PRUint32 aId)
{
  NS_ENSURE_TRUE(sTimeouts, PR_FALSE);

  LOG(("Trying to cancel timeout with id %i", aId));
  for (PRUint32 i = 0; i < sTimeouts->Length(); ++i) {
    if ((*sTimeouts)[i]->mId == aId) {
      purpleTimeout *timeout = (*sTimeouts)[i];
      timeout->mTimer->Cancel();
      delete timeout;
      sTimeouts->RemoveElementAt(i);
      LOG(("Timeout elt found at index %i; canceled and removed", i));
      return PR_TRUE;
    }
  }

  LOG(("purpleTimer::CancelTimer : Timeout with id %i not found.", aId));
  return PR_FALSE;
}
