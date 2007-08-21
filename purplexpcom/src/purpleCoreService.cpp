#include "purpleCoreService.h"
#include "purpleProtocol.h"
#include "purpleBuddy.h"
#include "purpleUtils.h"
#include "nsXPCOM.h"
#include <glib.h>
#include <libpurple/core.h>
#include <libpurple/account.h>

#include<nsServiceManagerUtils.h>
#include<nsComponentManagerUtils.h>
#include<nsIObserverService.h>
#include<nsIPrefService.h>
#include<nsCOMPtr.h>
#include<nsCRTGlue.h>
#include<stdio.h>

#include<nsArrayEnumerator.h>

//#define DEBUG_PURPLE_CORE_SERVICE

#define PREF_MESSENGER_ACCOUNTS "messenger.accounts"

NS_IMPL_ISUPPORTS1(purpleCoreService, purpleICoreService)

extern nsresult init_libpurple();

purpleCoreService::purpleCoreService()
  :mInitialized(PR_FALSE),
   mConversations(nsnull),
   mProtocols(nsnull)
{
  mPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  mBuddiesByPurpleBuddy.Init();
  mBuddiesById.Init();
}

purpleCoreService::~purpleCoreService()
{
  this->Quit();
}

/* boolean Init (); */
NS_IMETHODIMP purpleCoreService::Init()
{
  /* We shouldn't init the core twice. If the core service is already
     initialized, return with an error */
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  /* Init the core of libpurple */
  nsresult rv = init_libpurple();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Load the list of protocol plugins and their specific options */
  InitProtocols();

  /* Set to true before the end of the initialization because
     InitAccounts calls GetProtoById that ensures the core is
     initialized */
  mInitialized = PR_TRUE;

  /* Load saved accounts */
  InitAccounts();

#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("libpurple initialized\n");
#endif

  return NS_OK;
}

void purpleCoreService::InitProtocols()
{
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("start init protocols\n");

#if 1
  //this code demonstrate how to enumerate protocols from libpurple
  GList *iter = purple_plugins_get_protocols();
  for (int i = 0; iter; iter = iter->next) {
    PurplePlugin *plugin = (PurplePlugin *) iter->data;
    PurplePluginInfo *info = plugin->info;
    if (info && info->name) {
      printf("\t%d: %s (%s)\n", i++, info->name, info->id);
    }
  }
#endif

#endif
  for (GList *iter = purple_plugins_get_protocols();
       iter; iter = iter->next) {
    PurplePlugin *plugin = (PurplePlugin *) iter->data;
    PurplePluginInfo *info = plugin->info;
    if (info && info->name) {
#ifdef DEBUG_PURPLE_CORE_SERVICE
      printf("trying to create %s :\t (%s)\n", info->name, info->id);
#endif
      nsCOMPtr<purpleProtocol> proto = do_CreateInstance(PURPLE_PROTOCOL_CONTRACTID);
      proto->Init(info);
      mProtocols.AppendObject(proto);
    }
  }
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("end init protocols\n");
#endif
}

inline void purpleCoreService::UnInitProtocols()
{
  for (PRInt32 i = mProtocols.Count() - 1; i >= 0; --i) {
    mProtocols[i]->UnInit();
  }
  mProtocols.Clear();
}

/* void Quit (); */
NS_IMETHODIMP purpleCoreService::Quit()
{
  PURPLE_ENSURE_INIT(mInitialized);

  mConversations.Clear(); // don't know yet if there's a need for uninit
  mBuddiesByPurpleBuddy.Clear();
  mBuddiesById.Clear();
  UnInitAccounts();
  UnInitProtocols();
  mInitialized = PR_FALSE;
  notifyObservers(this, "purple-quit");
  purple_core_quit();
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("purple_core_quit\n");
#endif

  return NS_OK;
}

/* readonly attribute string version; */
NS_IMETHODIMP purpleCoreService::GetVersion(char * *aVersion)
{
  return returnString(aVersion, purple_core_get_version());
}

