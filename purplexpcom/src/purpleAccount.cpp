/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleAccount.h"
#include "purpleAccountBuddy.h"
#include "purpleCoreService.h"
#include "purpleConversation.h"
#include "purpleGListEnumerator.h"
#include "purpleSockets.h"
#include "purpleTimer.h"

#pragma GCC visibility push(default)
#include <libpurple/core.h>
#pragma GCC visibility pop

#include <imgITools.h>
#include <imgIContainer.h>
#include <nsIConsoleService.h>
#include <nsIPrefService.h>
#include <nsIProgrammingLanguage.h>
#include <nsIClassInfoImpl.h>
#include <nsComponentManagerUtils.h>
#include <nsNetUtil.h>
#include <nsServiceManagerUtils.h>
#include <nsStringStream.h>
#include <prplIProtocol.h>
#include <prprf.h>
#include "mozilla/Likely.h"

#define PREF_PREFIX             "messenger.account."
#define PREF_OPTIONS            "options."
#define PREF_PROXY              "proxy"

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleAccount:5
//
static PRLogModuleInfo *gPurpleAccountLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleAccountLog, PR_LOG_DEBUG, args)

extern int blist_signals_handle;

class purpleChatRoomField MOZ_FINAL : public prplIChatRoomField,
                                      public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PRPLICHATROOMFIELD

  purpleChatRoomField() : mType(-1) {}
  void Init(const proto_chat_entry *aChatRoomField)
  {
    mType =
      aChatRoomField->is_int ? (PRInt32) prplIChatRoomField::TYPE_INT :
      aChatRoomField->secret ? (PRInt32) prplIChatRoomField::TYPE_PASSWORD :
                               (PRInt32) prplIChatRoomField::TYPE_TEXT;
    mLabel = aChatRoomField->label;
    mIdentifier = aChatRoomField->identifier;
    mRequired = aChatRoomField->required;
    mMin = aChatRoomField->min;
    mMax = aChatRoomField->max;
  }

private:
  ~purpleChatRoomField() {}

protected:
  /* additional members */
  PRInt32 mType;
  PRInt32 mMin, mMax;
  nsCString mLabel;
  nsCString mIdentifier;
  bool mRequired;
};

// D36DC0D5-438F-4C65-865B-EDE5DB7906D5
#define PURPLE_CHAT_ROOM_FIELD_CID \
  { 0xD36DC0D5, 0x438F, 0x4C65,                               \
    { 0x86, 0x5B, 0xED, 0xE5, 0xDB, 0x79, 0x06, 0xD5 }        \
  }

NS_IMPL_CLASSINFO(purpleChatRoomField, NULL, 0, PURPLE_CHAT_ROOM_FIELD_CID)
NS_IMPL_THREADSAFE_CI(purpleChatRoomField)
NS_IMPL_ISUPPORTS1_CI(purpleChatRoomField, prplIChatRoomField)

#define PURPLE_IMPL_GETFIELDVALUE(aType, aName, aStar)                  \
  NS_IMETHODIMP purpleChatRoomField::Get##aName(aType a##aName)         \
  {                                                                     \
    NS_ENSURE_TRUE(mType != -1, NS_ERROR_NOT_INITIALIZED);              \
                                                                        \
    aStar a##aName = m##aName;                                          \
    return NS_OK;                                                       \
  }

/* readonly attribute AUTF8String label; */
PURPLE_IMPL_GETFIELDVALUE(nsACString &, Label, )

/* readonly attribute AUTF8String identifier; */
PURPLE_IMPL_GETFIELDVALUE(nsACString &, Identifier, )

/* readonly attribute boolean required; */
PURPLE_IMPL_GETFIELDVALUE(bool *, Required, *)

/* readonly attribute short type; */
PURPLE_IMPL_GETFIELDVALUE(PRInt16 *, Type, *)

/* readonly attribute long min; */
PURPLE_IMPL_GETFIELDVALUE(PRInt32 *, Min, *)

/* readonly attribute long max; */
PURPLE_IMPL_GETFIELDVALUE(PRInt32 *, Max, *)

#define MAX_KEYS 8

class purpleChatRoomFieldValues MOZ_FINAL : public prplIChatRoomFieldValues,
                                            public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PRPLICHATROOMFIELDVALUES

  purpleChatRoomFieldValues(GHashTable *aHashTable) :
    mHashTable(aHashTable),
    mKeyCount(0) {
  }
  inline GHashTable *GetHashTable() { return mHashTable; }

private:
  ~purpleChatRoomFieldValues() {
    if (MOZ_LIKELY(mHashTable))
      g_hash_table_destroy(mHashTable);

    while (mKeyCount)
      g_free(mKeys[--mKeyCount]);
  }

protected:
  /* additional members */
  GHashTable *mHashTable;

  PRInt32 mKeyCount;
  gchar *mKeys[MAX_KEYS];
};

// DB27B00F-E9E6-4FC1-B16C-AB47A7BF58ED
#define PURPLE_CHAT_ROOM_FIELD_VALUE_CID                      \
  { 0xDB27B00F, 0xE9E6, 0x4FC1,                               \
    { 0xB1, 0x6C, 0xAB, 0x47, 0xA7, 0xBF, 0x58, 0xED }        \
 }

NS_IMPL_CLASSINFO(purpleChatRoomFieldValues, NULL, 0,
                  PURPLE_CHAT_ROOM_FIELD_VALUE_CID)
