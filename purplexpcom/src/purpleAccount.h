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

#ifndef PURPLE_ACCOUNT_H_
# define PURPLE_ACCOUNT_H_

#include "purpleIAccount.h"
#include "purpleProtocol.h"
#include "nsCOMPtr.h"
#include<nsIPrefBranch.h>
#include<nsITimer.h>
#include<nsStringAPI.h>
#include<prtime.h>

#define UI_ID                  "instantbird"

// e450cf22-b98a-4095-b480-e57caf58a2eb
#define PURPLE_ACCOUNT_CID \
{ 0xe450cf22, 0xb98a, 0x4095, \
  { 0xb4, 0x80, 0xe5, 0x7c, 0xaf, 0x58, 0xa2, 0xeb } \
 }

#define PURPLE_ACCOUNT_CONTRACTID \
    "@instantbird.org/purple/account;1"

#define ACCOUNT_KEY "account"
#define PREF_AUTOLOGIN_PENDING  "messenger.accounts.autoLoginPending"

class purpleAccount : public purpleIAccount
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIACCOUNT

  purpleAccount();
  const nsCString& GetKey() { return mKey; }
  PRUint32 GetId() { return mId; }
  PurpleAccount *GetPurpleAccount() { return mAccount; }
  nsresult create(const nsACString& name, purpleProtocol *prpl,
                  const nsCString& aKey);
  nsresult load(const nsCString& aKey);
  nsresult checkAutoLogin();
  nsresult remove();
  void UnInit();
  void SetConnectionStateMsg(const char *aConnectionStateMsg);
  void SetDisconnectReason(PurpleConnectionError reason, const char *text);
  // Called when an account successfully connected, to reset values
  // related to connection error and auto-reconnect.
  void Connected();

private:
  ~purpleAccount();
  inline nsresult SetId(const nsCString& aId);
  nsresult sendUpdateNotification();
  inline nsresult GetPrefBranch();
  nsresult ensureStored(const nsCString& aName, const nsCString& aPrpl);
  nsresult storeAccount(PRUint32 aKey, const nsCString& aName, const nsCString& aPrpl);
  nsresult unstoreAccount();
  nsresult SetBoolPref(const char *aName, PRBool aValue);
  nsresult SetIntPref(const char *aName, PRInt32 aValue);
  nsresult SetStringPref(const char *aName, const char *aValue);
  inline void GetBoolPref(const char *aName);
  inline void GetIntPref(const char *aName);
  inline void GetCharPref(const char *aName);

  PurpleAccount *mAccount;
  nsCString mKey;
  PRUint32 mId;
  nsCOMPtr<purpleProtocol> mProtocol;
  nsCOMPtr<purpleIProxyInfo> mProxy;
  nsCOMPtr<nsIPrefBranch> mPrefBranch;
  nsCOMPtr<nsIPrefBranch> mPrefOptBranch;

protected:
  /* additional members */
  nsCString mConnectionStateMsg;
  nsCString mConnectionErrorMessage;
  PurpleConnectionError mConnectionErrorReason;

  /* Automatic reconnection stuff */
  nsCOMPtr<nsITimer> mTimer;
  static void reconnectTimerCallback(nsITimer *aTimer, void *aClosure);
  PRUint16 mReconnectAttempts;
  PRTime mTimeOfNextReconnect;

  /* Timer to save preferences */
  static void savePrefsNow(nsITimer *aTimer, void *aClosure);
  void initSavePrefsTimer();
  static nsCOMPtr<nsITimer> sSavePrefsTimer;

  /* Startup crash prevention stuff */
  void startAutoLogin();
  void finishedAutoLogin();

  // Is the current account connecting with autologin
  PRBool mAutoLoginPending;
  // How many accounts are currently connecting with autologin
  static PRUint32 sAutoLoginsPending;
};

#endif /* !PURPLE_ACCOUNT_H_ */
