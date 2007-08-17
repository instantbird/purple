#include "purpleCoreService.h"
#include "purpleProtocol.h"
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

  mConversations = do_CreateInstance(NS_ARRAY_CONTRACTID);

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

  mConversations = nsnull;
  UnInitAccounts();
  UnInitProtocols();
  mInitialized = PR_FALSE;
  purple_core_quit();
  printf("purple_core_quit\n");

  return NS_OK;
}

/* readonly attribute string version; */
NS_IMETHODIMP purpleCoreService::GetVersion(char * *aVersion)
{
  return returnString(aVersion, purple_core_get_version());
}

/* readonly attribute nsIArray conversations; */
NS_IMETHODIMP
purpleCoreService::GetConversations(nsIArray * *aConversations)
{
  *aConversations = mConversations;
  NS_ADDREF(*aConversations);

  return NS_OK;
}

/* purpleIConversation findConversationById (in long id); */
NS_IMETHODIMP
purpleCoreService::FindConversationById(PRInt32 aId,
					purpleIConversation **_retval)
{
  if (!aId)
    return NS_ERROR_NULL_POINTER;

#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("FindConversationById(%i)\n", aId);
#endif

  PRUint32 length;
  mConversations->GetLength(&length);
  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<purpleIConversation> conv;
    mConversations->QueryElementAt(i, NS_GET_IID(purpleIConversation),
				   getter_AddRefs(conv));
    PRInt32 id;
    conv->GetId(&id);
    if (id == aId) {
      *_retval = conv;
      NS_ADDREF(conv);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

void purpleCoreService::AddConversation(purpleIConversation* aConv)
{
  mConversations->AppendElement(aConv, PR_FALSE);
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

  return os->NotifyObservers(aObj, aEvent, nsnull);
}

/* [noscript] void fireAccountEvent (in purpleNativeAccount aAccount, in string event); */
NS_IMETHODIMP purpleCoreService::FireAccountEvent(PurpleAccount * aAccount,
						  const char *aEvent)
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    if (mAccounts[i]->IsPurpleAccount(aAccount)) {
      return notifyObservers(mAccounts[i], aEvent);
    }
  }

  return NS_ERROR_FAILURE;
}
