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

#include "purpleSockets.h"
#include <private/pprio.h>
#include <nsComponentManagerUtils.h>
#include <nsServiceManagerUtils.h>
#include <nsIProxyObjectManager.h>
#include <nsIEventTarget.h>
#include <nsIObserver.h>
#include <nsIObserverService.h>
#include <nsThreadUtils.h>
#include <nsNetUtil.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleSockets:5
//
static PRLogModuleInfo *gPurpleSocketsLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleSocketsLog, PR_LOG_DEBUG, args)

NS_IMPL_THREADSAFE_ISUPPORTS1(purpleSocket, purpleISocket)

//export NSPR_LOG_MODULES="nsSocketTransport:5"

PRUint32 purpleSocket::sLastSocket = 0;


class purpleSocketNetworkStateObserver : public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

purpleSocket::purpleSocket()
  : mFunction(NULL),
    mInternal(nsnull)
{
  LOG(("Creating purpleSocket @%x\n", this));
}

purpleSocket::~purpleSocket()
{
  LOG(("[%i] destructing purpleSocket @%x\n", mId, this));
  mFunction = NULL; // if we don't do this, C++ tries to delete the function
}

NS_IMETHODIMP purpleSocket::Init(PurpleInputFunction aFunction,
                                 gpointer aData, PRInt32 aFd,
                                 PurpleInputCondition aCondition,
                                 PRInt32 *aResultId)
{
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

  nsCOMPtr<nsIProxyObjectManager> pom = do_GetService("@mozilla.org/xpcomproxy;1");
  nsCOMPtr<purpleISocket> proxy;
  nsresult rv = pom->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                                       NS_GET_IID(purpleISocket),
                                       this,
                                       NS_PROXY_ASYNC | NS_PROXY_ALWAYS,
                                       getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  mInternal = new purpleSocketInternal(mId);
#else
  mInternal = new purpleSocketInternal();
#endif
  return mInternal->Init(PR_CreateSocketPollFd(mFd), proxy, pollFlags);
}

NS_IMETHODIMP purpleSocket::GetId(PRUint32 *aId)
{
  *aId = mId;
  return NS_OK;
}

NS_IMETHODIMP purpleSocket::Cancel()
{
  return mInternal->Cancel();
}

NS_IMETHODIMP purpleSocket::CallLibpurpleCallback()
{
  LOG(("[%i] CallLibpurpleCallback\n", mId));
  mFunction(mData, mFd, mCondition);

  return NS_OK;
}

NS_IMETHODIMP purpleSocket::NotifyLibPurple(PRInt16 aPollFlags)
{
  LOG(("[%i] NotifyLibPurple\n", mId));
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");

  if (mInternal->GetCanceled()) {
    // XXXFLo Need to make sure there can't be a race condition. Can
    // the watch be canceled between our call to GetCanceled and when
    // we return from mFunction?
    LOG(("[%i] --> Canceling, leaving!\n", mId));
    return NS_OK;
  }
  CallLibpurpleCallback();
  return mInternal->SetPollFlags(aPollFlags);
}


NS_IMPL_THREADSAFE_ISUPPORTS2(purpleSocketInternal, nsASocketHandler, nsIRunnable)

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

