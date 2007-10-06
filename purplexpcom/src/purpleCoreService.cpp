#include "purpleCoreService.h"
#include "purpleProtocol.h"
#include "purpleBuddy.h"
#include "purpleUtils.h"
#include "purpleStorage.h"
#include "purpleSockets.h"
#include "nsXPCOM.h"
#include <glib.h>
#include <libpurple/core.h>
#include <libpurple/account.h>
#include <libpurple/savedstatuses.h>

#include<nsServiceManagerUtils.h>
#include<nsComponentManagerUtils.h>
#include<nsIObserverService.h>
#include <nsIConsoleService.h>
#include<pref/nsIPrefService.h>
#include<nsCOMPtr.h>
#include<nsCRTGlue.h>
#include<stdio.h>

#include<nsArrayEnumerator.h>

//#define DEBUG_PURPLE_CORE_SERVICE

#define PREF_MESSENGER_ACCOUNTS "messenger.accounts"

NS_IMPL_ISUPPORTS1(purpleCoreService, purpleICoreService)

extern nsresult init_libpurple();
extern void connect_to_blist_signals();

purpleCoreService::purpleCoreService()
  :mInitialized(PR_FALSE),
   mConversations(nsnull),
   mProtocols(nsnull)
{
  mPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  mBuddiesByPurpleBuddy.Init();
  mBuddiesById.Init();
  mTagById.Init();
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

  /* Load the buddy list from mozStorage */
  InitBuddyList();

#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("libpurple initialized\n");
#endif

  return NS_OK;
}

nsresult purpleCoreService::InitBuddyList()
{
  /* Create the purple buddylist. */
  purple_set_blist(purple_blist_new());
  /* This returns early because the file blist.xml can't be found, but
     the important thing is that it sets blist_loaded to TRUE */
  purple_blist_load();

  /* Now, let's read the real data from mozStorage */
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, name FROM tags"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMoreData;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    PRInt32 id = statement->AsInt32(0);
    nsCString name;
    rv = statement->GetUTF8String(1, name);
    NS_ENSURE_SUCCESS(rv, rv);
#ifdef DEBUG_PURPLE_CORE_SERVICE 
    printf("pcs::InitBuddyList: reading group %s (id %i)\n",
           name.get(), id);
#endif
    PurpleGroup *group = purple_group_new(name.get());
    nsCOMPtr<purpleTag> tag = do_CreateInstance(PURPLE_TAG_CONTRACTID);
    tag->Init(id, group);
    mTagById.Put(id, tag);
    purple_blist_add_group(group, NULL);
  }

  rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, name, srv_alias, tag_id, account_id FROM buddies JOIN account_buddy ON buddy_id = id"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    PRInt32 id = statement->AsInt32(0);
    nsCString name;
    rv = statement->GetUTF8String(1, name);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString alias;
    rv = statement->GetUTF8String(2, alias);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt32 tagId = statement->AsInt32(3);
    PRInt32 accountId = statement->AsInt32(4);
#ifdef DEBUG_PURPLE_CORE_SERVICE 
    printf("pcs::InitBuddyList: reading buddy %s of account %i, group %i\n",
           name.get(), accountId, tagId);
#endif
    nsCOMPtr<purpleTag> tag;
    if (!mTagById.Get(tagId, getter_AddRefs(tag))) {
      // Should never happen. Corrupted blist.sqlite ?
#ifdef DEBUG_PURPLE_CORE_SERVICE 
      printf("pcs::InitBuddyList: Cannot get group with id %i\n", tagId);
#endif
      continue; 
    }
    nsCOMPtr<purpleAccount> account;
    rv = GetAccountById(accountId, getter_AddRefs(account));
    if (NS_FAILED(rv)) {
      // Should never happen. Corrupted blist.sqlite ?
#ifdef DEBUG_PURPLE_CORE_SERVICE 
      printf("pcs::InitBuddyList: Cannot get account with id %i\n", accountId);
#endif
      continue; 
    }
    PurpleBuddy *buddy = purple_buddy_new(account->GetPurpleAccount(),
                                          name.get(), alias.get());
    tag->addBuddy(buddy);

