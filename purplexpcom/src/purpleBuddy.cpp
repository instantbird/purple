#include "purpleBuddy.h"
#include "purpleCoreService.h"
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include "purpleUtils.h"
#include "purpleStorage.h"
#include <prprf.h>

NS_IMPL_ISUPPORTS1(purpleBuddy, purpleIBuddy)

//#define DEBUG_PURPLE_BUDDY

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
                           PRUint32 aId, purpleTag *aTag)
{
  mId = aId;
  AddAccount(aAccount, aBuddy, aTag);
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
    nsCString name(purple_normalize(aBuddy->account,
                                    purple_buddy_get_name(aBuddy)));
    nsresult rv = statement->BindUTF8StringParameter(0, name);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString prettyName(purple_buddy_get_name(aBuddy));
    rv = statement->BindUTF8StringParameter(1, prettyName);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString alias(purple_buddy_get_alias(aBuddy));
    rv = statement->BindUTF8StringParameter(2, alias);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt32Parameter(3, 0); // FIXME (0 = not visible);
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
                                            PurpleBuddy *aBuddy,
                                            purpleTag *aTag)
{
  nsCOMPtr<purpleAccountBuddy> pab = do_CreateInstance(PURPLE_ACCOUNTBUDDY_CONTRACTID);
  pab->Init(aAccount, aBuddy, aTag);
  mBuddies.AppendObject(pab);
  return pab;
}

nsresult purpleBuddy::AddAccount(PurpleBuddy *aBuddy)
{
  nsCOMPtr<purpleIAccount> account;
  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  nsresult rv = pcs->GetAccountByPurpleAccount(aBuddy->account,
                                               getter_AddRefs(account));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<purpleTag> tag;
  rv = pcs->GetTagFromPurpleBuddy(aBuddy, getter_AddRefs(tag));
  NS_ENSURE_SUCCESS(rv, rv);

  purpleAccountBuddy *pab = AddAccount(account, aBuddy, tag);
  return pab->Store(mId);
}

nsresult purpleBuddy::RemoveAccount(PurpleBuddy *aBuddy)
{
  for (PRInt32 i = 0; i < mBuddies.Count(); ++i) {
    if (mBuddies[i]->GetBuddy() == aBuddy) {
      mBuddies[i]->Uninit();
      mBuddies.RemoveObjectAt(i);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
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

  return mBuddies[0]->GetBuddyName(aName);
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

  *aAccountCount = mBuddies.Count();
  return NS_OK;
}

/* purpleIAccount getAccount (in short aAccountIndex); */
NS_IMETHODIMP purpleBuddy::GetAccount(PRUint16 aAccountIndex,
				      purpleIAccount **aResult)
{
  PURPLE_ENSURE_INIT(mId);

  if (aAccountIndex > mBuddies.Count()) {
    return NS_ERROR_FAILURE;
  }

  return mBuddies[aAccountIndex]->GetAccount(aResult);
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

  if (aAccountIndex > mBuddies.Count()) {
    return NS_ERROR_FAILURE;
  }

  purpleAccountBuddy *pab = mBuddies[aAccountIndex];
  return pab->CreateConversation(aResult);
}

/* [noscript] purpleIAccountBuddy getAccountBuddyForPurpleBuddy (in PurpleNativeBuddy aBuddy); */
NS_IMETHODIMP purpleBuddy::GetAccountBuddyForPurpleBuddy(PurpleBuddy * aBuddy,
                                                         purpleIAccountBuddy **aResult)
{
  for (PRInt32 i = 0; i < mBuddies.Count(); ++i) {
    if (mBuddies[i]->GetBuddy() == aBuddy) {
      *aResult = mBuddies[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}
