#include "purpleBuddy.h"
#include"purpleCoreService.h"
#include<nsServiceManagerUtils.h>
#include"purpleUtils.h"
#include "purpleStorage.h"
#include <prprf.h>

NS_IMPL_ISUPPORTS1(purpleBuddy, purpleIBuddy)

//#define DEBUG_PURPLE_BUDDY

void purpleAccountBuddy::Init(purpleIAccount *aAccount,
                              PurpleBuddy *aBuddy)
{
  mAccount = aAccount;
  mBuddy = aBuddy;
}

nsresult purpleAccountBuddy::Store(PRInt32 aBuddyId, PRInt32 aGroupId)
{
  char *key;
  nsresult rv = mAccount->GetId(&key);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 accountId;
  if (PR_sscanf(key, ACCOUNT_KEY "%u", &accountId) != 1) {
    return NS_ERROR_UNEXPECTED;
  }

  purpleStorage *storageInstance = purpleStorage::GetInstance();
  mozIStorageStatement *statement = storageInstance->mInsertAccountBuddy;
  mozStorageStatementScoper scoper(statement);
  rv = statement->BindInt32Parameter(0, accountId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, aBuddyId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(2, aGroupId);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_BUDDY
  printf("purpleAccountBuddy::Store(buddyId = %i, groupId = %i)\n",
         aBuddyId, aGroupId);
#endif
  return statement->Execute();
}

purpleBuddy::purpleBuddy()
{
  /* member initializers and constructor code */
  mId = 0;
#ifdef DEBUG_PURPLE_BUDDY
  printf("constructing purpleBuddy\n");
#endif
}

purpleBuddy::~purpleBuddy()
{
  /* destructor code */
#ifdef DEBUG_PURPLE_BUDDY
  printf("destructing purpleBuddy\n");
#endif
}

nsresult purpleBuddy::load(PurpleBuddy *aBuddy, purpleIAccount *aAccount,
                           PRUint32 aId)
{
  mId = aId;
  AddAccount(aAccount, aBuddy);
  return NS_OK;
}

nsresult purpleBuddy::Init(PurpleBuddy *aBuddy)
{
#ifdef DEBUG_PURPLE_BUDDY
  PurpleBlistNode *node = aBuddy->node.parent;
  PurpleGroup *group = (PurpleGroup *) node->parent;
  printf("purpleBuddy::Init Trying to add %s in group: %s for account %s (%s)\n",
	 purple_normalize(aBuddy->account, aBuddy->name), group->name, aBuddy->account->username, aBuddy->account->protocol_id);
#endif

  purpleStorage *storageInstance = purpleStorage::GetInstance();
  {
    mozIStorageStatement *statement = storageInstance->mInsertBuddy;
    mozStorageStatementScoper scoper(statement);
    nsCString name(purple_normalize(aBuddy->account, aBuddy->name));
    nsresult rv = statement->BindUTF8StringParameter(0, name);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString prettyName(aBuddy->name);
    rv = statement->BindUTF8StringParameter(1, prettyName);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt32Parameter(2, 0); // FIXME (0 = not visible);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mozIStorageStatement *statement = storageInstance->mLastId;
  mozStorageStatementScoper scoper(statement);
  PRBool ok;
  nsresult rv = statement->ExecuteStep(&ok);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!ok) {
#ifdef DEBUG_PURPLE_BUDDY
    printf("purpleBuddy::Init: couldn't get the id of the stored buddy\n");
#endif
    return NS_ERROR_FAILURE;
  }
  mId = statement->AsInt32(0);
#ifdef DEBUG_PURPLE_BUDDY
  printf("purpleBuddy::Init: stored buddy with id %i\n", mId);
#endif

  return AddAccount(aBuddy);
}

purpleAccountBuddy *purpleBuddy::AddAccount(purpleIAccount *aAccount,
                                            PurpleBuddy *aBuddy)
{
  purpleAccountBuddy *pab = mBuddies.AppendElement();
  pab->Init(aAccount, aBuddy);
  return pab;
}

nsresult purpleBuddy::AddAccount(PurpleBuddy *aBuddy)
{
  nsCOMPtr<purpleIAccount> account;
  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  nsresult rv = pcs->GetAccountByPurpleAccount(aBuddy->account,
                                               getter_AddRefs(account));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 groupId;
  rv = pcs->GetGroupIdFromPurpleBuddy(aBuddy, &groupId);
  NS_ENSURE_SUCCESS(rv, rv);

  purpleAccountBuddy *pab = AddAccount(account, aBuddy);
  return pab->Store(mId, groupId);
}

/* readonly attribute long id; */
NS_IMETHODIMP purpleBuddy::GetId(PRInt32 *aId)
{
  PURPLE_ENSURE_INIT(mId);

  *aId = mId;
  return NS_OK;
}

/* readonly attribute string name; */
NS_IMETHODIMP purpleBuddy::GetName(char * *aName)
{
  PURPLE_ENSURE_INIT(mId);

  //FIXME mBuddies[0].GetName(aName);
  return returnString(aName, mBuddies[0].mBuddy->name);
}

/* attribute string alias; */
NS_IMETHODIMP purpleBuddy::GetAlias(char * *aAlias)
{
  PURPLE_ENSURE_INIT(mId);

  return returnString(aAlias, mAlias.get());
}
NS_IMETHODIMP purpleBuddy::SetAlias(const char * aAlias)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute short nbOnline; */
NS_IMETHODIMP purpleBuddy::GetNbOnline(PRInt16 *aNbOnline)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute short accountCount; */
NS_IMETHODIMP purpleBuddy::GetAccountCount(PRInt16 *aAccountCount)
{
  PURPLE_ENSURE_INIT(mId);

  *aAccountCount = mBuddies.Length();
  return NS_OK;
}

/* purpleIAccount getAccount (in short aAccountIndex); */
NS_IMETHODIMP purpleBuddy::GetAccount(PRUint16 aAccountIndex,
				      purpleIAccount **aResult)
{
  PURPLE_ENSURE_INIT(mId);

  if (aAccountIndex > mBuddies.Length()) {
    return NS_ERROR_FAILURE;
  }

  //FIXME return mBuddies[aAccountIndex].GetAccount(aResult);
  *aResult = mBuddies[aAccountIndex].mAccount;
  NS_ADDREF(*aResult);
  return NS_OK;
}

/* nsISimpleEnumerator getTags (); */
NS_IMETHODIMP purpleBuddy::GetTags(nsISimpleEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator getAccounts (); */
NS_IMETHODIMP purpleBuddy::GetAccounts(nsISimpleEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* purpleIConversation createConversation (in unsigned short aAccountIndex); */
NS_IMETHODIMP purpleBuddy::CreateConversation(PRUint16 aAccountIndex,
					      purpleIConversation **aResult)
{
  PURPLE_ENSURE_INIT(mId);

  if (aAccountIndex > mBuddies.Length()) {
    return NS_ERROR_FAILURE;
  }

  purpleAccountBuddy *pab = &mBuddies[aAccountIndex];
  //FIXME return pab->CreateConversation(aResult);
  return pab->mAccount->CreateConversation(pab->mBuddy->name, aResult);
}
