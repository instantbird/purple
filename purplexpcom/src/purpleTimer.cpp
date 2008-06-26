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
#include<nsComponentManagerUtils.h>

//#define DEBUG_PURPLE_TIMER

#ifdef DEBUG_PURPLE_TIMER
# include<stdio.h>
#endif

/* Init static members */
PRUint32 purpleTimer::sLastTimer = 0;
nsTArray<purpleTimeout *> *purpleTimer::sTimeouts = nsnull;

void purpleTimer::init()
{
  sTimeouts = new nsTArray<purpleTimeout *>();
}

void purpleTimer::unInit()
{
  delete sTimeouts;
}

PRUint32 purpleTimer::AddTimeout(PRUint32 aInterval,
				   GSourceFunc aFunction,
				   gpointer aData)
{
  /* FIXME This is an horrible hack to prevent libpurple from writting
     prefs.xml, blist.xml, status.xml and pounces.xml

     Those files are all written with a callback with a timeout of 5
     secondes.

     The only other 5 seconds callback in libpurple is in
     oscar/peers.c and fortunately, it passes a non NULL data pointer.
  */
  if (aInterval == 5000 && aData == NULL) {
    /* assume this is a buddy list, accounts or prefs saving */
#ifdef DEBUG_PURPLE_TIMER
    printf("TIMEOUT REFUSED\n");
#endif
    return 0;
  }

  purpleTimeout *timer = new purpleTimeout();
  timer->mId = ++sLastTimer;
  timer->mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  timer->mFunction = aFunction;
  timer->mData = aData;
  timer->mTimer->InitWithFuncCallback(ExecTimer, timer,
				      aInterval, nsITimer::TYPE_REPEATING_SLACK);
  sTimeouts->AppendElement(timer);
#ifdef DEBUG_PURPLE_TIMER
  printf("TIMEOUTADD id = %i; interval = %ims\n", timer->mId, aInterval);
#endif

  return timer->mId;
}

void purpleTimer::ExecTimer(nsITimer *aTimer, void *aTimerClosure)
{
  purpleTimeout *timeout = (purpleTimeout *)aTimerClosure;

  /* We need to cache the id before calling mFunction, otherwise if
     mFunction calls purple_timeout_remove, timeout will already be
     freed when we will try to call CancelTimer */
  PRUint32 id = timeout->mId;
#ifdef DEBUG_PURPLE_TIMER
  printf("TIMEOUT EXEC id = %i\n", id);
#endif
  PRBool shouldContinue = timeout->mFunction(timeout->mData);
  if (!shouldContinue) {
    CancelTimer(id);
  }
}

PRBool purpleTimer::CancelTimer(PRUint32 aId)
{
#ifdef DEBUG_PURPLE_TIMER
  printf("Trying to cancel timeout with id %i\n", aId);
#endif
  for (PRUint32 i = 0; i < sTimeouts->Length(); ++i) {
    if ((*sTimeouts)[i]->mId == aId) {
      purpleTimeout *timeout = (*sTimeouts)[i];
      timeout->mTimer->Cancel();
      delete timeout;
      sTimeouts->RemoveElementAt(i);
#ifdef DEBUG_PURPLE_TIMER
      printf("Timeout elt found at index %i; canceled and removed\n", i);
#endif
      return PR_TRUE;
    }
  }

#ifdef DEBUG_PURPLE_TIMER
  printf("Timeout with id %i not found.\n", aId);
#endif
  return PR_FALSE;
}