NS_IMPL_THREADSAFE_CI(purpleChatRoomFieldValues)
NS_IMPL_ISUPPORTS1_CI(purpleChatRoomFieldValues, prplIChatRoomFieldValues)

/* AUTF8String getValue (in AUTF8String aIdentifier); */
NS_IMETHODIMP
purpleChatRoomFieldValues::GetValue(const nsACString & aIdentifier,
                                    nsACString & aResult)
{
  if (MOZ_UNLIKELY(!mHashTable)) {
    aResult.Truncate();
    return NS_OK;
  }

  aResult =
    static_cast<const char *>(g_hash_table_lookup(mHashTable,
                                                  PromiseFlatCString(aIdentifier).get()));
  return NS_OK;
}

/* void setValue (in AUTF8String aIdentifier, in AUTF8String aValue); */
NS_IMETHODIMP
purpleChatRoomFieldValues::SetValue(const nsACString & aIdentifier,
                                    const nsACString & aValue)
{
  if (MOZ_UNLIKELY(!mHashTable)) {
    mHashTable = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
  }

  PromiseFlatCString identifier(aIdentifier);
  char *key = const_cast<char *>(identifier.get());
  char *value = g_strdup(PromiseFlatCString(aValue).get());

  if (!g_hash_table_lookup(mHashTable, key)) {
    key = g_strdup(key);
    if (mKeyCount < MAX_KEYS)
      mKeys[mKeyCount++] = key;
    // If we have already MAX_KEYS keys, we will leak, but something
    // is already seriously wrong anyway...
  }
  g_hash_table_insert(mHashTable, key, value);

  return NS_OK;
}

// e450cf22-b98a-4095-b480-e57caf58a2eb
#define PURPLE_ACCOUNT_CID                                      \
  { 0xe450cf22, 0xb98a, 0x4095,                                 \
      { 0xb4, 0x80, 0xe5, 0x7c, 0xaf, 0x58, 0xa2, 0xeb }        \
  }

NS_IMPL_CLASSINFO(purpleAccount, NULL, 0, PURPLE_ACCOUNT_CID)
NS_IMPL_ISUPPORTS1_CI(purpleAccount, prplIAccount)

purpleAccount::purpleAccount()
  : mScoper(nullptr),
    mAccount(NULL),
    mConnectionErrorReason(NO_ERROR),
    mStatus(PURPLE_STATUS_AVAILABLE)
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleAccountLog)
    gPurpleAccountLog = PR_NewLogModule("purpleAccount");
#endif
  LOG(("Creating purpleAccount @%x\n", this));
}

purpleAccount::~purpleAccount()
{
  /* destructor code */
  LOG(("Destructing purpleAccount @%x\n", this));
  if (mAccount)
    UnInit();
}

/* void remove (); */
NS_IMETHODIMP purpleAccount::Remove()
{
  if (mAccount) {
    LOG(("Removing purpleAccount @%x\n", this));
    purple_accounts_delete(mAccount);
    mAccount = NULL;
  }
  return NS_OK;
}

/* void unInit (); */
NS_IMETHODIMP purpleAccount::UnInit()
{
  NS_PRECONDITION(mImAccount,
                  "Uninitializing uninitialized purpleAccount\n");
  NS_ENSURE_TRUE(mImAccount, NS_ERROR_NOT_INITIALIZED);

  if (!purple_get_core())
    mAccount = NULL;

#ifdef DEBUG
  nsCString protoId;
  nsCOMPtr<prplIProtocol> proto;
  nsresult rv = mImAccount->GetProtocol(getter_AddRefs(proto));
  if (proto)
    rv = proto->GetId(protoId);
  LOG(("uninitializing purpleAccount %s (%s)\n",
       mAccount ? purple_account_get_username(mAccount) : "(mAccount = NULL)",
       NS_SUCCEEDED(rv) ? protoId.get() : "unknown proto"));
#endif

  if (mAccount) {
    // disconnect before removing the ui_data pointer so that
    // account-disconnected signals can be sent
    purple_account_set_enabled(mAccount, UI_ID, FALSE);
    mAccount->ui_data = NULL;
    // This will call purple_proxy_info_destroy if there was a proxy
    purple_account_set_proxy_info(mAccount, NULL);

    purple_account_destroy(mAccount);
    mAccount = NULL;
  }
  mImAccount = nullptr;

  return NS_OK;
}

