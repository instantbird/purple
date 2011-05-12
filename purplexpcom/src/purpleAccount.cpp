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

#include "purpleAccount.h"
#include "purpleAccountBuddy.h"
#include "purpleCoreService.h"
#include "purpleConversation.h"
#include "purpleGListEnumerator.h"

#include <imgITools.h>
#include <imgIContainer.h>
#include <nsIConsoleService.h>
#include <nsIObserverService.h>
#include <nsIPrefService.h>
#include <nsIProgrammingLanguage.h>
#include <nsIClassInfoImpl.h>
#include <nsComponentManagerUtils.h>
#include <nsNetUtil.h>
#include <nsServiceManagerUtils.h>
#include <nsStringStream.h>
#include <prprf.h>

#pragma GCC visibility push(default)
#include <libpurple/savedstatuses.h>
#pragma GCC visibility pop

#define PREF_PREFIX             "messenger.account."
#define PREF_OPTIONS            "options."
#define PREF_PRPL               "prpl"
#define PREF_NAME               "name"
#define PREF_ALIAS              "alias"
#define PREF_PROXY              "proxy"
#define PREF_AUTOLOGIN          "autoLogin"
#define PREF_FIRST_CONNECTION_STATE "firstConnectionState"
#define PREF_PASSWORD           "password"
#define PREF_REMEMBER_PASSWORD  "rememberPassword"

#define INIT_CONDITION          (mProtocol && (!mHasValidProtocol || mAccount))

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleAccount:5
//
static PRLogModuleInfo *gPurpleAccountLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleAccountLog, PR_LOG_DEBUG, args)


class purpleChatRoomField : public purpleIChatRoomField,
                            public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PURPLEICHATROOMFIELD

  purpleChatRoomField() : mType(-1) {}
  void Init(const proto_chat_entry *aChatRoomField)
  {
    mType =
      aChatRoomField->is_int ? (PRInt32) purpleIChatRoomField::TYPE_INT :
      aChatRoomField->secret ? (PRInt32) purpleIChatRoomField::TYPE_PASSWORD :
                               (PRInt32) purpleIChatRoomField::TYPE_TEXT;
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
  PRPackedBool mRequired;
};

// D36DC0D5-438F-4C65-865B-EDE5DB7906D5
#define PURPLE_CHAT_ROOM_FIELD_CID \
  { 0xD36DC0D5, 0x438F, 0x4C65,                               \
    { 0x86, 0x5B, 0xED, 0xE5, 0xDB, 0x79, 0x06, 0xD5 }        \
  }

NS_IMPL_CLASSINFO(purpleChatRoomField, NULL, 0, PURPLE_CHAT_ROOM_FIELD_CID)
NS_IMPL_THREADSAFE_CI(purpleChatRoomField)
NS_IMPL_ISUPPORTS1_CI(purpleChatRoomField, purpleIChatRoomField)

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
PURPLE_IMPL_GETFIELDVALUE(PRBool *, Required, *)

/* readonly attribute short type; */
PURPLE_IMPL_GETFIELDVALUE(PRInt16 *, Type, *)

/* readonly attribute long min; */
PURPLE_IMPL_GETFIELDVALUE(PRInt32 *, Min, *)

/* readonly attribute long max; */
PURPLE_IMPL_GETFIELDVALUE(PRInt32 *, Max, *)

#define MAX_KEYS 8

class purpleChatRoomFieldValues : public purpleIChatRoomFieldValues,
                                  public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PURPLEICHATROOMFIELDVALUES

  purpleChatRoomFieldValues(GHashTable *aHashTable) :
    mHashTable(aHashTable),
    mKeyCount(0) {
  }

