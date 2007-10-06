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
#include<nsComponentManagerUtils.h>
#include<nsServiceManagerUtils.h>
#include<nsIProxyObjectManager.h>

//#define DEBUG_PURPLE_SOCKET <-- Should be defined in purpleSockets.h
//because there are additionnal fields when it's defined

#ifdef DEBUG_PURPLE_SOCKET
# include<stdio.h>
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(purpleSocket, purpleISocket)

//export NSPR_LOG_MODULES="nsSocketTransport:5"

PRUint32 purpleSocket::sLastSocket = 0;

#ifdef DEBUG_PURPLE_SOCKET
PRUint32 purpleSocket::sInstances = 0;
#endif

purpleSocket::purpleSocket()
  : mFunction(NULL),
    mInternal(nsnull)    
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("constructing new purpleSocket\n");
  ++sInstances;
#endif
}

purpleSocket::~purpleSocket()
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i] destructing purpleSocket [remains: %u]\n", mId, --sInstances);
#endif
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

#ifdef DEBUG_PURPLE_SOCKET
  mInternal = new purpleSocketInternal(mId);
#else
  NS_NEWXPCOM(mInternal, purpleSocketInternal);
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

nsresult purpleSocket::NotifyLibPurple(PRInt16 aPollFlags, PRInt16 aOutFlags)
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i] NotifyLibPurple\n", mId);
#endif

  NS_ASSERTION(NS_IsMainThread(), "wrong thread");

  if (mInternal->GetCanceled()) {
    // XXXFLo Need to make sure there can't be a race condition. Can
    // the watch be canceled between our call to GetCanceled and when
    // we return from mFunction?
#ifdef DEBUG_PURPLE_SOCKET
    printf("[%i] --> Canceling, leaving!\n", mId);
#endif
    return NS_OK;
  }
  mFunction(mData, mFd, mCondition);
  return mInternal->SetPollFlags(aPollFlags);
}


NS_IMPL_THREADSAFE_ISUPPORTS2(purpleSocketInternal, nsASocketHandler, nsIRunnable)

#ifdef DEBUG_PURPLE_SOCKET
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
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i]  constructing new purpleSocketInternal\n", mId);
#endif
}

purpleSocketInternal::~purpleSocketInternal()
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i]  destructing purpleSocketInternal\n", mId);
#endif
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
  mCanceled = PR_TRUE;
  return PostEvent();
}

nsresult purpleSocketInternal::PostEvent()
{
  nsresult rv;
  nsCOMPtr<nsIEventTarget> eventTarget = do_QueryInterface(purpleSocketWatcher::getSts(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = eventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
#ifdef DEBUG_PURPLE_SOCKET
  if (NS_FAILED(rv))
    printf("[%i]  Dispatch Failed!!!\n", mId);
  else
    printf("[%i]  Dispatch OK\n", mId);
#endif
  return rv;
}

void purpleSocketInternal::OnSocketReady(PRFileDesc* fd, PRInt16 outFlags)
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i]  OnSocketReady\n", mId);
#endif

  if (mDetached || mCanceled) {
#ifdef DEBUG_PURPLE_SOCKET
    printf("[%i]   --> detach in progress, leaving\n", mId);
#endif
    return;
  }

//   if (outFlags & (PR_POLL_ERR | PR_POLL_HUP | PR_POLL_NVAL)) {
//     printf("[%i]   error polling on listening socket\n", mId);
//     mCondition = NS_ERROR_UNEXPECTED;
//     return;
//   }

  PRInt16 pollFlags = mPollFlags;
  mPollFlags = 0;

  NS_ASSERTION(!NS_IsMainThread(), "wrong thread");

  if (!mProxy) {
#ifdef DEBUG_PURPLE_SOCKET
    printf("[%i]   mProxy is NULL\n", mId);
#endif
    return;
  }
  mProxy->NotifyLibPurple(pollFlags, outFlags);
}

void purpleSocketInternal::OnSocketDetached(PRFileDesc*)
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i]  OnSocketDetached\n", mId);
#endif
  mProxy = nsnull;
}