nsresult purpleAccount::Init(imIAccount *aImAccount,
                             prplIProtocol *aPrpl)
{
  NS_ENSURE_ARG_POINTER(aPrpl);
  NS_ENSURE_ARG_POINTER(aImAccount);

  mImAccount = aImAccount;

  nsresult rv = mImAccount->GetNumericId(&mId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString protoId;
  rv = aPrpl->GetId(protoId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString name;
  rv = aImAccount->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);

  mAccount = purple_account_new(name.get(), protoId.get());
  NS_ENSURE_TRUE(mAccount, NS_ERROR_FAILURE);

  mAccount->ui_data = this;
  purple_accounts_add(mAccount);

  /* Load the alias if any */
  nsCString alias;
  rv = mImAccount->GetAlias(alias);
  if (NS_SUCCEEDED(rv) && !alias.IsEmpty()) {
    purple_account_set_alias(mAccount, alias.get());
    LOG(("alias = %s\n", alias.get()));
  }

  /* create the string for the root of the pref branch */
  nsCString key;
  rv = mImAccount->GetId(key);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString root(PREF_PREFIX);
  root.Append(key);
  root.Append('.');

  /* get the branch */
  nsCOMPtr<nsIPrefService> prefService =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefService, NS_ERROR_UNEXPECTED);

  rv = prefService->GetBranch(root.get(), getter_AddRefs(mPrefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  /* get the options pref branch */
  root.Append(PREF_OPTIONS);
  rv = prefService->GetBranch(root.get(), getter_AddRefs(mPrefOptBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  /* Load proxy settings */
  nsCString proxyKey;
  rv = mPrefBranch->GetCharPref(PREF_PROXY, getter_Copies(proxyKey));
  /* Init mProxy */
  if (NS_SUCCEEDED(rv) &&
      StringBeginsWith(proxyKey, NS_LITERAL_CSTRING(PROXY_KEY))) {
    nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
    nsCOMPtr<nsISimpleEnumerator> proxies;
    rv = pcs->GetProxies(getter_AddRefs(proxies));
    bool hasNext;
    if (NS_SUCCEEDED(rv)) {
      while (NS_SUCCEEDED(proxies->HasMoreElements(&hasNext)) && hasNext) {
        nsCOMPtr<purpleIProxy> proxy;
        rv = proxies->GetNext(getter_AddRefs(proxy));
        nsCString tmpKey;
        if (NS_SUCCEEDED(rv) &&
            NS_SUCCEEDED(proxy->GetKey(tmpKey)) && tmpKey.Equals(proxyKey)) {
          mProxy = proxy;
          break;
        }
      }
    }
  }
  if (!mProxy) {
    mProxy = do_CreateInstance(PURPLE_PROXY_INFO_CONTRACTID);
    NS_ENSURE_TRUE(mProxy, NS_ERROR_OUT_OF_MEMORY);

    if (proxyKey.Equals(PROXY_KEY_ENVVAR))
      mProxy->SetType(purpleIProxyInfo::useEnvVar);
    else if (proxyKey.Equals(PROXY_KEY_NONE))
      mProxy->SetType(purpleIProxyInfo::noProxy);
    else
      mProxy->SetType(purpleIProxyInfo::useGlobal);
  }

  /* Give the information to libpurple */
  PurpleProxyInfo *info;
  rv = mProxy->GetPurpleProxy(&info);
  NS_ENSURE_SUCCESS(rv, rv);
  if (info)
    purple_account_set_proxy_info(mAccount, info);

  //get spec options
  PRUint32 count;
  char **prefs;
  rv = mPrefOptBranch->GetChildList("", &count, &prefs);
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("Number of specific pref: %i\n", count));
  while (count--) {
    PRInt32 type;
    const char *name = prefs[count];
    rv = mPrefOptBranch->GetPrefType(name, &type);
    if (NS_FAILED(rv)) {
      continue;
    }
    switch (type) {
    case nsIPrefBranch::PREF_INT:
      {
        PRInt32 val;
        rv = mPrefOptBranch->GetIntPref(name, &val);
        if (NS_SUCCEEDED(rv))
          purple_account_set_int(mAccount, name, val);
      }
      break;
    case nsIPrefBranch::PREF_BOOL:
      {
        bool val;
        rv = mPrefOptBranch->GetBoolPref(name, &val);
        if (NS_SUCCEEDED(rv))
          purple_account_set_bool(mAccount, name, val);
      }
      break;
    case nsIPrefBranch::PREF_STRING:
      {
        nsCString val;
        rv = mPrefOptBranch->GetCharPref(name, getter_Copies(val));
        if (NS_SUCCEEDED(rv))
          purple_account_set_string(mAccount, name, val.get());
      }
      break;
    default:
      continue;
    }
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, prefs);

  // Apply the current icon only if the account doesn't have a cached icon.

  // FIXME: This function is called both when creating an account and
  // when loading an existing account. Our goal here is to apply the
  // current icon to new accounts.  Usually, existing accounts
  // supporting buddy icons will have a cached icon. Unfortunately, if
  // during load we find an account that can support buddy icons but
  // has no cached buddy icon, we will apply the current buddy icon to
  // it but it won't be cached because the account ui ops aren't set
  // yet. If this happens, it will slow down each startup (because of
  // the icon conversion happening during startup). It's unlikely to
  // happen unless the user removes buddy_icon preferences from
  // about:config or a prpl which used to not support buddy icons got
  // updated.
  if (!purple_account_get_string(mAccount, "buddy_icon", NULL))
    ApplyCurrentUserIcon();
  else
    purple_buddy_icons_account_loaded(mAccount);

  return NS_OK;
}

/* attribute imIAccount imAccount; */
NS_IMETHODIMP purpleAccount::GetImAccount(imIAccount * *aImAccount)
{
  NS_IF_ADDREF(*aImAccount = mImAccount);
  return NS_OK;
}

/* void connect (); */
NS_IMETHODIMP purpleAccount::Connect()
{
  PURPLE_ENSURE_INIT(mAccount);
  LOG(("Attempting to connect %s\n", mAccount->username));
#ifdef DEBUG
  if (strcmp("prpl-null", purple_account_get_protocol_id(mAccount)))
#endif
    NS_ENSURE_TRUE(!NS_IsOffline(), NS_ERROR_FAILURE);

  nsCString password;
  nsresult rv = mImAccount->GetPassword(password);
  if (NS_SUCCEEDED(rv))
    purple_account_set_password(mAccount, password.get());

  /* enabling an account in libpurple connects it automatically */
  purpleAccountScoper scoper(mId);
  purple_account_set_enabled(mAccount, UI_ID, TRUE);
  // purple_account_connect(mAccount);

  return NS_OK;
}

/* void disconnect (); */
NS_IMETHODIMP purpleAccount::Disconnect()
{
  PURPLE_ENSURE_INIT(mAccount);
  LOG(("Attempting to disconnect %s\n", mAccount->username));

  purpleAccountScoper scoper(mId);
  purple_account_set_enabled(mAccount, UI_ID, FALSE);

  return NS_OK;
}

/* prplIConversation createConversation (in AUTF8String aName); */
NS_IMETHODIMP purpleAccount::CreateConversation(const nsACString& aName,
                                                prplIConversation **aResult)
{
  NS_ENSURE_TRUE(!aName.IsEmpty(), NS_ERROR_INVALID_ARG);

  purpleAccountScoper scoper(mId);
  PurpleConversation *conv;
  conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, this->mAccount,
                                 PromiseFlatCString(aName).get());
  NS_ENSURE_TRUE(conv, NS_ERROR_FAILURE);

  prplIConversation *result = purpleConversation::fromPurpleConv(conv);
  NS_ENSURE_TRUE(result, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*aResult = result);

  return NS_OK;
}

/* void addBuddy (in imITag aTag, in AUTF8String aName); */
NS_IMETHODIMP purpleAccount::AddBuddy(imITag *aTag,
                                      const nsACString& aName)
{
  PURPLE_ENSURE_INIT(mAccount);
  NS_ENSURE_ARG_POINTER(aTag);
  NS_ENSURE_ARG(!aName.IsEmpty());

  purpleAccountScoper scoper(mId);
  PurpleGroup *group = purpleAccountBuddy::GetPurpleGroupForTag(aTag);
  NS_ENSURE_TRUE(group, NS_ERROR_UNEXPECTED);

  PurpleBuddy *buddy =
    purple_buddy_new(mAccount, PromiseFlatCString(aName).get(), NULL);
  purple_blist_add_buddy(buddy, NULL, group, NULL);
  purple_account_add_buddy(mAccount, buddy);
  return NS_OK;
}

/* imIAccountBuddy LoadBuddy (in imIBuddy aBuddy, in imITag aTag); */
NS_IMETHODIMP purpleAccount::LoadBuddy(imIBuddy *aBuddy,
                                       imITag *aTag,
                                       imIAccountBuddy **aResult)
{
  NS_ENSURE_ARG_POINTER(aBuddy);
  NS_ENSURE_ARG_POINTER(aTag);
  PURPLE_ENSURE_INIT(mAccount);

  nsCString userName;
  nsresult rv = aBuddy->GetUserName(userName);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!userName.IsEmpty(), NS_ERROR_UNEXPECTED);

  PurpleGroup *group = purpleAccountBuddy::GetPurpleGroupForTag(aTag);
  NS_ENSURE_TRUE(group, NS_ERROR_UNEXPECTED);

  PurpleBuddy *buddy =
    purple_buddy_new(mAccount, userName.get(), NULL);

  // Set the server alias on the blist node if the display name of the
  // imIBuddy is different from the userName
  nsCString displayName;
  rv = aBuddy->GetDisplayName(displayName);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!displayName.IsEmpty(), NS_ERROR_UNEXPECTED);
  if (!displayName.Equals(userName))
    buddy->server_alias = purple_utf8_strip_unprintables(displayName.get());

  nsCOMPtr<imIAccountBuddy> accountBuddy =
    new purpleAccountBuddy(buddy, aBuddy);

  int previous_blist_signals_handle_value = blist_signals_handle;
  blist_signals_handle = 0;
  purple_blist_add_buddy(buddy, NULL, group, NULL);
  blist_signals_handle = previous_blist_signals_handle_value;

  NS_ADDREF(*aResult = accountBuddy);
  return NS_OK;
}

