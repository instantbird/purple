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

#pragma GCC visibility push(default)
#include <libpurple/account.h>
#pragma GCC visibility pop

#include <imIAccount.h>
#include "purpleAccountScoper.h"
#include "nsCOMPtr.h"
#include <nsIPrefBranch.h>
#include <nsStringAPI.h>

#define UI_ID                  "instantbird"

class purpleAccount : public prplIAccount
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
  nsresult SetBoolPref(const char *aName, PRBool aValue);
  nsresult SetIntPref(const char *aName, PRInt32 aValue);
  nsresult SetStringPref(const char *aName, const char *aValue);

  PurpleAccount *GetPurpleAccount() { return mAccount; }
  static inline purpleAccount *fromPurpleAccount(PurpleAccount *aAccount) {
    return static_cast<purpleAccount *>(aAccount->ui_data);
  }
  PurplePluginProtocolInfo *GetPrplInfo();
  inline PRUint32 ScopeId() { return mId; }
  inline void EnterScope() { mScoper = new purpleAccountScoper(mId); }
  inline void ExitScope() { delete mScoper; mScoper = nsnull; }

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
