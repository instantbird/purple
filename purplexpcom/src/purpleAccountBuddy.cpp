#include "purpleAccountBuddy.h"
#include "purpleCoreService.h"
#include "purpleAccount.h"
#include "purpleStorage.h"
#include "purpleUtils.h"
#include <nsServiceManagerUtils.h>
#include <prprf.h>

NS_IMPL_ISUPPORTS1(purpleAccountBuddy, purpleIAccountBuddy)

purpleAccountBuddy::purpleAccountBuddy()
{
  /* member initializers and constructor code */
}

purpleAccountBuddy::~purpleAccountBuddy()
{
  /* destructor code */
}

void purpleAccountBuddy::Init(purpleIAccount *aAccount,
                              PurpleBuddy *aBuddy,
                              purpleTag *aTag)
{
  mAccount = aAccount;
  mBuddy = aBuddy;
  mTag = aTag;
}

void purpleAccountBuddy::Uninit()
{
  mBuddy = NULL;
}

nsresult purpleAccountBuddy::Store(PRInt32 aBuddyId)
{
  char *key;
  nsresult rv = mAccount->GetId(&key);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 accountId;
  if (PR_sscanf(key, ACCOUNT_KEY "%u", &accountId) != 1) {
    return NS_ERROR_UNEXPECTED;
  }

  PRInt32 groupId;
  rv = mTag->GetId(&groupId);
  NS_ENSURE_SUCCESS(rv, rv);

  purpleStorage *storageInstance = purpleStorage::GetInstance();
  mozIStorageStatement *statement = storageInstance->mInsertAccountBuddy;
  mozStorageStatementScoper scoper(statement);
  rv = statement->BindInt32Parameter(0, accountId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, aBuddyId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(2, groupId);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_BUDDY
  printf("purpleAccountBuddy::Store(buddyId = %i, groupId = %i)\n",
         aBuddyId, groupId);
#endif
  return statement->Execute();
}

/* readonly attribute purpleIBuddy buddy; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddy(purpleIBuddy * *aBuddy)
{
  if (!mBuddy) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  return pcs->GetBuddyByPurpleBuddy(mBuddy, aBuddy);
}

/* readonly attribute purpleIAccount account; */
NS_IMETHODIMP purpleAccountBuddy::GetAccount(purpleIAccount * *aAccount)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aAccount = mAccount;
  NS_ADDREF(*aAccount);
  return NS_OK;
}

/* readonly attribute purpleITag tag; */
NS_IMETHODIMP purpleAccountBuddy::GetTag(purpleITag * *aTag)
{
  if (!mTag) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aTag = mTag;
  NS_ADDREF(*aTag);
  return NS_OK;
}

/* readonly attribute string buddyName; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddyName(char * *aBuddyName)
{
  if (!mBuddy) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return returnString(aBuddyName, mBuddy->name);
}

/* purpleIConversation createConversation (); */
NS_IMETHODIMP purpleAccountBuddy::CreateConversation(purpleIConversation **aResult)
{
  if (!mAccount || !mBuddy) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mAccount->CreateConversation(purple_buddy_get_name(mBuddy), aResult);
}

#define PURPLE_PRESENCE_GET_BOOL_IMPL(aName, aFctName)                  \
  NS_IMETHODIMP purpleAccountBuddy::Get##aName(PRBool *a##aName)        \
  {                                                                     \
    if (!mBuddy) {                                                      \
      return NS_ERROR_NOT_INITIALIZED;                                  \
    }                                                                   \
                                                                        \
    *a##aName = purple_presence_is_##aFctName(purple_buddy_get_presence(mBuddy)); \
    return NS_OK;                                                       \
  }

/* readonly attribute boolean online; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Online, online)

/* readonly attribute boolean available; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Available, available)

/* readonly attribute boolean idle; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Idle, idle)

/* readonly attribute string status; */
NS_IMETHODIMP purpleAccountBuddy::GetStatus(nsACString &aStatus)
{
  if (!mBuddy) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PurplePlugin *prpl = purple_find_prpl(purple_account_get_protocol_id(mBuddy->account));
  NS_ENSURE_ARG_POINTER(prpl);

  PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
  NS_ENSURE_ARG_POINTER(prpl_info);

  if (prpl_info->status_text && mBuddy->account->gc) {
    char *tmp1 = prpl_info->status_text(mBuddy);
    char *tmp2 = purple_unescape_html(tmp1);
    aStatus = tmp2;
    free(tmp1);
    free(tmp2);
  }
  else
    aStatus = "";

  return NS_OK;
}