/* purpleIBuddy getBuddyById (in long aId); */
NS_IMETHODIMP purpleCoreService::GetBuddyById(PRInt32 aId,
					      purpleIBuddy **aResult)
{
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("looking up buddy with id %i\n", aId);
#endif

  purpleBuddy *buddyPtr = NULL;
  if (mBuddiesById.Get(aId, &buddyPtr)) {
    *aResult = buddyPtr;
#ifdef DEBUG_PURPLE_CORE_SERVICE
    printf("purpleBuddy found in HashTable (by Id)\n");
#endif
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

/* [noscript] purpleIBuddy getBuddyByPurpleBuddy (in PurpleNativeBuddy aBuddy); */
NS_IMETHODIMP purpleCoreService::GetBuddyByPurpleBuddy(PurpleBuddy * aBuddy,
						       purpleIBuddy **aResult)
{
  /* First, lookup the PurpleBuddy pointer in our PurpleBuddy pointers
   * hash. If we find it, just return the associated purpleBuddy.
   */
  purpleBuddy *buddyPtr = NULL;
  if (mBuddiesByPurpleBuddy.Get((PRUint32) aBuddy, &buddyPtr)) {
    *aResult = buddyPtr;
#ifdef DEBUG_PURPLE_CORE_SERVICE
    printf("purpleBuddy found in HashTable\n");
#endif
    return NS_OK;
  }

  /* If we can't find it in the pointers hash, try to get the Id from
   * mozStorage. If we find it, append the new PurpleBuddy to it, and
   * add the new PurpleBuddy pointer to the hash.
   */
  //FIXME!!

  /* At this point, if we still haven't found it, this is a NEW
   * buddy. Create a new purpleBuddy instance, initialize it, store it
   * in mozStorage and add it in the two hashes.
   */
  nsresult rv;
  nsCOMPtr<purpleBuddy> buddy = do_CreateInstance(PURPLE_BUDDY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  buddy->Init(aBuddy);
  mBuddiesByPurpleBuddy.Put((PRUint32) aBuddy, buddy);
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("purpleBuddy added in HashTable with Id %i\n", buddy->getId());
#endif
  mBuddiesById.Put(buddy->getId(), buddy);

  *aResult = buddy;
  NS_ADDREF(*aResult);
  return NS_OK;
}

/* nsISimpleEnumerator getConversations (); */
NS_IMETHODIMP purpleCoreService::GetConversations(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  return NS_NewArrayEnumerator(aResult, mConversations);
}


/* purpleIProtocol getConversationById (in string aConversationId); */
NS_IMETHODIMP purpleCoreService::GetConversationById(PRInt32 aId,
						     purpleIConversation **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("GetConversationById(%i)\n", aId);
#endif

  for (PRInt32 i = mConversations.Count() - 1; i >= 0; --i) {
    if (mConversations[i]->GetId() == aId) {
      *aResult = mConversations[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

void purpleCoreService::AddConversation(purpleConversation* aConv)
{
  mConversations.AppendObject(aConv);
  notifyObservers(aConv, "new-conversation");
}

/* nsISimpleEnumerator getProtocols (); */
NS_IMETHODIMP purpleCoreService::GetProtocols(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  return NS_NewArrayEnumerator(aResult, mProtocols);
}

/* purpleIProtocol getProtocolById (in string aProtocolId); */
NS_IMETHODIMP purpleCoreService::GetProtocolById(const char *aProtocolId, purpleIProtocol **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  nsCString id(aProtocolId);
  for (PRInt32 i = mProtocols.Count() - 1; i >= 0; --i) {
    if (id.Equals(mProtocols[i]->GetId())) {
      *aResult = mProtocols[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

/* [noscript] purpleNativeProtocol getProtoById (in string aProtocolId); */
NS_IMETHODIMP purpleCoreService::GetProtoById(const char *aProtocolId, purpleProtocol * *aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  nsCString id(aProtocolId);
  for (PRInt32 i = mProtocols.Count() - 1; i >= 0; --i) {
    if (id.Equals(mProtocols[i]->GetId())) {
      *aResult = mProtocols[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}


/* purpleIAccount getAccountById (in string accountId); */
NS_IMETHODIMP purpleCoreService::GetAccountById(const char *aAccountId, purpleIAccount **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    if (mAccounts[i]->GetKey().Equals(aAccountId)) {
      *aResult = mAccounts[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

/* nsISimpleEnumerator getAccounts (); */
NS_IMETHODIMP purpleCoreService::GetAccounts(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  return NS_NewArrayEnumerator(aResult, mAccounts);
}

nsresult purpleCoreService::InitAccounts()
{
  /* Get the list of accounts */
  nsCString accountList;
  nsresult rv = mPrefService->GetCharPref(PREF_MESSENGER_ACCOUNTS,
					  getter_Copies(accountList));

#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("InitAccounts list = %s\n", accountList.get());
#endif

  nsCOMPtr<purpleAccount> account;
  char *newStr = accountList.BeginWriting();
  nsCAutoString key;
  for (char *token = NS_strtok(",", &newStr);
       token;
       token = NS_strtok(",", &newStr)) {
    key = token;
    key.StripWhitespace();
    
    if (key.IsEmpty()) {
      continue;
    }

    /* create the purpleAccount object */
    account = do_CreateInstance(PURPLE_ACCOUNT_CONTRACTID);
    rv = account->load(key);
    if (NS_FAILED(rv)) {
      continue;
    }

    /* Keep it the local account list */
    mAccounts.AppendObject(account);
  }

  return NS_OK;
}

inline void purpleCoreService::UnInitAccounts()
{
  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    mAccounts[i]->UnInit();
  }
  mAccounts.Clear();
}

/* purpleIAccount createAccount (in string name, in purpleIProtocol prpl); */
NS_IMETHODIMP purpleCoreService::CreateAccount(const char *name,
					       const char *prpl,
					       purpleIAccount **aResult)
{
  /* Get the list of accounts */
  nsCString accountList;
  nsresult rv = mPrefService->GetCharPref(PREF_MESSENGER_ACCOUNTS,
					  getter_Copies(accountList));
  NS_ENSURE_SUCCESS(rv, rv);

  /* First get a new unique account key */
  nsCString key;
  PRInt32 i = 1;
  PRBool found = PR_FALSE;
  do {
    key = "account";
    key.AppendInt(i++);
    for (PRInt32 j = mAccounts.Count() - 1; j >= 0; --j) {
      if ((found = mAccounts[j]->GetKey().Equals(key))) {
	break;
      }
    }
  } while (found);

  /* Get the prpl */
  nsCOMPtr<purpleProtocol> proto;
  rv = GetProtoById(prpl, getter_AddRefs(proto));
  NS_ENSURE_SUCCESS(rv, rv);

  /* Actually create the new account */
  nsCOMPtr<purpleAccount> account = do_CreateInstance(PURPLE_ACCOUNT_CONTRACTID);
  rv = account->create(name, proto, key);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Save the account list pref */
  if (accountList.IsEmpty())
    accountList = key;
  else {
    accountList.Append(',');
    accountList.Append(key);
  }
  mPrefService->SetCharPref(PREF_MESSENGER_ACCOUNTS, accountList.get());

  /* Keep it in the local account list */
  mAccounts.AppendObject(account);

  /* Notify observers */
  notifyObservers(account, "account-added");

  /* And return the new account as result */
  *aResult = account;
  NS_ADDREF(*aResult);
  return NS_OK;
}

/* void deleteAccount (in string accountId); */
NS_IMETHODIMP purpleCoreService::DeleteAccount(const char *aAccountId)
{
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("attempting to delete %s\n", aAccountId);
#endif
  /* delete the account */
  PRInt32 i;
  for (i = mAccounts.Count() - 1; i >= 0; --i) {
    if (mAccounts[i]->GetKey().Equals(aAccountId)) {
      /* Notify observers */
      notifyObservers(mAccounts[i], "account-removed");
      /* Remove the pref of the account, and remove it from libpurple */
      mAccounts[i]->remove();
      /* Remove it from our local array */
      mAccounts.RemoveObjectAt(i);
      break;
    }
  }
  if (i < 0) {
    return NS_ERROR_FAILURE;
  }

  /*Update the list of accounts in the prefs */

  /* Get the list of accounts */
  nsCString accountList;
  nsresult rv = mPrefService->GetCharPref(PREF_MESSENGER_ACCOUNTS,
					  getter_Copies(accountList));
  NS_ENSURE_SUCCESS(rv, rv);
 
  // reconstruct the new account list, re-adding all accounts except
  // the one with 'key'
  nsCString newAccountList;
  char *newStr = accountList.BeginWriting();
  
  for (char *token = NS_strtok(",", &newStr);
       token;
       token = NS_strtok(",", &newStr)) {
    nsCString testKey(token);
    testKey.StripWhitespace();

    // re-add the candidate key only if it's not the key we're looking for
    if (!testKey.IsEmpty() && !testKey.Equals(aAccountId)) {
      if (!newAccountList.IsEmpty())
	newAccountList.Append(',');
      newAccountList += testKey;
    }
  }

  // now write the new account list back to the prefs
  rv = mPrefService->SetCharPref(PREF_MESSENGER_ACCOUNTS,
				newAccountList.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure the change is written to the disk now
  nsCOMPtr<nsIPrefService> prefs = do_QueryInterface(mPrefService);
  return prefs->SavePrefFile(nsnull);
}

nsresult purpleCoreService::notifyObservers(nsISupports *aObj,
					    const char *aEvent)
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("notifying observers of %s\n", aEvent);
#endif
  return os->NotifyObservers(aObj, aEvent, nsnull);
}

/* [noscript] purpleIAccount getAccountByPurpleAccount (in PurpleNativeAccount aAccount); */
NS_IMETHODIMP purpleCoreService::GetAccountByPurpleAccount(PurpleAccount * aAccount,
							   purpleIAccount **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    if (mAccounts[i]->IsPurpleAccount(aAccount)) {
      *aResult = mAccounts[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

/* [noscript] void fireAccountEvent (in purpleNativeAccount aAccount, in string event); */
NS_IMETHODIMP purpleCoreService::FireAccountEvent(PurpleAccount * aAccount,
						  const char *aEvent)
{
  nsCOMPtr<purpleIAccount> account;
  nsresult rv = this->GetAccountByPurpleAccount(aAccount,
						getter_AddRefs(account));
  NS_ENSURE_SUCCESS(rv, rv);
  return notifyObservers(account, aEvent);
}

/* [noscript] void fireBuddyEvent (in purpleNativeBuddy aBuddy, in string event); */
NS_IMETHODIMP purpleCoreService::FireBuddyEvent(PurpleBuddy * aBuddy,
						  const char *aEvent)
{
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("attempting to send %s signal\n", aEvent);
#endif
  nsCOMPtr<purpleIBuddy> buddy;
  nsresult rv = this->GetBuddyByPurpleBuddy(aBuddy, getter_AddRefs(buddy));
  NS_ENSURE_SUCCESS(rv, rv);
  return notifyObservers(buddy, aEvent);
}
