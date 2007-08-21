#include "purpleBuddy.h"
#include"purpleCoreService.h"
#include<nsServiceManagerUtils.h>
#include"purpleUtils.h"

NS_IMPL_ISUPPORTS1(purpleBuddy, purpleIBuddy)

//#define DEBUG_PURPLE_BUDDY

PRUint32 purpleBuddy::lastId = 0;

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

nsresult purpleBuddy::Init(PurpleBuddy *aBuddy)
{
#ifdef DEBUG_PURPLE_BUDDY
  printf("initializing purpleBuddy\n");
#endif
  nsCOMPtr<purpleIAccount> account;
  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  nsresult rv = pcs->GetAccountByPurpleAccount(aBuddy->account,
						getter_AddRefs(account));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_BUDDY
  PurpleBlistNode *node = aBuddy->node.parent;
  PurpleGroup *group = (PurpleGroup *) node->parent;
  printf("Adding %s in group: %s for account %s (%s)\n",
	 aBuddy->name, group->name, aBuddy->account->username, aBuddy->account->protocol_id);
#endif
  purpleAccountBuddy *pab = mBuddies.AppendElement();
  pab->account = account;
  pab->buddy = aBuddy;

  mId = ++lastId;

  return NS_OK;
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

  return returnString(aName, mBuddies[0].buddy->name);
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

  *aResult = mBuddies[aAccountIndex].account;
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
  return pab->account->CreateConversation(pab->buddy->name, aResult);
}
