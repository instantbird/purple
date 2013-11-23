/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_ACCOUNT_BUDDY_H_
# define PURPLE_ACCOUNT_BUDDY_H_

#include "purpleAccount.h"
#include "imIContactsService.h"
#include <nsCOMPtr.h>
#include <nsStringAPI.h>

#pragma GCC visibility push(default)
#include <libpurple/blist.h>
#pragma GCC visibility pop

#define PURPLE_ACCOUNTBUDDY_CID                                 \
  { 0xaf152d5b, 0xfa54, 0x45d4,                                 \
      { 0xb6, 0xa7, 0x5b, 0x4c, 0xab, 0xef, 0x9b, 0x7c }        \
  }

#define GET_NODE_UI_DATA(aNode)                                 \
  purple_blist_node_get_ui_data(PURPLE_BLIST_NODE(aNode))

class purpleAccountBuddy MOZ_FINAL : public imIAccountBuddy
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMIACCOUNTBUDDY
  NS_DECL_IMISTATUSINFO

  purpleAccountBuddy(PurpleBuddy *aPurpleBuddy, imIBuddy *aBuddy = nullptr);
  static inline purpleAccountBuddy *fromPurpleBuddy(PurpleBuddy *aBuddy) {
    return static_cast<purpleAccountBuddy *>(GET_NODE_UI_DATA(aBuddy));
  }
  static inline imITag *GetTagFromPurpleGroup(PurpleGroup *aGroup)
  {
    return static_cast<imITag *>(GET_NODE_UI_DATA(aGroup));
  }
  static PurpleGroup *GetPurpleGroupForTag(imITag *aTag);
  nsresult NotifyObservers(const char *aSignal,
                           const PRUnichar *aData = nullptr);

private:
  ~purpleAccountBuddy();
  static void CleanUserInfo(void *aData);

protected:
  /* additional members */
  nsCOMPtr<imIAccount> mAccount;
  PurpleBuddy *mPurpleBuddy;
  nsCOMPtr<imIBuddy> mBuddy;
};

#endif /* !PURPLE_ACCOUNT_BUDDY_H_ */
