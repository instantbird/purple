#include "purpleProtocol.h"
#include "purpleAccount.h"
#include "purpleICoreService.h"
#include "purpleConversation.h"
#include<nsIMutableArray.h>
#include<nsIPrefBranch.h>
#include<nsCOMPtr.h>
#include"nsInterfaceHashtable.h"
#include"purpleBuddy.h"

// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include<nsCOMArray.h>

class purpleCoreService : public purpleICoreService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEICORESERVICE

  purpleCoreService();

  void AddConversation(purpleConversation* aConv);

private:
  ~purpleCoreService();
  void InitProtocols();
  nsresult InitAccounts();
  nsCOMPtr<nsIPrefBranch> mPrefService;
  inline void UnInitAccounts();
  inline void UnInitProtocols();

protected:
  PRBool mInitialized;
  // array of purpleIConversation
  nsCOMArray<purpleConversation> mConversations;
  nsCOMArray<purpleProtocol> mProtocols;
  nsCOMArray<purpleAccount> mAccounts;
  nsInterfaceHashtable<nsUint32HashKey, purpleBuddy> mBuddiesByPurpleBuddy;
  nsInterfaceHashtable<nsUint32HashKey, purpleBuddy> mBuddiesById;
};

#define PURPLE_CORE_SERVICE_CID \
    /* {3E8A9D5E-4774-465a-9733-EA81FD67F161} */ \
    { 0x3e8a9d5e, 0x4774, 0x465a, \
        { 0x97, 0x33, 0xea, 0x81, 0xfd, 0x67, 0xf1, 0x61 } \
    }

#define PURPLE_CORE_SERVICE_CONTRACTID \
    "@instantbird.org/purple/core;1"
