/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleSockets.h"
#include "purpleAccountScoper.h"
#include <private/pprio.h>
#include <nsComponentManagerUtils.h>
#include <nsServiceManagerUtils.h>
#include <nsIEventTarget.h>
#include <nsIObserver.h>
#include <nsIObserverService.h>
#include <nsThreadUtils.h>
#include <nsNetUtil.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleSockets:5
//
static PRLogModuleInfo *gPurpleSocketsLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleSocketsLog, PR_LOG_DEBUG, args)

NS_IMPL_ISUPPORTS1(purpleSocket, nsIRunnable)

//export NSPR_LOG_MODULES="nsSocketTransport:5"

PRUint32 purpleSocket::sLastSocket = 0;


class purpleSocketNetworkStateObserver MOZ_FINAL : public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

purpleSocket::purpleSocket()
  : mFunction(NULL)
{
  LOG(("Creating purpleSocket @%x\n", this));
}

purpleSocket::~purpleSocket()
{
  LOG(("[%i] destructing purpleSocket @%x\n", mId, this));
  mFunction = NULL; // if we don't do this, C++ tries to delete the function
}

nsresult purpleSocket::Init(PurpleInputFunction aFunction, gpointer aData,
                            PRInt32 aFd, PurpleInputCondition aCondition,
                            PRInt32 *aResultId)
{
  mAccountId = purpleAccountScoper::GetCurrentAccountId();
  mId = ++sLastSocket;
  *aResultId = mId;
  mFunction = aFunction;
  mData = aData;
  mFd = aFd;
  mCondition = aCondition;
  PRUint16 pollFlags = 0;
  if (aCondition & PURPLE_INPUT_READ) {
    pollFlags |= (PR_POLL_READ | PR_POLL_EXCEPT);
  }

  if (aCondition & PURPLE_INPUT_WRITE) {
    pollFlags |= (PR_POLL_WRITE | PR_POLL_EXCEPT);
  }

#ifdef PR_LOGGING
  mInternal = new purpleSocketInternal(mId);
#else
  mInternal = new purpleSocketInternal();
#endif
  return mInternal->Init(PR_CreateSocketPollFd(mFd), this, pollFlags);
}

void purpleSocket::CallLibpurpleCallback()
{
  LOG(("[%i] CallLibpurpleCallback (accountId = %u)\n", mId, mAccountId));

  purpleAccountScoper scoper(mAccountId);
  mFunction(mData, mFd, mCondition);
}

NS_IMETHODIMP purpleSocket::Run()
{
  LOG(("[%i] purpleSocket::Run\n", mId));
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");

  if (mInternal->GetCanceled()) {
    LOG(("[%i] --> already canceled before notifying libpurple, ignore!\n", mId));
    return NS_OK;
  }

  CallLibpurpleCallback();

  if (mInternal->GetCanceled()) {
    LOG(("[%i] --> the callback canceled the watcher, leaving!\n", mId));
    return NS_OK;
  }

  return mInternal->PostEvent();
}


NS_IMPL_ISUPPORTS2(purpleSocketInternal, nsASocketHandler, nsIRunnable)

#ifdef PR_LOGGING
purpleSocketInternal::purpleSocketInternal(PRUint32 aId)
  : mId(aId),
#else
purpleSocketInternal::purpleSocketInternal()
  :
#endif
  mAttached(PR_FALSE),
  mDetached(PR_FALSE),
  mCanceled(PR_FALSE)
{
  MOZ_COUNT_CTOR(purpleSocketInternal);
  LOG(("[%i]  constructing new purpleSocketInternal @%x\n", mId, this));
}

purpleSocketInternal::~purpleSocketInternal()
{
  MOZ_COUNT_DTOR(purpleSocketInternal);
  LOG(("[%i]  destructing purpleSocketInternal @%x\n", mId, this));
  PR_DestroySocketPollFd(mFd);
}

nsresult purpleSocketInternal::Init(PRFileDesc *aFd, nsIRunnable *aRunnable,
                                    PRUint16 aPollFlags)
{
  mFd = aFd;
  mRunnable = aRunnable;
  mPollFlagsInternal = aPollFlags;
  return PostEvent();
}

nsresult purpleSocketInternal::Cancel()
{
  LOG(("[%i]  Cancel\n", mId));
  if (mCanceled) {
    LOG(("[%i]  Already Canceled\n", mId));
    return NS_OK;
  }
  mCanceled = PR_TRUE;
  return PostEvent();
}

