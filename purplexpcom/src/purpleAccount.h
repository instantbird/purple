/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_ACCOUNT_H_
# define PURPLE_ACCOUNT_H_

#pragma GCC visibility push(default)
#include <libpurple/account.h>
#pragma GCC visibility pop

#include <imIAccount.h>
#include "purpleAccountScoper.h"
#include "nsCOMPtr.h"
#include <nsIPrefBranch.h>
#include <nsStringAPI.h>

#define UI_ID                  "instantbird"

class purpleAccount MOZ_FINAL : public prplIAccount
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PRPLIACCOUNT

  // Called by purpleProtocol:
  purpleAccount();
  nsresult Init(imIAccount *aImAccount, prplIProtocol *aPrpl);

  // Called from purpleInitAccounts:
  nsresult Connecting(const nsACString & aConnectionStateMessage);
  nsresult Connected();
  nsresult Disconnecting(PRInt16 aConnectionErrorReason,
                         const nsACString & aConnectionErrorMessage);
  nsresult Disconnected();

  // Called from purpleInitAccounts, to save a pref libpurple has changed.
  nsresult SetBoolPref(const char *aName, bool aValue);
  nsresult SetIntPref(const char *aName, PRInt32 aValue);
  nsresult SetStringPref(const char *aName, const char *aValue);

  PurpleAccount *GetPurpleAccount() { return mAccount; }
  static inline purpleAccount *fromPurpleAccount(PurpleAccount *aAccount) {
    return static_cast<purpleAccount *>(aAccount->ui_data);
  }
  PurplePluginProtocolInfo *GetPrplInfo();
  inline PRUint32 ScopeId() { return mId; }
  inline void EnterScope() { mScoper = new purpleAccountScoper(mId); }
  inline void ExitScope() { delete mScoper; mScoper = nullptr; }

private:
  ~purpleAccount();
  nsresult ApplyCurrentUserIcon();
  inline void SetAccountIcon(const nsCString& aBuffer);
  purpleAccountScoper *mScoper;

protected:
  /* additional members */
  PRUint32 mId;
  nsCOMPtr<imIAccount> mImAccount;
  PurpleAccount *mAccount;
  nsCOMPtr<purpleIProxyInfo> mProxy;
  PRInt16 mConnectionErrorReason;
  PurpleStatusPrimitive mStatus;
  nsCOMPtr<nsIPrefBranch> mPrefBranch;
  nsCOMPtr<nsIPrefBranch> mPrefOptBranch;
};

#endif /* !PURPLE_ACCOUNT_H_ */
