/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Instantbird messenging client, released
 * 2007.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "purpleCoreService.h"
#include "purpleStorage.h"
#include "purpleSockets.h"
#include "purpleDNS.h"
#include "purpleTimer.h"

#pragma GCC visibility push(default)
#include <libpurple/core.h>
#include <libpurple/savedstatuses.h>
#pragma GCC visibility pop

#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsIObserverService.h>
#include <nsIConsoleService.h>
#include <nsIPrefService.h>
#include <nsCOMPtr.h>
#include <nsCRTGlue.h>
#include <nsXPCOM.h>
#include <nsArrayEnumerator.h>

#ifdef PR_LOGGING
#include <prprf.h>
//
// NSPR_LOG_MODULES=purpleCoreService:5
//
static PRLogModuleInfo *gPurpleCoreServiceLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleCoreServiceLog, PR_LOG_DEBUG, args)


#define PREF_MESSENGER_ACCOUNTS "messenger.accounts"

NS_IMPL_ISUPPORTS1(purpleCoreService, purpleICoreService)

extern nsresult init_libpurple();
extern void connect_to_blist_signals();

purpleCoreService::purpleCoreService()
  :mInitialized(PR_FALSE),
   mQuitting(PR_FALSE),
   mProtocols(nsnull),
   mProxies(nsnull),
   mAccounts(nsnull),
   mConversations(nsnull)
{
#ifdef PR_LOGGING
  if (!gPurpleCoreServiceLog)
    gPurpleCoreServiceLog = PR_NewLogModule("purpleCoreService");
#endif
  mPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  mBuddiesById.Init();
  mTagById.Init();
}

purpleCoreService::~purpleCoreService()
{
  if (mInitialized)
    this->Quit();
}

/* boolean Init (); */
NS_IMETHODIMP purpleCoreService::Init()
{
  /* We shouldn't init the core twice. If the core service is already
     initialized, return with an error */
  NS_ENSURE_TRUE(!mInitialized, NS_ERROR_ALREADY_INITIALIZED);

  purpleSocketWatcher::init();
  purpleDNS::init();
  purpleTimer::init();

  /* Init the core of libpurple */
  nsresult rv = init_libpurple();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Load the list of protocol plugins and their specific options */
  InitProtocols();

  /*Load the list of proxies. Should be done before loading the accounts */
  InitProxies();

  /* Set to true before the end of the initialization because
     InitAccounts calls GetProtoById that ensures the core is
     initialized */
  mInitialized = PR_TRUE;

  /* Load saved accounts */
  InitAccounts();

  /* Load the buddy list from mozStorage */
  InitBuddyList();

  LOG(("Libpurple initialized"));

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

    LOG(("pcs::InitBuddyList: reading group %s (id %i)",
         name.get(), id));
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
    LOG(("pcs::InitBuddyList: reading buddy %s of account %i, group %i",
         name.get(), accountId, tagId));

    nsCOMPtr<purpleTag> tag;
    if (!mTagById.Get(tagId, getter_AddRefs(tag))) {
      // Should never happen. Corrupted blist.sqlite ?
#ifdef PR_LOGGING
      char errorMsg[60];
      if (PR_snprintf(errorMsg, 60, "pcs::InitBuddyList: Cannot get group with id %i", tagId) > 0)
        NS_WARNING(errorMsg);
#endif
      continue;
    }
    nsCOMPtr<purpleAccount> account;
    rv = GetAccountById(accountId, getter_AddRefs(account));
    if (NS_FAILED(rv)) {
      // Should never happen. Corrupted blist.sqlite ?
#ifdef PR_LOGGING
      char errorMsg[60];
      if (PR_snprintf(errorMsg, 60, "pcs::InitBuddyList: Cannot get account with id %i", accountId) > 0)
        NS_WARNING(errorMsg);
#endif
      continue;
    }

    // When alias == name, don't set the alias so that when calling
    // purple_buddy_get_alias this useless alias doesn't override the
    // server-side alias (needed for MSN that won't send server-side
    // aliases when syncing the buddy list and will wait until the
    // buddies are online to send it)
    PurpleBuddy *buddy =
      purple_buddy_new(account->GetPurpleAccount(), name.get(),
                       alias.Equals(name) ? NULL : alias.get());
    tag->addBuddy(buddy);

    LOG(("Looking for purpleBuddy with id %i in HashTable", id));
    nsCOMPtr<purpleBuddy> purpleBuddy;
    if (!mBuddiesById.Get(id, getter_AddRefs(purpleBuddy))) {
      purpleBuddy = do_CreateInstance(PURPLE_BUDDY_CONTRACTID, &rv);
      if (NS_FAILED(rv))
        continue;
      purpleBuddy->load(buddy, account, id, tag);
      mBuddiesById.Put(id, purpleBuddy);
      LOG(("purpleBuddy added in HashTable with Id %i", id));
    }
    else {
      LOG(("Found purpleBuddy with id %i in HashTable, adding account", id));
      purpleBuddy->AddAccount(account, buddy, tag);
    }
    buddy->node.ui_data = purpleBuddy;
  }

  connect_to_blist_signals();
  return NS_OK;
}