nsresult purpleSocketInternal::PostEvent()
{
  nsresult rv;
  nsCOMPtr<nsIEventTarget> eventTarget =
    do_QueryInterface(purpleSocketWatcher::getSts(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  // 'this' may be released and freed on the socket thread before we
  // return from the Dispatch call, so cache the value of mId.
  PRUint32 id = mId;
#endif
  rv = eventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  LOG(("[%i]  Dispatch %s\n", id, NS_FAILED(rv) ? "Failed!!!" : "OK"));
  return rv;
}

void purpleSocketInternal::OnSocketReady(PRFileDesc* fd, PRInt16 outFlags)
{
  LOG(("[%i]  OnSocketReady\n", mId));

  if (mDetached || mCanceled) {
    LOG(("[%i]   --> detach in progress, leaving\n", mId));
    return;
  }

//   if (outFlags & (PR_POLL_ERR | PR_POLL_HUP | PR_POLL_NVAL)) {
//     LOG(("[%i]   error polling on listening socket\n", mId));
//     mCondition = NS_ERROR_UNEXPECTED;
//     return;
//   }

  mPollFlagsInternal = mPollFlags;
  mPollFlags = 0;

  NS_ASSERTION(!NS_IsMainThread(), "wrong thread");

  if (!mRunnable) {
    LOG(("[%i]   mRunnable is NULL\n", mId));
    return;
  }
  NS_DispatchToMainThread(mRunnable);
}

void purpleSocketInternal::OnSocketDetached(PRFileDesc*)
{
  LOG(("[%i]  OnSocketDetached\n", mId));
  mRunnable = nullptr;
}

NS_IMETHODIMP purpleSocketInternal::Run()
{
  LOG(("[%i]  Run called: ", mId));

  if (mDetached) {
    LOG(("already detached, leaving!\n"));
    return NS_OK;
  }

  if (mCanceled) {
    if (!mAttached) {
      // The watch on this socket has been canceled before we had
      // enought time to attach it. |OnSocketDetached| will never be
      // called because the socket hasn't been attached. We should
      // release the runnable to ensure it can be deleted.
      mRunnable = nullptr;
      LOG(("removed useless runnable\n"));
    }
    else {
      mCondition = NS_ERROR_UNEXPECTED;
      LOG(("detach requested\n"));
    }
    mDetached = PR_TRUE;
    return NS_OK;
  }

  if (mPollFlagsInternal) {
    mPollFlags = mPollFlagsInternal;
    mPollFlagsInternal = 0;
    LOG(("pollFlags set\n"));
  }

  if (!mAttached) {
    mAttached = PR_TRUE;
    LOG(("attach requested\n"));
    return OnAttach();
  }

  return NS_OK;
}

nsresult purpleSocketInternal::OnAttach()
{
  nsISocketTransportService *sts = purpleSocketWatcher::getSts();

  nsresult rv = sts->AttachSocket(mFd, this);
  if (NS_FAILED(rv)) {
    NS_WARNING("cannot attach purpleSocket\n");
  }
  else {
    LOG(("socket attached\n"));
  }
  return rv;
}

void purpleSocketInternal::IsLocal(bool *aIsLocal)
{
  *aIsLocal = false;
}

/* Init static members */
nsTArray<purpleSocket *> *purpleSocketWatcher::sSockets = nullptr;
nsCOMPtr<nsISocketTransportService> purpleSocketWatcher::sSts;
nsCOMPtr<nsIObserver> purpleSocketWatcher::sObserver;

void purpleSocketWatcher::init()
{
#ifdef PR_LOGGING
  if (!gPurpleSocketsLog)
    gPurpleSocketsLog = PR_NewLogModule("purpleSockets");
#endif
  sSockets = new nsTArray<purpleSocket *>();
  sObserver = new purpleSocketNetworkStateObserver();
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    observerService->AddObserver(sObserver,
                                 NS_IOSERVICE_GOING_OFFLINE_TOPIC,
                                 PR_FALSE);
  }
}

void purpleSocketWatcher::goingOffline()
{
  LOG(("purpleSocketWatcher:unInit: Removing %i pending watchers",
       sSockets->Length()));

  // We don't loop from 0 to Length - 1 because the libpurple callback
  // can cancel some watchers so the length of the array may change
  // during the loop. See bug 158
  for (PRInt32 i = sSockets->Length(); i; i = sSockets->Length()) {
    nsCOMPtr<purpleSocket> socket = (*sSockets)[i - 1];
    sSockets->RemoveElementAt(i - 1);
    socket->CallLibpurpleCallback();
    socket->Cancel();
  }

  sSts = nullptr;
}

void purpleSocketWatcher::unInit()
{
  if (sObserver) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      observerService->RemoveObserver(sObserver,
                                      NS_IOSERVICE_GOING_OFFLINE_TOPIC);
    }

    sObserver = nullptr;
  }

  LOG(("purpleSocketWatcher:unInit: Removing %i leftover watchers",
       sSockets->Length()));
  for (PRUint32 i = 0; i < sSockets->Length(); ++i)
    (*sSockets)[i]->Cancel();
  delete sSockets;
  sSockets = nullptr;

  sSts = nullptr;
}