#ifdef DEBUG_PURPLE_CORE_SERVICE 
    printf("Looking for purpleBuddy with id %i in HashTable\n", id);
#endif
    nsCOMPtr<purpleBuddy> purpleBuddy;
    if (!mBuddiesById.Get(id, getter_AddRefs(purpleBuddy))) {
      purpleBuddy = do_CreateInstance(PURPLE_BUDDY_CONTRACTID, &rv);
      if (NS_FAILED(rv))
        continue;
      purpleBuddy->load(buddy, account, id, tag);
      mBuddiesById.Put(id, purpleBuddy);
#ifdef DEBUG_PURPLE_CORE_SERVICE
      printf("purpleBuddy added in HashTable with Id %i\n", id);
#endif
    }
    else {
#ifdef DEBUG_PURPLE_CORE_SERVICE 
      printf("found purpleBuddy with id %i in HashTable, adding account\n", id);
#endif
      purpleBuddy->AddAccount(account, buddy, tag);
    }
    mBuddiesByPurpleBuddy.Put((PRUint32) buddy, purpleBuddy);
  }

  connect_to_blist_signals();
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
  UnInitAccounts();
  UnInitProtocols();
  mInitialized = PR_FALSE;
  NotifyObservers(this, "purple-quit");
  purple_core_quit();
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("purple_core_quit\n");
#endif
  mBuddiesByPurpleBuddy.Clear(); // FIXME: useless
  mBuddiesById.Clear(); // FIXME: this can and should be done before
                        // purple_core_quit() once the blist signal
                        // handlers are removed properly
  purpleStorage::unInit();
  purpleSocketWatcher::unInit();

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
  printf("pcs->GetBuddyById(%i)\n", aId);
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
#ifdef DEBUG_PURPLE_CORE_SERVICE 
  printf("pcs::GetBuddyByPurpleBuddy(buddy = %s, group = %s)\n", aBuddy->name,
         purple_group_get_name(purple_buddy_get_group(aBuddy)));
#endif

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
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  mozIStorageStatement *statement = storageInstance->mGetBuddyIdFromNameAndPrpl;
  mozStorageStatementScoper scoper(statement);
  nsCString name(purple_normalize(aBuddy->account, aBuddy->name));
  nsresult rv = statement->BindUTF8StringParameter(0, name);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString prpl(aBuddy->account->protocol_id);
  rv = statement->BindUTF8StringParameter(1, prpl);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_CORE_SERVICE 
  printf("pcs->GetBuddyByPurpleBuddy(buddy = %s (%s)) Searching in mozStorage\n",
         name.get(), prpl.get());
#endif

  PRBool found;
  rv = statement->ExecuteStep(&found);
  NS_ENSURE_SUCCESS(rv, rv);
  if (found) {
    PRInt32 id = statement->AsInt32(0);
    rv = GetBuddyById(id, aResult);
    NS_ENSURE_SUCCESS(rv, rv);
    purpleBuddy *buddy = static_cast<purpleBuddy *>(*aResult);
    mBuddiesByPurpleBuddy.Put((PRUint32) aBuddy, buddy);
    return buddy->AddAccount(aBuddy);
  }

  /* At this point, if we still haven't found it, this is a NEW
   * buddy. Create a new purpleBuddy instance, initialize it, store it
   * in mozStorage and add it in the two hashes.
   */
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

