#include "purpleTimer.h"
#include<nsComponentManagerUtils.h>

//#define DEBUG_PURPLE_TIMER

#ifdef DEBUG_PURPLE_TIMER
# include<stdio.h>
#endif

/* Init static members */
PRUint32 purpleTimer::mLastTimer = 0;
nsTArray<purpleTimeout *> purpleTimer::mTimeouts;

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
  timer->mId = ++mLastTimer;
  timer->mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  timer->mFunction = aFunction;
  timer->mData = aData;
  timer->mTimer->InitWithFuncCallback(ExecTimer, timer,
				      aInterval, nsITimer::TYPE_REPEATING_SLACK);
  mTimeouts.AppendElement(timer);
#ifdef DEBUG_PURPLE_TIMER
  printf("TIMEOUTADD id = %i; interval = %ims\n", timer->mId, aInterval);
#endif

  return timer->mId;
}

void purpleTimer::ExecTimer(nsITimer *aTimer, void *aTimerClosure)
{
  purpleTimeout *timeout = (purpleTimeout *)aTimerClosure;
#ifdef DEBUG_PURPLE_TIMER
  printf("TIMEOUT EXEC id = %i\n", timeout->mId);
#endif
  PRBool shouldContinue = timeout->mFunction(timeout->mData);
  if (!shouldContinue) {
    CancelTimer(timeout->mId);
  }
}

PRBool purpleTimer::CancelTimer(PRUint32 aId)
{
#ifdef DEBUG_PURPLE_TIMER
  printf("Trying to cancel timeout with id %i\n", aId);
#endif
  for (PRUint32 i = 0; i < mTimeouts.Length(); ++i) {
    if (mTimeouts[i]->mId == aId) {
      purpleTimeout *timeout = mTimeouts[i];
      timeout->mTimer->Cancel();
      delete timeout;
      mTimeouts.RemoveElementAt(i);
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
