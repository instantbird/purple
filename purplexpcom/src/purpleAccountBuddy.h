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

#ifndef PURPLE_ACCOUNT_BUDDY_H_
# define PURPLE_ACCOUNT_BUDDY_H_

#include "purpleIAccountBuddy.h"
#include "purpleTag.h"
#include <nsCOMPtr.h>

// af152d5b-fa54-45d4-b6a7-5b4cabef9b7c
#define PURPLE_ACCOUNTBUDDY_CID                                 \
  { 0xaf152d5b, 0xfa54, 0x45d4,                                 \
      { 0xb6, 0xa7, 0x5b, 0x4c, 0xab, 0xef, 0x9b, 0x7c }        \
  }

#define PURPLE_ACCOUNTBUDDY_CONTRACTID          \
  "@instantbird.org/purple/accountbuddy;1"

class purpleAccountBuddy : public purpleIAccountBuddy
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIACCOUNTBUDDY

  purpleAccountBuddy();
  void Init(purpleIAccount *aAccount, PurpleBuddy *aBuddy, purpleTag *aTag);
  void Uninit();
  nsresult Store(PRInt32 aBuddyId);
  nsresult UnStore(PRInt32 aBuddyId);
  PurpleBuddy *GetBuddy() { return mBuddy; }
  void SetBuddyAlias(const nsCString& aBuddyAlias);

private:
  ~purpleAccountBuddy();
  static void CleanUserInfo(void *aData);

protected:
  /* additional members */
  nsCOMPtr<purpleIAccount> mAccount;
  PurpleBuddy *mBuddy;
  nsCOMPtr<purpleTag> mTag;
};

#endif /* !PURPLE_ACCOUNT_BUDDY_H_ */