private:
  ~purpleChatRoomFieldValues() {
    if (NS_LIKELY(mHashTable))
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
NS_IMPL_ISUPPORTS1_CI(purpleChatRoomFieldValues, purpleIChatRoomFieldValues)

/* AUTF8String getValue (in AUTF8String aIdentifier); */
NS_IMETHODIMP
purpleChatRoomFieldValues::GetValue(const nsACString & aIdentifier,
                                    nsACString & aResult)
{
  if (NS_UNLIKELY(!mHashTable)) {
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
  if (NS_UNLIKELY(!mHashTable)) {
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

/* [noscript] readonly attribute GHashTablePtr hashTable; */
NS_IMETHODIMP
purpleChatRoomFieldValues::GetHashTable(GHashTable * *aHashTable)
{
  *aHashTable = mHashTable;
  return NS_OK;
}

NS_IMPL_CLASSINFO(purpleAccount, NULL, 0, PURPLE_ACCOUNT_CID)
NS_IMPL_ISUPPORTS3_CI(purpleAccount, purpleIAccount,
                      purpleIAccountBase, nsIObserver)

purpleAccount::purpleAccount(PRBool aIsValidProtocol)
  : mAccount(NULL)
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleAccountLog)
    gPurpleAccountLog = PR_NewLogModule("purpleAccount");
#endif
  LOG(("Creating purpleAccount @%x\n", this));
  mConcreteAccount = this;
  mHasValidProtocol = aIsValidProtocol;
}

purpleAccount::~purpleAccount()
{
  /* destructor code */
  LOG(("Destructing purpleAccount @%x\n", this));
  if (mAccount)
    UnInit();
}

/* void UnInit (); */
NS_IMETHODIMP purpleAccount::UnInit()
{
  NS_PRECONDITION(INIT_CONDITION,
                  "Uninitializing uninitialized purpleAccount\n");
  NS_ENSURE_TRUE(INIT_CONDITION, NS_ERROR_NOT_INITIALIZED);

#ifdef DEBUG
  nsCString protoId;
  nsresult rv = NS_ERROR_FAILURE;
  if (mProtocol)
    rv = mProtocol->GetId(protoId);
  LOG(("uninitializing purpleAccount %s (%s)\n",
       mAccount ? purple_account_get_username(mAccount) : "(mAccount = NULL)",
       NS_SUCCEEDED(rv) ? protoId.get() : "mProtocol = NULL"));
#endif

  if (mHasValidProtocol) {
    nsCOMPtr<nsIObserverService> os =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (NS_LIKELY(os)) {
      os->RemoveObserver(this, "user-icon-changed");
      os->RemoveObserver(this, "user-display-name-changed");
    }
  }

  if (mAccount) {
    // disconnect before removing the ui_data pointer so that
    // account-disconnected signals can be sent
    purple_account_set_enabled(mAccount, UI_ID, FALSE);
    mAccount->ui_data = NULL;
    // This will call purple_proxy_info_destroy if there was a proxy
    purple_account_set_proxy_info(mAccount, NULL);
    purple_accounts_delete(mAccount);
    mAccount = NULL;
  }
  purpleAccountBase::UnInit();

  return NS_OK;
}

#define PURPLE_IMPL_GETPREF(prettyType, purpleType, PRType)             \
  inline void purpleAccount::Get##prettyType##Pref(const char *aName)   \
  {                                                                     \
    PRType val;                                                         \
    nsresult rv = mPrefOptBranch->Get##prettyType##Pref(aName, &val);   \
    if (NS_SUCCEEDED(rv)) {                                             \
      purple_account_set_##purpleType(mAccount, aName, val);            \
    }                                                                   \
  }

inline void purpleAccount::GetCharPref(const char *aName)
{
  nsCString val;
  nsresult rv = mPrefOptBranch->GetCharPref(aName, getter_Copies(val));
  if (NS_SUCCEEDED(rv)) {
    purple_account_set_string(mAccount, aName, val.get());
  }
}

PURPLE_IMPL_GETPREF(Bool, bool, PRBool)
PURPLE_IMPL_GETPREF(Int, int, PRInt32)

/* void init (in AUTF8String aKey, in AUTF8String aName); */
nsresult purpleAccount::Init(const nsACString & aKey,
                             const nsACString & aName,
                             purpleIProtocol *aPrpl)
{
  nsresult rv = purpleAccountBase::Init(aKey, aName, aPrpl);

  if (mHasValidProtocol) {
    NS_ENSURE_TRUE(!mName.IsEmpty() && mProtocol, NS_ERROR_NOT_INITIALIZED);

    nsCString protoId;
    nsresult rv = mProtocol->GetId(protoId);
    NS_ENSURE_SUCCESS(rv, rv);

    mAccount = purple_account_new(mName.get(), protoId.get());
    NS_ENSURE_TRUE(mAccount, NS_ERROR_FAILURE);

    mAccount->ui_data = this;
    purple_accounts_add(mAccount);
  }

  /* Load the alias if any */
  nsCString alias;
  rv = mPrefBranch->GetCharPref(PREF_ALIAS, getter_Copies(alias));
  if (NS_SUCCEEDED(rv) && !alias.IsEmpty() && mAccount) {
    purple_account_set_alias(mAccount, alias.get());
    LOG(("alias = %s\n", alias.get()));
  }

  /* Load proxy settings */
  nsCString proxyKey;
  rv = mPrefBranch->GetCharPref(PREF_PROXY, getter_Copies(proxyKey));
  /* Init mProxy */
  if (NS_SUCCEEDED(rv) &&
      StringBeginsWith(proxyKey, NS_LITERAL_CSTRING(PROXY_KEY))) {
    nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
    nsCOMPtr<nsISimpleEnumerator> proxies;
    rv = pcs->GetProxies(getter_AddRefs(proxies));
    PRBool hasNext;
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

  if (!mHasValidProtocol)
    return NS_OK;

  /* Give the information to libpurple */
  PurpleProxyInfo *info;
  rv = mProxy->GetPurpleProxy(&info);
  NS_ENSURE_SUCCESS(rv, rv);
  if (info)
    purple_account_set_proxy_info(mAccount, info);

  /* Load the password if it was saved */
  nsCString pass;
  rv = mPrefBranch->GetCharPref(PREF_PASSWORD, getter_Copies(pass));
  if (NS_SUCCEEDED(rv)) {
    purple_account_set_password(mAccount, pass.get());
    LOG(("password Set\n"));
  }

  if (pass.IsEmpty())
    SetMissingPasswordIfRequired();

  /* Load the rememberPassword pref */
  PRBool rememberPass;
  rv = mPrefBranch->GetBoolPref(PREF_REMEMBER_PASSWORD, &rememberPass);
  if (NS_SUCCEEDED(rv)) {
    purple_account_set_remember_password(mAccount, rememberPass);
    LOG(("rememberPassword = %s\n", rememberPass ? "true" : "false"));
  }

  //get spec options
  PRUint32 count;
  char **prefs;
  rv = mPrefOptBranch->GetChildList("", &count, &prefs);
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("Number of specific pref: %i\n", count));
  while (count--) {
    PRInt32 type;
    rv = mPrefOptBranch->GetPrefType(prefs[count], &type);
    if (NS_FAILED(rv)) {
      continue;
    }
    switch (type) {
    case nsIPrefBranch::PREF_INT:
      GetIntPref(prefs[count]);
      break;
    case nsIPrefBranch::PREF_BOOL:
      GetBoolPref(prefs[count]);
      break;
    case nsIPrefBranch::PREF_STRING:
      GetCharPref(prefs[count]);
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

  // Observe user icon changes.
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  os->AddObserver(this, "user-display-name-changed", PR_FALSE);
  return os->AddObserver(this, "user-icon-changed", PR_FALSE);
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

  /* enabling an account in libpurple connects it automatically */
  purple_account_set_enabled(mAccount, UI_ID, TRUE);
  // purple_account_connect(mAccount);

  return NS_OK;
}

/* void disconnect (); */
NS_IMETHODIMP purpleAccount::Disconnect()
{
  PURPLE_ENSURE_INIT(mAccount);
  LOG(("Attempting to disconnect %s\n", mAccount->username));

  purple_account_set_enabled(mAccount, UI_ID, FALSE);

  return NS_OK;
}

/* purpleIConversation createConversation (in AUTF8String aName); */
NS_IMETHODIMP purpleAccount::CreateConversation(const nsACString& aName,
                                                purpleIConversation **aResult)
{
  NS_ENSURE_TRUE(!aName.IsEmpty(), NS_ERROR_INVALID_ARG);

  PurpleConversation *conv;
  conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, this->mAccount,
                                 PromiseFlatCString(aName).get());
  NS_ENSURE_TRUE(conv, NS_ERROR_FAILURE);

  purpleIConversation *result = purpleConversation::fromPurpleConv(conv);
  NS_ENSURE_TRUE(result, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*aResult = result);

  return NS_OK;
}

/* void addBuddy (in imITag aTag, in AUTF8String aName); */
NS_IMETHODIMP purpleAccount::AddBuddy(imITag *aTag,
                                      const nsACString& aName)
{
  PURPLE_ENSURE_INIT(mProtocol && mAccount);
  NS_ENSURE_ARG_POINTER(aTag);
  NS_ENSURE_ARG(!aName.IsEmpty());

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
  PURPLE_ENSURE_INIT(mProtocol && mAccount);

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
  purple_blist_add_buddy(buddy, NULL, group, NULL);

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

/* nsISimpleEnumerator getChatRoomFields (); */
NS_IMETHODIMP purpleAccount::GetChatRoomFields(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mProtocol && mAccount);
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  NS_ENSURE_TRUE(gc, NS_ERROR_FAILURE);

  PurplePluginProtocolInfo *prplInfo = GetPrplInfo();
  NS_ENSURE_TRUE(prplInfo && prplInfo->chat_info, NS_ERROR_FAILURE);

  purpleGListEnumerator *enumerator = new purpleGListEnumerator();
  enumerator->Init(prplInfo->chat_info(gc),
                   purpleTypeToInterface<purpleChatRoomField,
                                         purpleIChatRoomField,
                                         proto_chat_entry>);

  NS_ADDREF(*aResult = enumerator);
  return NS_OK;
}

/* readonly attribute boolean canJoinChat; */
NS_IMETHODIMP purpleAccount::GetCanJoinChat(PRBool *aCanJoinChat)
{
  PURPLE_ENSURE_INIT(mProtocol);

  PurplePluginProtocolInfo *prplInfo = GetPrplInfo();
  NS_ENSURE_TRUE(prplInfo, NS_ERROR_FAILURE);

  *aCanJoinChat =
    prplInfo->join_chat && prplInfo->chat_info && prplInfo->chat_info_defaults;
  return NS_OK;
}

/* purpleIChatRoomFieldValues getChatRoomDefaultFieldValues ([optional] in AUTF8String aDefaultChatName); */
NS_IMETHODIMP
purpleAccount::GetChatRoomDefaultFieldValues(const nsACString & aDefaultChatName,
                                             purpleIChatRoomFieldValues **aResult)
{
  PURPLE_ENSURE_INIT(mProtocol && mAccount);
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

/* void joinChat (in purpleIChatRoomFieldValues aComponents); */
NS_IMETHODIMP purpleAccount::JoinChat(purpleIChatRoomFieldValues *aComponents)
{
  NS_ENSURE_TRUE(aComponents, NS_ERROR_INVALID_ARG);

  PURPLE_ENSURE_INIT(mAccount);
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  NS_ENSURE_TRUE(gc, NS_ERROR_FAILURE);

  GHashTable *components;
  nsresult rv = aComponents->GetHashTable(&components);
  NS_ENSURE_SUCCESS(rv, rv);

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

/* attribute AUTF8String password; */
NS_IMETHODIMP purpleAccount::GetPassword(nsACString& aPassword)
{
  PURPLE_ENSURE_INIT(INIT_CONDITION);

  if (NS_UNLIKELY(!mAccount)) {
    // Will get the password from the preferences
    return purpleAccountBase::GetPassword(aPassword);
  }

  aPassword = purple_account_get_password(mAccount);
  return NS_OK;
}

NS_IMETHODIMP purpleAccount::SetPassword(const nsACString& aPassword)
{
  PURPLE_ENSURE_INIT(INIT_CONDITION);

  if (mAccount)
    purple_account_set_password(mAccount, PromiseFlatCString(aPassword).get());

  return purpleAccountBase::SetPassword(aPassword);
}

/* attribute boolean rememberPassword; */
NS_IMETHODIMP purpleAccount::GetRememberPassword(PRBool *aRememberPassword)
{
  PURPLE_ENSURE_INIT(mAccount);

  *aRememberPassword = purple_account_get_remember_password(mAccount);
  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetRememberPassword(PRBool aRememberPassword)
{
  PURPLE_ENSURE_INIT(mAccount);

  purple_account_set_remember_password(mAccount, aRememberPassword);
  SetBoolPref(PREF_REMEMBER_PASSWORD, aRememberPassword);

  return NS_OK;
}

/* attribute string alias; */
NS_IMETHODIMP purpleAccount::GetAlias(nsACString& aAlias)
{
  PURPLE_ENSURE_INIT(INIT_CONDITION);

  if (NS_UNLIKELY(!mAccount)) {
    // Get the alias from the preferences
    return purpleAccountBase::GetAlias(aAlias);
  }

  aAlias = purple_account_get_alias(mAccount);
  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetAlias(const nsACString& aAlias)
{
  PURPLE_ENSURE_INIT(INIT_CONDITION);

  if (mAccount) {
    if (!aAlias.IsEmpty())
      purple_account_set_alias(mAccount, PromiseFlatCString(aAlias).get());
    else
      purple_account_set_alias(mAccount, NULL);
  }

  return purpleAccountBase::SetAlias(aAlias);
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
  PURPLE_ENSURE_INIT(INIT_CONDITION);

  mProxy = aProxyInfo;

  // Save the pref
  nsCString key;
  nsresult rv = mProxy->GetKey(key);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetStringPref(PREF_PROXY, key.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Give it to libpurple;
  PurpleProxyInfo *info;
  rv = mProxy->GetPurpleProxy(&info);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mAccount)
    purple_account_set_proxy_info(mAccount, info);

  return NS_OK;
}

#define PURPLE_IMPL_SET(prettyType, purpleType, PRType, prefType)       \
  NS_IMETHODIMP purpleAccount::Set##prettyType(const char *aName,       \
                                               PRType aVal)             \
  {                                                                     \
    PURPLE_ENSURE_INIT(mAccount);                                       \
                                                                        \
    /* Give that pref to libpurple */                                   \
    purple_account_set_##purpleType(mAccount, aName, aVal);             \
                                                                        \
    /* Save the pref in our preference system */                        \
    nsresult rv = GetPrefBranch();                                      \
    NS_ENSURE_SUCCESS(rv, rv);                                          \
                                                                        \
    return purpleAccountBase::Set##prettyType(aName, aVal);             \
  }

/* void setBool (in string aName, in boolean aVal); */
PURPLE_IMPL_SET(Bool, bool, PRBool, Bool)
/* void setInt (in string aName, in long aVal); */
PURPLE_IMPL_SET(Int, int, PRInt32, Int)
/* void setString (in string aName, in string aVal); */
PURPLE_IMPL_SET(String, string, const char *, Char)

#define PURPLE_IMPL_SETPREF(prettyType, prefType, PRType)               \
  nsresult purpleAccount::Set##prettyType##Pref(const char *aName,      \
                                                PRType aValue)          \
  {                                                                     \
    nsresult rv = GetPrefBranch();                                      \
    NS_ENSURE_SUCCESS(rv, rv);                                          \
                                                                        \
    rv = mPrefBranch->Set##prefType##Pref(aName, aValue);               \
    NS_ENSURE_SUCCESS(rv, rv);                                          \
                                                                        \
    initSavePrefsTimer();                                               \
    return NS_OK;                                                       \
  }

// nsresult purpleAccount::SetBoolPref(const char *aName, PRBool aValue)
PURPLE_IMPL_SETPREF(Bool, Bool, PRBool)
// nsresult purpleAccount::SetIntPref(const char *aName, PRInt32 aValue)
PURPLE_IMPL_SETPREF(Int, Int, PRInt32)
// nsresult purpleAccount::SetStringPref(const char *aName, const char *aValue)
PURPLE_IMPL_SETPREF(String, Char, const char *)

/* void disconnecting ([optional] in short aConnectionErrorReason,
                      [optional] in AUTF8String aConnectionErrorMessage); */
NS_IMETHODIMP purpleAccount::Disconnecting(PRInt16 aConnectionErrorReason,
                                           const nsACString & aConnectionErrorMessage)
{
  // When an account is disconnected because of an error, we get
  // called from the libpurple report_disconnect_reason uiop and from
  // the signing-off signal handler.
  if (mConnectionState == STATE_DISCONNECTING)
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
  if (purple_savedstatus_get_type(purple_savedstatus_get_current()) != PURPLE_STATUS_OFFLINE)
    purple_account_set_enabled(mAccount, UI_ID, FALSE);
  purple_account_set_connection(mAccount, gc);

  return purpleAccountBase::Disconnecting(aConnectionErrorReason,
                                          aConnectionErrorMessage);
}

/* Get flag from PurpleConnection->flags */
#define PURPLE_IMPL_GETFLAG(aName, aFlag)                                 \
  NS_IMETHODIMP purpleAccount::Get##aName(PRBool *a##aName)               \
  {                                                                       \
    PURPLE_ENSURE_INIT(mAccount);                                         \
    PURPLE_ENSURE_INIT(mAccount->gc);                                     \
    PurpleConnectionFlags flags = mAccount->gc->flags;                    \
    *a##aName = (flags & PURPLE_CONNECTION_##aFlag) ? PR_TRUE : PR_FALSE; \
    return NS_OK;                                                         \
  }

/* readonly attribute boolean HTMLEnabled; */
PURPLE_IMPL_GETFLAG(HTMLEnabled, HTML)

/* readonly attribute boolean noBackgroundColors; */
PURPLE_IMPL_GETFLAG(NoBackgroundColors, NO_BGCOLOR)

/* readonly attribute boolean autoResponses; */
PURPLE_IMPL_GETFLAG(AutoResponses, AUTO_RESP)

/* readonly attribute boolean singleFormatting; */
PURPLE_IMPL_GETFLAG(SingleFormatting, FORMATTING_WBFO)

/* readonly attribute boolean noNewlines; */
PURPLE_IMPL_GETFLAG(NoNewlines, NO_NEWLINES)

/* readonly attribute boolean noFontSizes; */
PURPLE_IMPL_GETFLAG(NoFontSizes, NO_FONTSIZE)

/* readonly attribute boolean noUrlDesc; */
PURPLE_IMPL_GETFLAG(NoUrlDesc, NO_URLDESC)

/* readonly attribute boolean noImages; */
PURPLE_IMPL_GETFLAG(NoImages, NO_IMAGES)

/* nsIObserver implementation */
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
      purple_account_set_public_alias(mAccount,
                                      aData ? NS_ConvertUTF16toUTF8(aData).get() : NULL,
                                      NULL, NULL);
    }
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
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

  nsresult rv;
  nsCOMPtr<purpleICoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFileURL> fileURL;
  rv = pcs->GetUserIcon(getter_AddRefs(fileURL));
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

  PRUint32 available;
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
  PRBool found = PR_FALSE;
  for (const char *token = NS_strtok(",", &tmp);
       token;
       token = NS_strtok(",", &tmp)) {
    // Possible types in libpurple: png,gif,jpeg,bmp,ico
    if (!strcmp(token, "png") && mimeType.Equals("image/png") ||
        !strcmp(token, "gif") && mimeType.Equals("image/gif") ||
        !strcmp(token, "jpeg") && mimeType.Equals("image/jpeg") ||
        !strcmp(token, "bmp") && mimeType.Equals("image/bmp") ||
        !strcmp(token, "ico") && mimeType.Equals("image/x-icon")) {
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
                                    newWidth, newHeight,
                                    getter_AddRefs(iconStream));
    if (NS_FAILED(rv))
      continue;

    // Read the stream into a new buffer.
    nsCString newBuffer;
    PRUint32 avail;
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
  // libpurple will g_free so we need to g_memdup the buffer.
  guchar *data =
    static_cast<guchar *>(g_memdup(aBuffer.get(), aBuffer.Length()));
  purple_buddy_icons_set_account_icon(mAccount, data, aBuffer.Length());
}