void purpleCoreService::InitProtocols()
{
  LOG(("Start init protocols"));

#ifdef PR_LOGGING
  // this code demonstrate how to enumerate protocols from libpurple
  GList *iter = purple_plugins_get_protocols();
  for (int i = 0; iter; iter = iter->next) {
    PurplePlugin *plugin = (PurplePlugin *) iter->data;
    PurplePluginInfo *info = plugin->info;
    if (info && info->name) {
      LOG(("\t%d: %s (%s)", i++, info->name, info->id));
    }
  }
#endif

  for (GList *iter = purple_plugins_get_protocols();
       iter; iter = iter->next) {
    PurplePlugin *plugin = (PurplePlugin *) iter->data;
    PurplePluginInfo *info = plugin->info;
    if (info && info->name) {
      LOG(("Trying to create %s :\t (%s)", info->name, info->id));
      nsCOMPtr<purpleProtocol> proto = do_CreateInstance(PURPLE_PROTOCOL_CONTRACTID);
      proto->Init(info);
      mProtocols.AppendObject(proto);
    }
  }
  LOG(("End init protocols"));
}

/* void Quit (); */
NS_IMETHODIMP purpleCoreService::Quit()
{
  PURPLE_ENSURE_INIT(mInitialized);
  NS_ENSURE_TRUE(!mQuitting, NS_ERROR_FAILURE);

  mQuitting = PR_TRUE;
  NotifyObservers(this, "purple-quit");

#define UNINIT(aThing)                                          \
  for (PRInt32 i = m##aThing.Count() - 1; i >= 0; --i) {        \
    m##aThing[i]->UnInit();                                     \
  }                                                             \
  m##aThing.Clear()

  UNINIT(Conversations);
  UNINIT(Accounts);
  UNINIT(Protocols);
  mProxies.Clear();
  mInitialized = PR_FALSE;
  purple_core_quit();
  LOG(("purple_core_quit"));
  mBuddiesById.Clear(); // FIXME: this can and should be done before
                        // purple_core_quit() once the blist signal
                        // handlers are removed properly
  purpleStorage::unInit();
  purpleSocketWatcher::unInit();
  purpleDNS::unInit();
  purpleTimer::unInit();

  mInitialized = PR_FALSE;
  return NS_OK;
}

/* readonly attribute AUTF8String version; */
NS_IMETHODIMP purpleCoreService::GetVersion(nsACString& aVersion)
{
  aVersion = purple_core_get_version();
  return NS_OK;
}

