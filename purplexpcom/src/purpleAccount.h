#ifndef PURPLE_ACCOUNT_H_
# define PURPLE_ACCOUNT_H_

#include "purpleIAccount.h"
#include "purpleProtocol.h"
#include "nsCOMPtr.h"
#include <libpurple/account.h>
#include<nsIPrefBranch.h>

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

class purpleAccount : public purpleIAccount
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIACCOUNT

  purpleAccount();
  const nsCString& GetKey() { return mKey; }
  nsresult create(const char *name, purpleProtocol *prpl,
       	          const nsCString& aKey);
  nsresult load(const nsCString& aKey);
  nsresult remove();
  void UnInit();

private:
  ~purpleAccount();
  inline void SetId(const nsCString& aId);
  inline nsresult GetPrefBranch();
  nsresult SetBoolPref(const char *aName, PRBool aValue);
  nsresult SetIntPref(const char *aName, PRInt32 aValue);
  nsresult SetStringPref(const char *aName, const char *aValue);
  inline void GetBoolPref(const char *aName);
  inline void GetIntPref(const char *aName);
  inline void GetCharPref(const char *aName);

  PurpleAccount *mAccount;
  nsCString mKey;
  nsCOMPtr<purpleProtocol> mProtocol;
  nsCOMPtr<nsIPrefBranch> mPrefBranch;

protected:
  /* additional members */
};

#endif /* !PURPLE_ACCOUNT_H_ */