nsresult purpleSocketInternal::Init(PRFileDesc *aFd, purpleISocket *aProxy,
                                    PRUint16 aPollFlags)
{
  mFd = aFd;
  mProxy = aProxy;
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
  nsCOMPtr<nsIEventTarget> eventTarget = do_QueryInterface(purpleSocketWatcher::getSts(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = eventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  LOG(("[%i]  Dispatch %s\n", mId, NS_FAILED(rv) ? "Failed!!!" : "OK"));
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

  PRInt16 pollFlags = mPollFlags;
  mPollFlags = 0;

  NS_ASSERTION(!NS_IsMainThread(), "wrong thread");

  if (!mProxy) {
    LOG(("[%i]   mProxy is NULL\n", mId));
    return;
  }
  mProxy->NotifyLibPurple(pollFlags);
}

void purpleSocketInternal::OnSocketDetached(PRFileDesc*)
{
  LOG(("[%i]  OnSocketDetached\n", mId));
  mProxy = nsnull;
}

nsresult purpleSocketInternal::Run()
{
  LOG(("[%i]  Run called: ", mId));

  if (mDetached) {
    LOG(("already detached, leaving!\n"));
    return NS_OK;
  }

  if (mPollFlagsInternal) {
    if (mCanceled) {
      LOG(("already canceling, leaving!\n"));
      mPollFlagsInternal = 0;
      return NS_OK;
    }
    mPollFlags = mPollFlagsInternal;
    mPollFlagsInternal = 0;
    LOG(("pollFlags set\n"));
  }
  else if (mAttached) {
    mCondition = NS_ERROR_UNEXPECTED;
    mDetached = PR_TRUE;
    LOG(("detach requested\n"));
    return NS_OK;
  }

  if (!mAttached) {
    if (mCanceled) {
      // The watch on this socket has been canceled before we had
      // enought time to attach it. |OnSocketDetached| will never be
      // called because the socket hasn't been attached. We should
      // release the proxy to ensure this object can be deleted.
      mProxy = nsnull;
      LOG(("removed useless proxy\n"));
      return NS_OK;
    }
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

nsresult purpleSocketInternal::SetPollFlags(PRInt16 aPollFlags)
{
  LOG(("[%i]  trying to set pollflags to %i\n", mId, aPollFlags));
  if (mCanceled) {
    LOG(("[%i]  --> canceling, leaving!\n", mId));
    return NS_OK;
  }
  mPollFlagsInternal = aPollFlags;
  return PostEvent();
}

/* Init static members */
nsTArray<purpleISocket *> *purpleSocketWatcher::sSockets = nsnull;
nsCOMPtr<nsISocketTransportService> purpleSocketWatcher::sSts;
nsCOMPtr<nsIObserver> purpleSocketWatcher::sObserver;

void purpleSocketWatcher::init()
{
#ifdef PR_LOGGING
  if (!gPurpleSocketsLog)
    gPurpleSocketsLog = PR_NewLogModule("purpleSockets");
#endif
  sSockets = new nsTArray<purpleISocket *>();
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
  for (PRInt32 i = sSockets->Length() - 1; i >= 0; i = sSockets->Length() - 1) {
    nsCOMPtr<purpleISocket> socket = (*sSockets)[i];
    socket->CallLibpurpleCallback();
    socket->Cancel();
  }
  sSockets->Clear();

  sSts = nsnull;
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

    sObserver = nsnull;
  }

  LOG(("purpleSocketWatcher:unInit: Removing %i leftover watchers",
       sSockets->Length()));
  for (PRUint32 i = 0; i < sSockets->Length(); ++i)
    (*sSockets)[i]->Cancel();
  delete sSockets;
  sSockets = nsnull;

  sSts = nsnull;
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
  NS_ENSURE_TRUE(sSockets, NS_ERROR_FAILURE);
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  NS_ENSURE_TRUE(!NS_IsOffline(), 0);

  nsresult rv;
  nsCOMPtr<purpleISocket> socket = do_CreateInstance(PURPLE_SOCKET_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, 0);

  PRInt32 id;
  rv = socket->Init(aFunction, aData, aFd, aCondition, &id);
  NS_ENSURE_SUCCESS(rv, 0);

  sSockets->AppendElement(socket);
  LOG(("[%i]SOCKETADD fd = %i\n", id, aFd));

  return id;
}

PRBool purpleSocketWatcher::CancelWatch(PRUint32 aId)
{
  LOG(("[%u]SOCKETCANCEL\n", aId));
  NS_ENSURE_TRUE(sSockets, NS_ERROR_FAILURE);

  for (PRUint32 i = 0; i < sSockets->Length(); ++i) {
    PRUint32 id;
    (*sSockets)[i]->GetId(&id);
    if (id == aId) {
      (*sSockets)[i]->Cancel();
      sSockets->RemoveElementAt(i);
      LOG(("[%u]Socket elt found at index %i; canceled and removed\n", aId, i));
      return PR_TRUE;
    }
  }

  NS_WARNING("Failed to cancelWatch: socket not found");
  return PR_FALSE;
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