PurplePluginProtocolInfo *purpleAccount::GetPrplInfo()
{
  NS_ENSURE_TRUE(mAccount, NULL);

  PurplePlugin *prpl;
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  if (gc)
    prpl = purple_connection_get_prpl(gc);
  else
    prpl = purple_find_prpl(purple_account_get_protocol_id(mAccount));
  NS_ENSURE_TRUE(prpl, NULL);

  return PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
}

/* void requestBuddyInfo (in AUTF8String aBuddyName); */
NS_IMETHODIMP purpleAccount::RequestBuddyInfo(const nsACString & aBuddyName)
{
  PURPLE_ENSURE_INIT(mAccount);
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  if (!gc)
    return NS_OK;

  PurplePluginProtocolInfo *prplInfo = GetPrplInfo();
  if (!prplInfo || !prplInfo->get_info)
    return NS_OK;

  prplInfo->get_info(gc, PromiseFlatCString(aBuddyName).get());
  return NS_OK;
}

/* void requestRoomInfo (in prplIRoomInfoCallback aCallback); */
NS_IMETHODIMP purpleAccount::RequestRoomInfo(prplIRoomInfoCallback *aCallback)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isRoomInfoStale; */
NS_IMETHODIMP purpleAccount::GetIsRoomInfoStale(bool *aIsRoomInfoStale)
{
  *aIsRoomInfoStale = false;
  return NS_OK;
}