/* void addBuddy (in purpleIAccount aAccount, in purpleITag aTag, in string aName); */
NS_IMETHODIMP purpleCoreService::AddBuddy(purpleIAccount *aAccount,
                                          purpleITag *aTag, const char *aName)
{
  NS_ENSURE_ARG_POINTER(aAccount);
  NS_ENSURE_ARG_POINTER(aTag);

  purpleTag *tag = static_cast<purpleTag *>(aTag);
  PurpleAccount *account = static_cast<purpleAccount *>(aAccount)->GetPurpleAccount();
  PurpleBuddy *buddy = purple_buddy_new(account, aName, NULL);
  tag->addBuddy(buddy);
  purple_account_add_buddy(account, buddy);
  return NS_OK;
}

/* [noscript] void purpleBuddyRemoved (in PurpleNativeBuddy aBuddy); */
NS_IMETHODIMP purpleCoreService::PurpleBuddyRemoved(PurpleBuddy *aBuddy)
{
  nsCOMPtr<purpleBuddy> buddy;
  if (mBuddiesByPurpleBuddy.Get((PRUint32) aBuddy, getter_AddRefs(buddy)))
    return buddy->RemoveAccount(aBuddy);

  return NS_ERROR_FAILURE;
}

/* purpleITag getTagById (in long aId); */
NS_IMETHODIMP purpleCoreService::GetTagById(PRInt32 aId, purpleITag **aResult)
{
  purpleTag *tag;
  if (mTagById.Get(aId, &tag)) {
    *aResult = tag;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult purpleCoreService::GetTagIdFromName(const nsCString& aName,
                                             PRInt32 *aResult)
{
  //FIXME The result should be cached.
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageStatement *statement = storageInstance->mGetGroupIdFromName;
  mozStorageStatementScoper scoper(statement);
  nsresult rv = statement->BindUTF8StringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMoreData;
  if (NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    *aResult = statement->AsInt32(0);
#ifdef DEBUG_PURPLE_CORE_SERVICE 
    printf("pcs::GetTagIdFromName(name = %s), found at id %i\n", aName.get(), *aResult);
#endif
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult purpleCoreService::GetTagFromName(const nsCString& aName,
                                           purpleTag **aResult)
{
  /* First, get the id from mozStorage */
  PRInt32 id;
  nsresult rv = GetTagIdFromName(aName, &id);

  if (NS_SUCCEEDED(rv)) {
    /* We found the id, just get the tag from the hashtable */
    if (mTagById.Get(id, aResult))
      return NS_OK;

    /* if the tag isn't in the hashtable, this means we are still
       loading the buddy list from mozStorage, and this method
       shouldn't have been called */
  }

  /* This tag doesn't exist yet */
  return NS_ERROR_FAILURE;
}

nsresult purpleCoreService::GetTagFromPurpleBuddy(PurpleBuddy *aBuddy,
                                                  purpleTag **aResult)
{
  PurpleGroup *group = purple_buddy_get_group(aBuddy);
  nsCString groupName(purple_group_get_name(group));

  nsresult rv = GetTagFromName(groupName, aResult);
  if (NS_FAILED(rv)) {
    /* Ok, we didn't find it, so this is a new tag */
    purpleStorage *storageInstance = purpleStorage::GetInstance();
    mozIStorageStatement *statement = storageInstance->mInsertGroup;
    mozStorageStatementScoper scoper(statement);
    rv = statement->BindUTF8StringParameter(0, groupName);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt32Parameter(1, 0); // FIXME (0 = not visible);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
#ifdef DEBUG_PURPLE_CORE_SERVICE 
    printf("pcs::GetTagFromPurpleBuddy: stored group %s\n", groupName.get());
#endif
    PRInt32 id;
    rv = GetTagIdFromName(groupName, &id);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<purpleTag> tag = do_CreateInstance(PURPLE_TAG_CONTRACTID);
    tag->Init(id, group);
    mTagById.Put(id, tag);
    *aResult = tag;
    NS_ADDREF(*aResult);
  }

  return rv;
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
  NotifyObservers(aConv, "new-conversation");
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
  return NS_OK; // we don't want to throw an exception if called from JS
}

nsresult purpleCoreService::GetAccountById(PRUint32 aAccountId, purpleAccount **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    if (mAccounts[i]->GetId() == aAccountId) {
      *aResult = mAccounts[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
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

  /* Prepare the statement that will be used to check that the key
     isn't already used in the database */
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id FROM accounts WHERE id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  /* First get a new unique account key */
  nsCString key;
  PRInt32 i = 1;
  PRBool found = PR_FALSE;
  do {
    key = ACCOUNT_KEY;
    key.AppendInt(i);
    for (PRInt32 j = mAccounts.Count() - 1; j >= 0; --j) {
      if ((found = mAccounts[j]->GetKey().Equals(key))) {
	break;
      }
    }
    if (!found) {
      /* We have found a key that isn't used in the pref, double check
         it isn't already saved in the sqlite database. This should
         never happen. */
      statement->BindInt32Parameter(0, i);
      rv = statement->ExecuteStep(&found);
      NS_ENSURE_SUCCESS(rv, rv);
      if (found) {
        /* This should never happen so log a string to the error console */
        nsCOMPtr<nsIConsoleService> consoleService = 
          do_GetService( "@mozilla.org/consoleservice;1" );

        nsString msg(NS_LITERAL_STRING("No account "));
        msg.AppendInt(i);
        msg.Append(NS_LITERAL_STRING(" but there is some data in the buddy list for an account with this number. Your profile may be corrupted."));
        consoleService->LogStringMessage(msg.get());
      }
    }
    ++i;
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
  NotifyObservers(account, "account-added");

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
      NotifyObservers(mAccounts[i], "account-removed");
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

/* void notifyObservers (in nsISupports aObj, in string aEvent); */
NS_IMETHODIMP purpleCoreService::NotifyObservers(nsISupports *aObj,
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
  return NotifyObservers(account, aEvent);
}

/* [noscript] void fireBuddyEvent (in purpleNativeBuddy aBuddy, in string event); */
NS_IMETHODIMP purpleCoreService::FireBuddyEvent(PurpleBuddy * aBuddy,
						  const char *aEvent)
{
#ifdef DEBUG_PURPLE_CORE_SERVICE 
  printf("attempting to send %s signal, group = %s, buddy = %s\n", aEvent,
         purple_group_get_name(purple_buddy_get_group(aBuddy)), aBuddy->name);
#endif
  nsCOMPtr<purpleIBuddy> buddy;
  nsresult rv = this->GetBuddyByPurpleBuddy(aBuddy, getter_AddRefs(buddy));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<purpleIAccountBuddy> pab;
  rv = buddy->GetAccountBuddyForPurpleBuddy(aBuddy, getter_AddRefs(pab));
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NotifyObservers(pab, aEvent);
}

/* readonly attribute mozIStorageConnection storageConnection; */
NS_IMETHODIMP purpleCoreService::GetStorageConnection(mozIStorageConnection * *aStorageConnection)
{
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  *aStorageConnection = storageInstance->GetConnection();
  NS_ADDREF(*aStorageConnection);
  return NS_OK;
}

nsresult purpleCoreService::SetStatus(PurpleStatusPrimitive aStatus,
                                      const nsACString &aMessage)
{
  PurpleSavedStatus *status = purple_savedstatus_get_default();
  NS_ENSURE_ARG_POINTER(status);

  purple_savedstatus_set_type(status, aStatus);
  purple_savedstatus_set_message(status, PromiseFlatCString(aMessage).get());
  purple_savedstatus_activate(status);
  return NS_OK;
}

/* void away (in string aMessage); */
NS_IMETHODIMP purpleCoreService::Away(const nsACString &aMessage)
{
  return SetStatus(PURPLE_STATUS_AWAY, aMessage);
}

/* void back (in string aMessage); */
NS_IMETHODIMP purpleCoreService::Back(const nsACString &aMessage)
{
  return SetStatus(PURPLE_STATUS_AVAILABLE, aMessage);
}
