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

#include<nsCOMPtr.h>
#include<nsAutoPtr.h>
#include <nsTArray.h>
#include <glib.h>
#pragma GCC visibility push(default)
#include <libpurple/eventloop.h>
#pragma GCC visibility pop
#include<nsStringAPI.h>
#include"purpleISocket.h"

#include<prio.h>
#include<nsIRunnable.h>
#include<necko/nsASocketHandler.h>
#include<necko/nsISocketTransportService.h>

// 2e8f828f-2b2f-4a82-9efe-3cf8d983fbbf
#define PURPLE_SOCKET_CID \
{ 0x2e8f828f, 0x2b2f, 0x4a82, \
  { 0x9e, 0xfe, 0x3c, 0xf8, 0xd9, 0x83, 0xfb, 0xbf } \
 }

#define PURPLE_SOCKET_CONTRACTID \
    "@instantbird.org/purple/socket;1"

class purpleSocketInternal : public nsASocketHandler,
  public nsIRunnable
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsresult Init(PRFileDesc *aFd, purpleISocket *aProxy, PRUint16 aPollFlags);
  void OnSocketReady(PRFileDesc*, PRInt16);
  void OnSocketDetached(PRFileDesc*);
  nsresult PostEvent();
  nsresult Cancel();
  PRBool GetCanceled() { return mCanceled; }
#ifdef DEBUG
  purpleSocketInternal(PRUint32 aId);
#else
  purpleSocketInternal();
#endif
  ~purpleSocketInternal();
  nsresult SetPollFlags(PRInt16 aPollFlags);
#ifdef DEBUG
  PRUint32 mId;
#endif

 private:
  nsresult OnAttach();
  PRFileDesc *mFd;
  PRInt16 mPollFlagsInternal;
  nsCOMPtr<purpleISocket> mProxy;
  PRBool mAttached;
  PRBool mDetached;
  PRBool mCanceled;
};

class purpleSocket : public purpleISocket
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEISOCKET

  purpleSocket();
  ~purpleSocket();
 private:
  static PRUint32 sLastSocket;
  PRUint32 mId;
  PRInt32 mFd;
  PurpleInputFunction mFunction;
  gpointer mData;
  PurpleInputCondition mCondition;
  nsRefPtr<purpleSocketInternal> mInternal;
};

typedef void (purpleSocket:: *purpleSocketFunc)(void);

class purpleSocketWatcher
{
 public:
  static PRUint32 AddWatch(gint aFd, PurpleInputCondition aCondition,
			   PurpleInputFunction aFunction, gpointer aData);
  static PRBool CancelWatch(PRUint32 aId);
  static nsISocketTransportService *getSts();
  static void init();
  static void unInit();

 private:
  static nsTArray<purpleISocket *> *sSockets;
  static nsCOMPtr<nsISocketTransportService> sSts;
};
