/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include<nsCOMPtr.h>
#include "nsCOMArray.h"
#include <glib.h>
#include <nsITimer.h>

#include <nsIRunnable.h>
class purpleTimeout MOZ_FINAL : public nsIRunnable
{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  purpleTimeout() {}
  PRUint32 mAccountId;
  PRUint32 mId;
  nsCOMPtr<nsITimer> mTimer;
  GSourceFunc mFunction;
  gpointer mData;

 private:
  ~purpleTimeout() {}
};

class purpleTimer
{
 public:
  static void init();
  static void unInit();
  static bool initialized() { return !!sTimeouts; }

  static PRUint32 AddTimeout(PRUint32 aInterval,
                             GSourceFunc aFunction,
                             gpointer aData);
  static void ExecTimer(nsITimer *aTimer, void *aTimerClosure);
  static gboolean CancelTimer(PRUint32 aId);
  static nsresult CancelTimerFromAccountId(PRUint32 aAccountId);

 private:
  static nsCOMArray<purpleTimeout> *sTimeouts;
  static PRUint32 sLastTimer;
};