/* nsISimpleEnumerator getChatRoomFields (); */
NS_IMETHODIMP purpleAccount::GetChatRoomFields(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mAccount);
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  NS_ENSURE_TRUE(gc, NS_ERROR_FAILURE);

  PurplePluginProtocolInfo *prplInfo = GetPrplInfo();
  NS_ENSURE_TRUE(prplInfo && prplInfo->chat_info, NS_ERROR_FAILURE);

  purpleGListEnumerator *enumerator = new purpleGListEnumerator();
  enumerator->Init(prplInfo->chat_info(gc),
                   purpleTypeToInterface<purpleChatRoomField,
                                         prplIChatRoomField,
                                         proto_chat_entry>);

  NS_ADDREF(*aResult = enumerator);
  return NS_OK;
}

/* readonly attribute boolean canJoinChat; */
NS_IMETHODIMP purpleAccount::GetCanJoinChat(bool *aCanJoinChat)
{
  PurplePluginProtocolInfo *prplInfo = GetPrplInfo();
  NS_ENSURE_TRUE(prplInfo, NS_ERROR_FAILURE);

  *aCanJoinChat =
    prplInfo->join_chat && prplInfo->chat_info && prplInfo->chat_info_defaults;
  return NS_OK;
}

/* prplIChatRoomFieldValues getChatRoomDefaultFieldValues ([optional] in AUTF8String aDefaultChatName); */
NS_IMETHODIMP
purpleAccount::GetChatRoomDefaultFieldValues(const nsACString & aDefaultChatName,
                                             prplIChatRoomFieldValues **aResult)
{
  PURPLE_ENSURE_INIT(mAccount);
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  NS_ENSURE_TRUE(gc, NS_ERROR_FAILURE);

  PurplePluginProtocolInfo *prplInfo = GetPrplInfo();
  NS_ENSURE_TRUE(prplInfo, NS_ERROR_FAILURE);

  PromiseFlatCString defaultChatName(aDefaultChatName);
  const char *chatName = defaultChatName.get();
  if (!*chatName)
    chatName = NULL;
  NS_ENSURE_TRUE(prplInfo->chat_info_defaults, NS_ERROR_UNEXPECTED);
  GHashTable *hashTable = prplInfo->chat_info_defaults(gc, chatName);
  NS_ADDREF(*aResult = new purpleChatRoomFieldValues(hashTable));
  return NS_OK;
}

/* void joinChat (in prplIChatRoomFieldValues aComponents); */
NS_IMETHODIMP purpleAccount::JoinChat(prplIChatRoomFieldValues *aComponents)
{
  NS_ENSURE_TRUE(aComponents, NS_ERROR_INVALID_ARG);

  PURPLE_ENSURE_INIT(mAccount);
  purpleAccountScoper scoper(mId);
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  NS_ENSURE_TRUE(gc, NS_ERROR_FAILURE);

  GHashTable *components =
    static_cast<purpleChatRoomFieldValues *>(aComponents)->GetHashTable();
  NS_ENSURE_TRUE(components, NS_ERROR_FAILURE);

  serv_join_chat(gc, components);
  return NS_OK;
}

/* readonly attribute AUTF8String normalizedName; */
NS_IMETHODIMP purpleAccount::GetNormalizedName(nsACString& aNormalizedName)
{
  NS_ENSURE_TRUE(mAccount, NS_ERROR_NOT_INITIALIZED);

  aNormalizedName = purple_normalize(mAccount,
                                     purple_account_get_username(mAccount));
  return NS_OK;
}

/* AUTF8String normalize(in AUTF8String aName); */
NS_IMETHODIMP purpleAccount::Normalize(const nsACString & aName, nsACString & aNormalizedName)
{
  NS_ENSURE_TRUE(mAccount, NS_ERROR_NOT_INITIALIZED);

  aNormalizedName = purple_normalize(mAccount, PromiseFlatCString(aName).get());
  return NS_OK;
}

/* attribute purpleIProxyInfo proxyInfo; */
NS_IMETHODIMP purpleAccount::GetProxyInfo(purpleIProxyInfo * *aProxyInfo)
{
  PURPLE_ENSURE_INIT(mProxy);

  NS_ADDREF(*aProxyInfo = mProxy);
  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetProxyInfo(purpleIProxyInfo * aProxyInfo)
{
  NS_ENSURE_ARG(aProxyInfo);
  PURPLE_ENSURE_INIT(mAccount);

  mProxy = aProxyInfo;

  // Save the pref
  nsCString key;
  nsresult rv = mProxy->GetKey(key);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(mPrefBranch, NS_ERROR_NOT_INITIALIZED);
  rv = mPrefBranch->SetCharPref(PREF_PROXY, key.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Give it to libpurple;
  PurpleProxyInfo *info;
  rv = mProxy->GetPurpleProxy(&info);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mAccount)
    purple_account_set_proxy_info(mAccount, info);

  return NS_OK;
}

#define PURPLE_IMPL_SET(prettyType, purpleType, PRType, val)            \
  NS_IMETHODIMP purpleAccount::Set##prettyType(const char *aName,       \
                                               PRType aVal)             \
  {                                                                     \
    PURPLE_ENSURE_INIT(mAccount);                                       \
                                                                        \
    /* Give that pref to libpurple */                                   \
    purple_account_set_##purpleType(mAccount, aName, val);              \
    return NS_OK;                                                       \
  }

/* void setBool (in string aName, in boolean aVal); */
PURPLE_IMPL_SET(Bool, bool, bool, aVal)
/* void setInt (in string aName, in long aVal); */
PURPLE_IMPL_SET(Int, int, PRInt32, aVal)
/* void setString (in string aName, in AUTF8String aVal); */
PURPLE_IMPL_SET(String, string, const nsACString &,
                PromiseFlatCString(aVal).get())

