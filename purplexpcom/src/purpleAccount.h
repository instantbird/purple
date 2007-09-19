#ifndef PURPLE_ACCOUNT_H_
# define PURPLE_ACCOUNT_H_

#include "purpleIAccount.h"
#include "purpleProtocol.h"
#include "nsCOMPtr.h"
#include <libpurple/account.h>
#include<pref/nsIPrefBranch.h>

// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include<nsStringAPI.h>

#define UI_ID                  "instantbird"

// e450cf22-b98a-4095-b480-e57caf58a2eb
#define PURPLE_ACCOUNT_CID \
{ 0xe450cf22, 0xb98a, 0x4095, \
  { 0xb4, 0x80, 0xe5, 0x7c, 0xaf, 0x58, 0xa2, 0xeb } \
 }

#define PURPLE_ACCOUNT_CONTRACTID \
    "@instantbird.org/purple/account;1"

#define ACCOUNT_KEY "account"

class purpleAccount : public purpleIAccount
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIACCOUNT

  purpleAccount();
  const nsCString& GetKey() { return mKey; }
  PRUint32 GetId() { return mId; }
  PurpleAccount *GetPurpleAccount() { return mAccount; }
  nsresult create(const char *name, purpleProtocol *prpl,
       	          const nsCString& aKey);
  nsresult load(const nsCString& aKey);
  nsresult remove();
  void UnInit();
  PRBool IsPurpleAccount(const PurpleAccount *aAccount) {
    return mAccount == aAccount;
  }

private:
  ~purpleAccount();
  inline nsresult SetId(const nsCString& aId);
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
  nsCOMPtr<nsIPrefBranch> mPrefBranch;

protected:
  /* additional members */
  nsCString mConnectionStateMsg;
};

#endif /* !PURPLE_ACCOUNT_H_ */