nsISocketTransportService *purpleSocketWatcher::getSts()
{
  if (!sSts) {
    nsresult rv;
    sSts = do_GetService("@mozilla.org/network/socket-transport-service;1", &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "could not get nsSocketTransportService\n");
  }

  return sSts;
}

PRUint32 purpleSocketWatcher::AddWatch(gint aFd, PurpleInputCondition aCondition,
                                       PurpleInputFunction aFunction, gpointer aData)
{
  NS_ENSURE_TRUE(sSockets, 0);
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  NS_ENSURE_TRUE(!NS_IsOffline(), 0);

  nsresult rv;
  purpleSocket *socket = new purpleSocket();
  NS_ENSURE_TRUE(socket, 0);

  PRInt32 id;
  rv = socket->Init(aFunction, aData, aFd, aCondition, &id);
  NS_ENSURE_SUCCESS(rv, 0);

  sSockets->AppendElement(socket);

#ifdef DEBUG
  const char *mode = "none";
  if ((aCondition & PURPLE_INPUT_READ) && (aCondition & PURPLE_INPUT_WRITE))
    mode = "read/write";
  else if (aCondition & PURPLE_INPUT_READ)
    mode = "read";
  else if (aCondition & PURPLE_INPUT_WRITE)
    mode = "write";

  LOG(("[%i]SOCKETADD fd = %i, accountId = %u, mode = %s\n",
       id, aFd, purpleAccountScoper::GetCurrentAccountId(), mode));
#endif

  return id;
}

gboolean purpleSocketWatcher::CancelWatch(PRUint32 aId)
{
  LOG(("[%u]SOCKETCANCEL\n", aId));
  NS_ENSURE_TRUE(sSockets, FALSE);

  for (PRUint32 i = 0; i < sSockets->Length(); ++i) {
    PRUint32 id = (*sSockets)[i]->GetId();
    if (id == aId) {
      (*sSockets)[i]->Cancel();
      sSockets->RemoveElementAt(i);
      LOG(("[%u]Socket elt found at index %i; canceled and removed\n", aId, i));
      return TRUE;
    }
  }

  NS_WARNING("Failed to cancelWatch: socket not found");
  return FALSE;
}

nsresult purpleSocketWatcher::CancelWatchFromAccountId(PRUint32 aAccountId)
{
  LOG(("[ ]SOCKETCANCELFROMACCOUNTID, accountId = %u\n", aAccountId));
  NS_ENSURE_TRUE(sSockets, NS_ERROR_FAILURE);

  for (PRInt32 i = sSockets->Length() - 1; i >= 0; --i) {
    PRUint32 accountId = (*sSockets)[i]->GetAccountId();
    if (accountId == aAccountId) {
      (*sSockets)[i]->Cancel();
      sSockets->RemoveElementAt(i);
      LOG(("[ ]Socket from account %u found at index %i; canceled and removed\n",
           aAccountId, i));
      NS_WARNING("Found socket listener for disconnected account");
    }
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(purpleSocketNetworkStateObserver, nsIObserver)

NS_IMETHODIMP
purpleSocketNetworkStateObserver::Observe(nsISupports *aSubject,
                                          const char *aTopic,
                                          const PRUnichar *aData)
{
  LOG(("Got notified of %s", aTopic));
  purpleSocketWatcher::goingOffline();

  return NS_OK;
}