#define PURPLE_IMPL_SETPREF(prettyType, prefType, PRType)               \
  nsresult purpleAccount::Set##prettyType##Pref(const char *aName,      \
                                                PRType aValue)          \
  {                                                                     \
    NS_ENSURE_TRUE(mPrefOptBranch, NS_ERROR_NOT_INITIALIZED);           \
                                                                        \
    return mPrefOptBranch->Set##prefType##Pref(aName, aValue);          \
  }

// nsresult purpleAccount::SetBoolPref(const char *aName, bool aValue)
PURPLE_IMPL_SETPREF(Bool, Bool, bool)
// nsresult purpleAccount::SetIntPref(const char *aName, PRInt32 aValue)
PURPLE_IMPL_SETPREF(Int, Int, PRInt32)
// nsresult purpleAccount::SetStringPref(const char *aName, const char *aValue)
PURPLE_IMPL_SETPREF(String, Char, const char *)

nsresult purpleAccount::Connecting(const nsACString & aConnectionStateMessage)
{
  bool connecting = PR_FALSE;
  mImAccount->GetConnecting(&connecting);

  if (!connecting)
    mImAccount->Observe(this, "account-connecting", nullptr);

  if (aConnectionStateMessage.IsEmpty())
    return NS_OK;

  return mImAccount->Observe(this, "account-connect-progress",
                             NS_ConvertUTF8toUTF16(aConnectionStateMessage).get());
}

nsresult purpleAccount::Connected()
{
  return mImAccount->Observe(this, "account-connected", nullptr);
}

/* readonly attribute short connectionErrorReason; */
NS_IMETHODIMP purpleAccount::GetConnectionErrorReason(PRInt16 *aConnectionErrorReason)
{
  *aConnectionErrorReason = mConnectionErrorReason;
  return NS_OK;
}

/* readonly attribute string connectionTarget; */
NS_IMETHODIMP purpleAccount::GetConnectionTarget(nsACString & aConnectionTarget)
{
  aConnectionTarget.Truncate();
  return NS_OK;
}

nsresult purpleAccount::Disconnecting(PRInt16 aConnectionErrorReason,
                                      const nsACString & aConnectionErrorMessage)
{
  // When an account is disconnected because of an error, we get
  // called from the libpurple report_disconnect_reason uiop and from
  // the signing-off signal handler.
  bool disconnecting;
  if (NS_SUCCEEDED(mImAccount->GetDisconnecting(&disconnecting)) &&
      disconnecting)
    return NS_OK;

  /* Disable the account, otherwise libpurple will attempt to reconnect it
     when the status changes (including when idleness changes)

     Warning: the following is very ugly!

     When we disable the account, libpurple disconnect it, which destroys
     the PurpleConnection associated with the account. Then, it crashes in
     set_current_error because the gc field of the account points to
     already freed memory.

     To workaround this, we take the current value of gc, then replace
     it by NULL so that purple_account_set_enabled(false) believes the
     account is already disconnected and doesn't attempt to do it.

     Finally, we can put the correct value of gc back in place.
     purple_connection_disconnect_cb, called with a timer by libpurple,
     will actually call purple_account_disconnect and free the gc later.
   */
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  purple_account_set_connection(mAccount, NULL);
  if (mStatus != PURPLE_STATUS_OFFLINE)
    purple_account_set_enabled(mAccount, UI_ID, FALSE);
  purple_account_set_connection(mAccount, gc);

  mConnectionErrorReason = aConnectionErrorReason;
  return mImAccount->Observe(this, "account-disconnecting",
                             NS_ConvertUTF8toUTF16(aConnectionErrorMessage).get());
}

nsresult purpleAccount::Disconnected()
{
  // Double check that callbacks of the account have been removed.
  purpleSocketWatcher::CancelWatchFromAccountId(mId);
  purpleTimer::CancelTimerFromAccountId(mId);

  return mImAccount->Observe(this, "account-disconnected", nullptr);
}

/* Get flag from PurpleConnection->flags */
#define PURPLE_IMPL_GETFLAG(aName, aFlag)                                 \
  NS_IMETHODIMP purpleAccount::Get##aName(bool *a##aName)                 \
  {                                                                       \
    PURPLE_ENSURE_INIT(mAccount);                                         \
    PURPLE_ENSURE_INIT(mAccount->gc);                                     \
    PurpleConnectionFlags flags = mAccount->gc->flags;                    \
    *a##aName = (flags & PURPLE_CONNECTION_##aFlag) ? PR_TRUE : PR_FALSE; \
    return NS_OK;                                                         \
  }

/* readonly attribute boolean HTMLEnabled; */
PURPLE_IMPL_GETFLAG(HTMLEnabled, HTML)

/* readonly attribute boolean HTMLEscapePlainText; */
NS_IMETHODIMP purpleAccount::GetHTMLEscapePlainText(bool *aHTMLEscapePlainText)
{
  *aHTMLEscapePlainText = PR_TRUE;
  return NS_OK;
}

/* readonly attribute boolean noBackgroundColors; */
PURPLE_IMPL_GETFLAG(NoBackgroundColors, NO_BGCOLOR)

/* readonly attribute boolean autoResponses; */
PURPLE_IMPL_GETFLAG(AutoResponses, AUTO_RESP)

/* readonly attribute boolean singleFormatting; */
PURPLE_IMPL_GETFLAG(SingleFormatting, FORMATTING_WBFO)

/* readonly attribute boolean noFontSizes; */
PURPLE_IMPL_GETFLAG(NoFontSizes, NO_FONTSIZE)