/* purpleIBuddy getBuddyById (in long aId); */
NS_IMETHODIMP purpleCoreService::GetBuddyById(PRInt32 aId,
                                              purpleIBuddy **aResult)
{
  LOG(("pcs->GetBuddyById(%i)", aId));

  purpleBuddy *buddyPtr = NULL;
  if (mBuddiesById.Get(aId, &buddyPtr)) {
    *aResult = buddyPtr;
    LOG(("purpleBuddy found in HashTable (by Id)"));
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

/* [noscript] purpleIBuddy getBuddyByPurpleBuddy (in PurpleNativeBuddy aBuddy); */
NS_IMETHODIMP purpleCoreService::GetBuddyByPurpleBuddy(PurpleBuddy * aBuddy,
                                                       purpleIBuddy **aResult)
{
  LOG(("pcs::GetBuddyByPurpleBuddy(buddy = %s, group = %s)", aBuddy->name,
       purple_group_get_name(purple_buddy_get_group(aBuddy))));

  /* First, check the ui_data pointer, if it is not NULL then just
   * return the associated purpleBuddy.
   */
  purpleBuddy *buddyPtr = (purpleBuddy *)aBuddy->node.ui_data;
  if (buddyPtr) {
    *aResult = buddyPtr;
    NS_ADDREF(*aResult);
    LOG(("purpleBuddy found in ui_data"));
    return NS_OK;
  }

  /* If we can't find it with the ui_data pointer, try to get the Id
   * from mozStorage. If we find it, append the new PurpleBuddy to it,
   * and add the new PurpleBuddy pointer to the hash.
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

  LOG(("pcs->GetBuddyByPurpleBuddy(buddy = %s (%s)) Searching in mozStorage",
       name.get(), prpl.get()));

  PRBool found;
  rv = statement->ExecuteStep(&found);
  NS_ENSURE_SUCCESS(rv, rv);
  if (found) {
    PRInt32 id = statement->AsInt32(0);
    rv = GetBuddyById(id, aResult);
    /* If we fail here, it's probably because the mozStorage entry
       we found is some junk that should have been removed when an
       account or a buddy has been deleted, in this case, do as if we
       didn't find it in the database */
    if (NS_SUCCEEDED(rv)) {
      purpleBuddy *buddy = static_cast<purpleBuddy *>(*aResult);
      aBuddy->node.ui_data = buddy;
      return buddy->AddAccount(aBuddy);
    }
    NS_WARNING("Let's ignore the dummy mozStorage entry we found!");
  }

  /* At this point, if we still haven't found it, this is a NEW
   * buddy. Create a new purpleBuddy instance, initialize it, store it
   * in mozStorage and add it in the two hashes.
   */
  nsCOMPtr<purpleBuddy> buddy = do_CreateInstance(PURPLE_BUDDY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  buddy->Init(aBuddy);
  aBuddy->node.ui_data = buddy;
  LOG(("purpleBuddy added in HashTable with Id %i", buddy->getId()));
  mBuddiesById.Put(buddy->getId(), buddy);

  *aResult = buddy;
  NS_ADDREF(*aResult);
  return NS_OK;
}

/* void addBuddy (in purpleIAccount aAccount, in purpleITag aTag, in AUTF8String aName); */
NS_IMETHODIMP purpleCoreService::AddBuddy(purpleIAccount *aAccount,
                                          purpleITag *aTag, const nsACString& aName)
{
  NS_ENSURE_ARG_POINTER(aAccount);
  NS_ENSURE_ARG_POINTER(aTag);
  NS_ENSURE_ARG(!aName.IsEmpty());

  purpleTag *tag = static_cast<purpleTag *>(aTag);
  PurpleAccount *account = static_cast<purpleAccount *>(aAccount)->GetPurpleAccount();
  PurpleBuddy *buddy = purple_buddy_new(account, PromiseFlatCString(aName).get(), NULL);
  tag->addBuddy(buddy);
  purple_account_add_buddy(account, buddy);
  return NS_OK;
}

/* [noscript] void purpleBuddyRemoved (in PurpleNativeBuddy aBuddy); */
NS_IMETHODIMP purpleCoreService::PurpleBuddyRemoved(PurpleBuddy *aBuddy)
{
  purpleBuddy *buddy = (purpleBuddy *)aBuddy->node.ui_data;
  if (buddy)
    return buddy->RemoveAccount(aBuddy);

  return NS_ERROR_FAILURE;
}

/* purpleITag createTag (in AUTF8String aName); */
NS_IMETHODIMP purpleCoreService::CreateTag(const nsACString& aName,
                                           purpleITag **aResult)
{
  NS_ENSURE_ARG(!aName.IsEmpty());

  //FIXME unacceptable code duplication from GetTagFromPurpleBuddy

  // if the tag already exists, we don't want to create a duplicate
  nsCString groupName = PromiseFlatCString(aName);

  nsCOMPtr<purpleTag> tag;
  nsresult rv = GetTagFromName(groupName, getter_AddRefs(tag));
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
    LOG(("pcs::CreateTag: stored group %s", groupName.get()));
    PRInt32 id;
    rv = GetTagIdFromName(groupName, &id);
    NS_ENSURE_SUCCESS(rv, rv);

    PurpleGroup *group = purple_group_new(groupName.get());
    tag = do_CreateInstance(PURPLE_TAG_CONTRACTID);
    tag->Init(id, group);
    mTagById.Put(id, tag);
    purple_blist_add_group(group, NULL);
  }
  *aResult = tag;
  NS_ADDREF(*aResult);

  return NS_OK;
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
    LOG(("pcs::GetTagIdFromName(name = %s), found at id %i", aName.get(), *aResult));
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
    LOG(("pcs::GetTagFromPurpleBuddy: stored group %s", groupName.get()));
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
NS_IMETHODIMP purpleCoreService::GetConversationById(PRUint32 aId,
                                                     purpleIConversation **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  LOG(("GetConversationById(%i)", aId));

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

void purpleCoreService::RemoveConversation(PurpleConversation *aConv)
{
  LOG(("purpleCoreService::RemoveConversation aConv = %i", (size_t) aConv));

  purpleConversation *conv = (purpleConversation *)aConv->ui_data;
  for (PRInt32 i = mConversations.Count() - 1; i >= 0; --i) {
    if (mConversations[i] == conv) {
      LOG(("purpleCoreSevice::RemoveConversation: Found"));
      mConversations[i]->UnInit();
      mConversations.RemoveObjectAt(i);
      return;
    }
  }
  NS_WARNING("purpleCoreSevice::RemoveConversation: Not Found");
}

/* nsISimpleEnumerator getProtocols (); */
NS_IMETHODIMP purpleCoreService::GetProtocols(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  return NS_NewArrayEnumerator(aResult, mProtocols);
}

/* purpleIProtocol getProtocolById (in AUTF8String aProtocolId); */
NS_IMETHODIMP purpleCoreService::GetProtocolById(const nsACString& aProtocolId, purpleIProtocol **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mProtocols.Count() - 1; i >= 0; --i) {
    if (aProtocolId.Equals(mProtocols[i]->GetId())) {
      *aResult = mProtocols[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

/* [noscript] purpleNativeProtocol getProtoById (in AUTF8String aProtocolId); */
NS_IMETHODIMP purpleCoreService::GetProtoById(const nsACString& aProtocolId, purpleProtocol * *aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mProtocols.Count() - 1; i >= 0; --i) {
    if (aProtocolId.Equals(mProtocols[i]->GetId())) {
      *aResult = mProtocols[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

#ifdef DEBUG
  nsCString warning("Failed to get protocol ");
  warning.Append(aProtocolId);
  NS_WARNING(warning.get());
#endif
  return NS_ERROR_FAILURE;
}


nsresult purpleCoreService::InitProxies()
{
  /* Get the list of proxies */
  nsCString proxyList;
  nsresult rv = mPrefService->GetCharPref(PREF_MESSENGER_PROXIES,
                                          getter_Copies(proxyList));

  LOG(("InitProxies list = %s", proxyList.get()));

  nsCOMPtr<purpleProxy> proxy;
  char *newStr = proxyList.BeginWriting();
  nsCAutoString key;
  for (char *token = NS_strtok(",", &newStr);
       token;
       token = NS_strtok(",", &newStr)) {
    key = token;
    key.StripWhitespace();

    if (key.IsEmpty()) {
      continue;
    }

    LOG(("Creating proxy %s", key.get()));

    /* create the purpleProxy object */
    proxy = do_CreateInstance(PURPLE_PROXY_CONTRACTID);
    NS_ENSURE_TRUE(proxy, NS_ERROR_OUT_OF_MEMORY);

    LOG(("Creating proxy %s", key.get()));

    rv = proxy->Init(key);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to init proxy!");
      continue;
    }

    /* Keep it in the local proxy list */
    mProxies.AppendObject(proxy);
    LOG(("Adding proxy %s in the list", key.get()));
  }

  /* Read the pref indicating the global proxy settings */
  rv = mPrefService->GetCharPref(PREF_MESSENGER_GLOBAL_PROXY,
                                 getter_Copies(key));
  NS_ENSURE_SUCCESS(rv, rv);

  /* Init mGlobalProxy */
  if (StringBeginsWith(key, NS_LITERAL_CSTRING(PROXY_KEY))) {
    for (PRInt32 i = mProxies.Count() - 1; i >= 0; --i) {
      if (mProxies[i]->GetKey().Equals(key)) {
        mGlobalProxy = mProxies[i];
        break;
      }
    }
  }

  if (!mGlobalProxy) {
    mGlobalProxy = do_CreateInstance(PURPLE_PROXY_INFO_CONTRACTID);
    if (!mGlobalProxy)
      return NS_ERROR_OUT_OF_MEMORY;
    if (key.Equals(PROXY_KEY_ENVVAR))
      mGlobalProxy->SetType(purpleIProxyInfo::useEnvVar);
    else
      mGlobalProxy->SetType(purpleIProxyInfo::noProxy);
  }

  /* Give the information to libpurple */
  PurpleProxyInfo *info;
  rv = mGlobalProxy->GetPurpleProxy(&info);
  NS_ENSURE_SUCCESS(rv, rv);
  purple_global_proxy_set_info(info);

  return NS_OK;
}

/* purpleIProxy createProxy (in short aType, in AUTF8String aHost,
                             in unsigned long aPort, in AUTF8String aUsername,
                             in AUTF8String aPassword); */
NS_IMETHODIMP purpleCoreService::CreateProxy(PRInt16 aType, const nsACString & aHost,
                                             PRUint32 aPort, const nsACString & aUsername,
                                             const nsACString & aPassword,
                                             purpleIProxy **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  /* First check that we don't already have an identical proxy */
  for (PRInt32 j = mProxies.Count() - 1; j >= 0; --j) {
    if ((mProxies[j]->Equals(aType, aHost, aPort, aUsername, aPassword))) {
      NS_ADDREF(*aResult = mProxies[j]);
      return NS_OK;
    }
  }

  /* Get the list of proxies */
  nsCString proxyList;
  nsresult rv = mPrefService->GetCharPref(PREF_MESSENGER_PROXIES,
                                          getter_Copies(proxyList));
  NS_ENSURE_SUCCESS(rv, rv);

  /* Get a new unique proxy key */
  nsCString key;
  PRInt32 i = 1;
  PRBool found = PR_FALSE;
  do {
    key = PROXY_KEY;
    key.AppendInt(i);
    for (PRInt32 j = mProxies.Count() - 1; j >= 0; --j) {
      if ((found = mProxies[j]->GetKey().Equals(key))) {
        break;
      }
    }
    ++i;
  } while (found);

  /* Actually create the new proxy */
  nsCOMPtr<purpleProxy> proxy = do_CreateInstance(PURPLE_PROXY_CONTRACTID);
  rv = proxy->Init(key, aType, aHost, aPort, aUsername, aPassword);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Save the proxy list pref */
  if (proxyList.IsEmpty())
    proxyList = key;
  else {
    proxyList.Append(',');
    proxyList.Append(key);
  }
  mPrefService->SetCharPref(PREF_MESSENGER_PROXIES, proxyList.get());

  /* Keep it in the local proxy list */
  mProxies.AppendObject(proxy);

  /* And return the new proxy as result */
  NS_ADDREF(*aResult = proxy);
  return NS_OK;
}

/* nsISimpleEnumerator getProxies (); */
NS_IMETHODIMP purpleCoreService::GetProxies(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  return NS_NewArrayEnumerator(aResult, mProxies);
}

/* attribute purpleIProxyInfo globalProxy; */
NS_IMETHODIMP purpleCoreService::GetGlobalProxy(purpleIProxyInfo * *aGlobalProxy)
{
  PURPLE_ENSURE_INIT(mInitialized);

  NS_ADDREF(*aGlobalProxy = mGlobalProxy);
  return NS_OK;
}
NS_IMETHODIMP purpleCoreService::SetGlobalProxy(purpleIProxyInfo * aGlobalProxy)
{
  PURPLE_ENSURE_INIT(mInitialized);
  NS_ENSURE_ARG_POINTER(aGlobalProxy);

  mGlobalProxy = aGlobalProxy;

  // Save the pref
  nsCString key;
  nsresult rv = mGlobalProxy->GetKey(key);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mPrefService->SetCharPref(PREF_MESSENGER_GLOBAL_PROXY, key.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Give it to libpurple;
  PurpleProxyInfo *info;
  rv = mGlobalProxy->GetPurpleProxy(&info);
  NS_ENSURE_SUCCESS(rv, rv);

  purple_global_proxy_set_info(info);

  return NS_OK;
}


/* purpleIAccount getAccountById (in AUTF8String accountId); */
NS_IMETHODIMP purpleCoreService::GetAccountById(const nsACString& aAccountId, purpleIAccount **aResult)
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

  LOG(("InitAccounts list = %s", accountList.get()));

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

    /* Attempt to connect the account if autoLogin is set */
    account->checkAutoLogin();
  }

  return NS_OK;
}

/* purpleIAccount createAccount(in AUTF8String aName, in AUTF8String aPrpl); */
NS_IMETHODIMP purpleCoreService::CreateAccount(const nsACString &aName,
                                               const nsACString &aPrpl,
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
  rv = GetProtoById(aPrpl, getter_AddRefs(proto));
  NS_ENSURE_SUCCESS(rv, rv);

  /* Actually create the new account */
  nsCOMPtr<purpleAccount> account = do_CreateInstance(PURPLE_ACCOUNT_CONTRACTID);
  rv = account->create(aName, proto, key);
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

/* void deleteAccount (in AUTF8String accountId); */
NS_IMETHODIMP purpleCoreService::DeleteAccount(const nsACString& aAccountId)
{
  LOG(("Attempting to delete %s", PromiseFlatCString(aAccountId).get()));

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

  NS_ENSURE_TRUE(i >= 0, NS_ERROR_FAILURE);

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

  LOG(("Notifying observers of %s", aEvent));
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
    LOG(("mAccounts[%i]: %s", i, mAccounts[i]->IsPurpleAccount(NULL) ? "NULL" : "not null"));
  }

  return NS_ERROR_FAILURE;
}

/* [noscript] void fireAccountEvent (in purpleNativeAccount aAccount, in string event); */
NS_IMETHODIMP purpleCoreService::FireAccountEvent(PurpleAccount * aAccount,
                                                  const char *aEvent)
{
  LOG(("FireAccountEvent: event: %s, account name: %s", aEvent, aAccount->username));

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
  LOG(("Attempting to send %s signal, group = %s, buddy = %s", aEvent,
       purple_group_get_name(purple_buddy_get_group(aBuddy)), aBuddy->name));
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
