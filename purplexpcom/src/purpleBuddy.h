#ifndef PURPLE_BUDDY_H_
#define PURPLE_BUDDY_H_

#include "purpleIBuddy.h"
#include "nsCOMPtr.h"
#include "purpleAccount.h"
#include "purpleAccountBuddy.h"
#include "nsTArray.h"

// 9090b5e6-51fb-4495-92d1-a0f713f4f7f9
#define PURPLE_BUDDY_CID					\
  { 0x9090b5e6, 0x51fb, 0x4495,					\
      { 0x92, 0xd1, 0xa0, 0xf7, 0x13, 0xf4, 0xf7, 0xf9 }	\
  }

#define PURPLE_BUDDY_CONTRACTID			\
  "@instantbird.org/purple/buddy;1"

class purpleBuddy : public purpleIBuddy
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIBUDDY

  purpleBuddy();
  nsresult Init(PurpleBuddy *aBuddy);
  nsresult load(PurpleBuddy *aBuddy, purpleIAccount *aAccount,
                PRUint32 aId, purpleTag *aTag);
  nsresult AddAccount(PurpleBuddy *aBuddy);
  purpleAccountBuddy *AddAccount(purpleIAccount *aAccount,
                                 PurpleBuddy *aBuddy,
                                 purpleTag *aTag);
  PRUint32 getId() { return mId; }

private:
  ~purpleBuddy();

protected:
  /* additional members */
  PRUint32 mId;
  nsCString mAlias;
  nsCOMArray<purpleAccountBuddy> mBuddies;
};

#endif /* !PURPLE_BUDDY_H_ */
