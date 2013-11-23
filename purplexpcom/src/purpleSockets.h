/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include<nsCOMPtr.h>
#include<nsAutoPtr.h>
#include <nsTArray.h>
#include <glib.h>
#pragma GCC visibility push(default)
#include <libpurple/eventloop.h>
#pragma GCC visibility pop
#include<nsStringAPI.h>

#include<prio.h>
#include<nsIRunnable.h>
#include<nsASocketHandler.h>
#include<nsISocketTransportService.h>

class nsIObserver;

class purpleSocketInternal MOZ_FINAL : public nsASocketHandler,
  public nsIRunnable
{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsresult Init(PRFileDesc *aFd, nsIRunnable *aRunnable, PRUint16 aPollFlags);
  void OnSocketReady(PRFileDesc*, PRInt16);
  void OnSocketDetached(PRFileDesc*);
  void IsLocal(bool*);
  uint64_t ByteCountSent() { return 0; }
  uint64_t ByteCountReceived() { return 0; }
  nsresult PostEvent();
  nsresult Cancel();
  bool GetCanceled() { return mCanceled; }
#ifdef PR_LOGGING
  purpleSocketInternal(PRUint32 aId);
#else
  purpleSocketInternal();
#endif
  ~purpleSocketInternal();
#ifdef PR_LOGGING
  PRUint32 mId;
#endif

 private:
  nsresult OnAttach();
  PRFileDesc *mFd;
  PRInt16 mPollFlagsInternal;
  nsCOMPtr<nsIRunnable> mRunnable;
  bool mAttached;
  bool mDetached;
  bool mCanceled;
};

class purpleSocket MOZ_FINAL : public nsIRunnable
{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  purpleSocket();
  ~purpleSocket();

  nsresult Init(PurpleInputFunction aFunction, gpointer aData, PRInt32 aFd,
                PurpleInputCondition aCondition, PRInt32 *aResultId);
  void CallLibpurpleCallback();
  void Cancel() { mInternal->Cancel(); }
  PRUint32 GetId() { return mId; }
  PRUint32 GetAccountId() { return mAccountId; }

 private:
  static PRUint32 sLastSocket;
  PRUint32 mAccountId;
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
  static gboolean CancelWatch(PRUint32 aId);
  static nsresult CancelWatchFromAccountId(PRUint32 aAccountId);
  static nsISocketTransportService *getSts();
  static void init();
  static void unInit();
  static void goingOffline();

 private:
  static nsTArray<purpleSocket *> *sSockets;
  static nsCOMPtr<nsISocketTransportService> sSts;
  static nsCOMPtr<nsIObserver> sObserver;
};