nsresult purpleSocketInternal::Run()
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i]  Run called: ", mId);
#endif

  if (mDetached) {
#ifdef DEBUG_PURPLE_SOCKET
    printf("already detached, leaving!\n");
#endif
    return NS_OK;
  }

  if (mPollFlagsInternal) {
    if (mCanceled) {
#ifdef DEBUG_PURPLE_SOCKET
      printf("already canceling, leaving!\n");
#endif
      mPollFlagsInternal = 0;
      return NS_OK;
    }
    mPollFlags = mPollFlagsInternal;
    mPollFlagsInternal = 0;
#ifdef DEBUG_PURPLE_SOCKET
    printf("pollFlags set\n");
#endif
  }
  else if (mAttached) {
    mCondition = NS_ERROR_UNEXPECTED;
    mDetached = PR_TRUE;
#ifdef DEBUG_PURPLE_SOCKET
    printf("detach requested\n");
#endif
    return NS_OK;
  }

  if (!mAttached) {
    if (mCanceled) {
      // The watch on this socket has been canceled before we had
      // enought time to attach it. |OnSocketDetached| will never be
      // called because the socket hasn't been attached. We should
      // release the proxy to ensure this object can be deleted.
      mProxy = nsnull;
#ifdef DEBUG_PURPLE_SOCKET
      printf("removed useless proxy\n");
#endif
      return NS_OK;
    }
    mAttached = PR_TRUE;
#ifdef DEBUG_PURPLE_SOCKET
    printf("attach requested\n");
#endif
    return OnAttach();
  }

  return NS_OK;
}

nsresult purpleSocketInternal::OnAttach()
{
  nsPISocketTransportService *sts = purpleSocketWatcher::getSts();

  nsresult rv = sts->AttachSocket(mFd, this);
  if (NS_FAILED(rv)) {
#ifdef DEBUG_PURPLE_SOCKET
    printf("cannot attach purpleSocket\n");
#endif
  }
  else {
#ifdef DEBUG_PURPLE_SOCKET
  printf("socket attached\n");
#endif
}
  return rv;
}

nsresult purpleSocketInternal::SetPollFlags(PRInt16 aPollFlags)
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i]  trying to set pollflags to %i\n", mId, aPollFlags);
#endif
  if (mCanceled) {
#ifdef DEBUG_PURPLE_SOCKET
    printf("[%i]  --> canceling, leaving!\n", mId);
#endif
    return NS_OK;
  }
  mPollFlagsInternal = aPollFlags;
  return PostEvent();
}

/* Init static members */
nsTArray<purpleISocket *> purpleSocketWatcher::sSockets;
nsCOMPtr<nsPISocketTransportService> purpleSocketWatcher::sSts;

nsPISocketTransportService *purpleSocketWatcher::getSts()
{
  if (!sSts) {
    nsresult rv;
    sSts = do_GetService("@mozilla.org/network/socket-transport-service;1", &rv);
#ifdef DEBUG_PURPLE_SOCKET
    if (NS_FAILED(rv))
      printf("cannot get nsSocketTransportService\n");
    else
      printf("got service nsSocketTransportService\n");
#endif
  }

  return sSts;
};

PRUint32 purpleSocketWatcher::AddWatch(gint aFd, PurpleInputCondition aCondition,
				       PurpleInputFunction aFunction, gpointer aData)
{
  nsresult rv;

  NS_ASSERTION(NS_IsMainThread(), "wrong thread");

  nsCOMPtr<purpleISocket> socket = do_CreateInstance(PURPLE_SOCKET_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return 0;

  PRInt32 id;
  rv = socket->Init(aFunction, aData, aFd, aCondition, &id);
  if (NS_FAILED(rv))
    return 0;

  sSockets.AppendElement(socket);
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i]SOCKETADD fd = %i\n", id, aFd);
#endif

  return id;
}

PRBool purpleSocketWatcher::CancelWatch(PRUint32 aId)
{
#ifdef DEBUG_PURPLE_SOCKET
  printf("[%u]SOCKETCANCEL\n", aId);
#endif

  for (PRUint32 i = 0; i < sSockets.Length(); ++i) {
    PRUint32 id;
    sSockets[i]->GetId(&id);
    if (id == aId) {
      sSockets[i]->Cancel();
      sSockets.RemoveElementAt(i);
#ifdef DEBUG_PURPLE_SOCKET
      printf("[%u]Socket elt found at index %i; canceled and removed\n", aId, i);
#endif
      return PR_TRUE;
    }
  }

#ifdef DEBUG_PURPLE_SOCKET
  printf("[%i]Not found.\n", aId);
#endif
  return PR_FALSE;
}