/* readonly attribute boolean noUrlDesc; */
PURPLE_IMPL_GETFLAG(NoUrlDesc, NO_URLDESC)

/* readonly attribute boolean noImages; */
PURPLE_IMPL_GETFLAG(NoImages, NO_IMAGES)

NS_IMETHODIMP purpleAccount::Observe(nsISupports *aSubject,
                                     const char *aTopic,
                                     const PRUnichar *aData)
{
  NS_ENSURE_TRUE(mAccount, NS_ERROR_NOT_INITIALIZED);

  if (!strcmp("user-icon-changed", aTopic)) {
    LOG(("observing user-icon changed on account %s\n",
         purple_account_get_username(mAccount)));
    return ApplyCurrentUserIcon();
  }

  if (!strcmp("user-display-name-changed", aTopic)) {
    if (purple_account_is_connected(mAccount)) {
      purpleAccountScoper scoper(mId);
      purple_account_set_public_alias(mAccount,
                                      aData ? NS_ConvertUTF16toUTF8(aData).get() : NULL,
                                      NULL, NULL);
    }
    return NS_OK;
  }

  if (!strcmp("status-changed", aTopic)) {
    nsresult rv;
    nsCOMPtr<imIUserStatusInfo> usi = do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt16 imStatus = imIStatusInfo::STATUS_UNKNOWN;
    rv = usi->GetStatusType(&imStatus);
    NS_ENSURE_SUCCESS(rv, rv);

#define STATUS_UNSET STATUS_UNKNOWN
#define MAP_PURPLE_STATUS(aStatus)                \
    case imIStatusInfo::STATUS_##aStatus:         \
      mStatus = PURPLE_STATUS_##aStatus;          \
      break

    switch (imStatus) {
      MAP_PURPLE_STATUS(OFFLINE);
      MAP_PURPLE_STATUS(UNAVAILABLE);
      MAP_PURPLE_STATUS(AVAILABLE);
      MAP_PURPLE_STATUS(AWAY);
      MAP_PURPLE_STATUS(UNSET);
      MAP_PURPLE_STATUS(INVISIBLE);

      default:
        return NS_ERROR_UNEXPECTED;
    }
#undef MAP_PURPLE_STATUS

    nsCString message;
    rv = usi->GetStatusText(message);
    NS_ENSURE_SUCCESS(rv, rv);

    const PurpleStatusType *status_type =
      purple_account_get_status_type_with_primitive(mAccount, mStatus);

    if (!status_type && mStatus == PURPLE_STATUS_UNAVAILABLE) {
      status_type =
        purple_account_get_status_type_with_primitive(mAccount,
                                                      PURPLE_STATUS_AWAY);
    }

    if (!message.IsEmpty() && purple_status_type_get_attr(status_type, "message"))
      purple_account_set_status(mAccount, purple_status_type_get_id(status_type),
                                TRUE, "message", message.get(), NULL);
    else
      purple_account_set_status(mAccount, purple_status_type_get_id(status_type),
                                TRUE, NULL);
    return NS_OK;
  }

  return NS_OK;
}

