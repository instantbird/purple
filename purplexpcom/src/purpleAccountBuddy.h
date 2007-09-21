#ifndef PURPLE_ACCOUNT_BUDDY_H_
# define PURPLE_ACCOUNT_BUDDY_H_

#include "purpleIAccountBuddy.h"
#include "purpleTag.h"
#include <nsCOMPtr.h>
#include <libpurple/account.h>

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
  nsresult Store(PRInt32 aBuddyId);
  PurpleBuddy *GetBuddy() { return mBuddy; }

private:
  ~purpleAccountBuddy();

protected:
  /* additional members */
  nsCOMPtr<purpleIAccount> mAccount;
  PurpleBuddy *mBuddy;
  nsCOMPtr<purpleTag> mTag;
};

#endif /* !PURPLE_ACCOUNT_BUDDY_H_ */