nsresult purpleAccount::ApplyCurrentUserIcon()
{
  // Summary:
  // * If the prpl doesn't support buddy icons, return early.
  // * Get the icon file URL from purpleCoreService.
  // * Read the image into a buffer, and decode it to know it's width,
  //   height and mime type.
  // * if the image we have is acceptable w.r.t prpl constraints
  //    * use the icon and return
  //   else
  //    * iterate other the list of image formats the prpl support:
  //      for each format, attempt to convert the image
  // Get a channel for the icon
  // See if the prpl has constraints
  // Get image info (size, dimensions, type)
  // If should resize
  //   Ask libpurple for the desired new size
  // Get the image data
  // Set the icon
  // Store the handle in the preferences

  NS_ENSURE_TRUE(mAccount, NS_ERROR_NOT_INITIALIZED);

  PurplePluginProtocolInfo *prplInfo = GetPrplInfo();
  NS_ENSURE_TRUE(prplInfo, NS_ERROR_FAILURE);

  if (!prplInfo->set_buddy_icon) {
    // The protocol plugin doesn't support buddy icons, don't bother...
    LOG((" protocol plugin %s doesn't support buddy icons\n",
         purple_account_get_protocol_id(mAccount)));
    return NS_OK;
  }

  nsCOMPtr<imIUserStatusInfo> usi;
  nsresult rv = mImAccount->GetStatusInfo(getter_AddRefs(usi));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFileURL> fileURL;
  rv = usi->GetUserIcon(getter_AddRefs(fileURL));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!fileURL) {
    LOG(("Removing the old icon"));
    SetAccountIcon(EmptyCString());
    return NS_OK;
  }

  // Inspired from nsFaviconService::SetFaviconDataFromDataURL
  // (toolkit/components/places/nsFaviconService.cpp)
  nsCOMPtr<nsIChannel> channel;
  NS_NewChannel(getter_AddRefs(channel), fileURL);
  NS_ENSURE_SUCCESS(rv, rv);

  // Let's say blocking stream is OK for local (small) files...
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t available;
  rv = stream->Available(&available);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(available != 0, NS_ERROR_FAILURE);

  // Read all the data.
  nsCString buffer;
  buffer.SetLength(available);
  PRUint32 numRead;
  rv = stream->Read(buffer.BeginWriting(), available, &numRead);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(numRead == available, NS_ERROR_FAILURE);

  nsCString mimeType;
  rv = channel->GetContentType(mimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<imgITools> imgtool =
    do_CreateInstance("@mozilla.org/image/tools;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringInputStream> rawStream =
    do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = rawStream->ShareData(buffer.get(), available);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<imgIContainer> image;
  rv = imgtool->DecodeImageData(rawStream, mimeType, getter_AddRefs(image));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 width = 0;
  PRInt32 height = 0;
  image->GetWidth(&width);
  image->GetHeight(&height);
  NS_ENSURE_TRUE(width && height, NS_ERROR_FAILURE);

  LOG(("read image of type %s, height: %i, width: %i\n",
       mimeType.get(), height, width));

  // We have all the info we need about the input image. Now see what
  // the prpl can accept.
  NS_ENSURE_TRUE(prplInfo->icon_spec.format, NS_ERROR_UNEXPECTED);

  nsCString format(prplInfo->icon_spec.format);
  char *tmp = format.BeginWriting();
  bool found = false;
  for (const char *token = NS_strtok(",", &tmp);
       token;
       token = NS_strtok(",", &tmp)) {
    // Possible types in libpurple: png,gif,jpeg,bmp,ico
    if ((!strcmp(token, "png") && mimeType.Equals("image/png")) ||
        (!strcmp(token, "gif") && mimeType.Equals("image/gif")) ||
        (!strcmp(token, "jpeg") && mimeType.Equals("image/jpeg")) ||
        (!strcmp(token, "bmp") && mimeType.Equals("image/bmp")) ||
        (!strcmp(token, "ico") && mimeType.Equals("image/x-icon"))) {
      found = PR_TRUE;
      LOG(("  current file is of acceptable type: %s", token));
      break;
    }
  }

  if (found &&
      (!prplInfo->icon_spec.max_filesize ||
       available <= prplInfo->icon_spec.max_filesize) &&              /* The file size is acceptable */
      (!(prplInfo->icon_spec.scale_rules & PURPLE_ICON_SCALE_SEND) || /* The prpl doesn't want us to scale before it sends */
       (prplInfo->icon_spec.min_width <= width &&
        prplInfo->icon_spec.max_width >= width &&
        prplInfo->icon_spec.min_height <= height &&
        prplInfo->icon_spec.max_height >= height))) {                 /* The icon is of the correct size */
    SetAccountIcon(buffer);
    return NS_OK;
  }

  // We should resize and/or convert.
  PRInt32 newWidth = width;
  PRInt32 newHeight = height;

  if ((prplInfo->icon_spec.scale_rules & PURPLE_ICON_SCALE_SEND) &&
      (width < prplInfo->icon_spec.min_width ||
       width > prplInfo->icon_spec.max_width ||
       height < prplInfo->icon_spec.min_height ||
       height > prplInfo->icon_spec.max_height)) {
    purple_buddy_icon_get_scale_size(&prplInfo->icon_spec,
                                     &newWidth, &newHeight);
  }

  format = prplInfo->icon_spec.format;
  tmp = format.BeginWriting();
  LOG(("Going to resize (w: %i, h: %i) or convert, possible formats: %s",
       newWidth, newHeight, tmp));
  for (const char *ext = NS_strtok(",", &tmp);
       ext;
       ext = NS_strtok(",", &tmp)) {
    // Get the new mime type from the file extension.
    const char *newMimeType;
    if (!strcmp(ext, "png"))
      newMimeType = "image/png";
    else if (!strcmp(ext, "gif"))
      newMimeType = "image/gif";
    else if (!strcmp(ext, "jpeg"))
      newMimeType = "image/jpeg";
    else if (!strcmp(ext, "bmp"))
      newMimeType = "image/bmp";
    else if (!strcmp(ext, "ico"))
      newMimeType = "image/x-icon";
    else
      continue;

    // scale and recompress
    nsCOMPtr<nsIInputStream> iconStream;
    rv = imgtool->EncodeScaledImage(image, nsDependentCString(newMimeType),
                                    newWidth, newHeight, EmptyString(),
                                    getter_AddRefs(iconStream));
    if (NS_FAILED(rv))
      continue;

    // Read the stream into a new buffer.
    nsCString newBuffer;
    uint64_t avail;
    iconStream->Available(&avail);
    newBuffer.SetLength(avail);
    PRUint32 length;
    iconStream->Read(newBuffer.BeginWriting(), avail, &length);
    NS_ENSURE_TRUE(length == avail && length > 0, NS_ERROR_FAILURE);

    LOG((" converted into %s, new size: %i", newMimeType, newBuffer.Length()));
    if ((prplInfo->icon_spec.max_filesize != 0) &&
        (newBuffer.Length() > prplInfo->icon_spec.max_filesize))
      continue;

    SetAccountIcon(newBuffer);
    return NS_OK;
  }

  // Report the error.
  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService("@mozilla.org/consoleservice;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString msg(NS_LITERAL_STRING("Could not convert the icon to a suitable format for "));
  msg.Append(NS_ConvertUTF8toUTF16(purple_account_get_username(mAccount)));
  return consoleService->LogStringMessage(msg.get());
}

void purpleAccount::SetAccountIcon(const nsCString& aBuffer)
{
  LOG(("Setting icon for account %s", purple_account_get_username(mAccount)));
  purpleAccountScoper scoper(mId);
  // libpurple will g_free so we need to g_memdup the buffer.
  guchar *data =
    static_cast<guchar *>(g_memdup(aBuffer.get(), aBuffer.Length()));
  purple_buddy_icons_set_account_icon(mAccount, data, aBuffer.Length());
}
