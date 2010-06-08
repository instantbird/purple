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
#include "purpleClassInfo.h"
#include "purpleCoreService.h"
#include "purpleGListEnumerator.h"
#include "purpleStorage.h"
#include <nsIObserverService.h>
#include <nsIPrefService.h>
#include <nsIClassInfoImpl.h>
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsNetUtil.h>
#include <prprf.h>

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

PURPLE_IMPL_ISUPPORTS1_CI(purpleChatRoomField, purpleIChatRoomField)

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

PURPLE_IMPL_ISUPPORTS1_CI(purpleChatRoomFieldValues, purpleIChatRoomFieldValues)

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

purpleAccount::purpleAccount()
  : mAccount(NULL)
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleAccountLog)
    gPurpleAccountLog = PR_NewLogModule("purpleAccount");
#endif
  LOG(("Creating purpleAccount @%x\n", this));
  mConcreteAccount = this;
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

nsresult purpleAccount::Create(const nsACString & aName,
                               purpleIProtocol *aPrpl,
                               const nsACString & aKey)
{
  NS_ENSURE_ARG_POINTER(aPrpl);
  NS_ENSURE_TRUE(!aName.IsEmpty(), NS_ERROR_UNEXPECTED);

  nsCString prpl;
  nsresult rv = aPrpl->GetId(prpl);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool alreadyExists = PR_FALSE;
  rv = aPrpl->AccountExists(aName, &alreadyExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (alreadyExists) {
    // adding a duplicate account would confuse libpurple
    // XXX: Should this test be in purpleProtocol::CreatePurpleAccount?
    NS_WARNING("Attempted to create a duplicate account!");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  rv = aPrpl->CreatePurpleAccount(aName, this, &mAccount);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetId(aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  mName = aName;
  mProtocol = aPrpl;
  mHasValidProtocol = PR_TRUE;

  rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  PromiseFlatCString flatName(aName);
  mPrefBranch->SetCharPref(PREF_NAME, flatName.get());
  mPrefBranch->SetCharPref(PREF_PRPL, prpl.get());

  rv = storeAccount(mId, flatName, prpl);
  if (NS_FAILED(rv)) {
    mPrefBranch->DeleteBranch("");
    return rv;
  }

  mProxy = do_CreateInstance(PURPLE_PROXY_INFO_CONTRACTID);
  NS_ENSURE_TRUE(mProxy, NS_ERROR_OUT_OF_MEMORY);

  mProxy->SetType(purpleIProxyInfo::useGlobal);

  SetFirstConnectionState(FIRST_CONNECTION_SET);
  SetMissingPasswordIfRequired();
  return NS_OK;
}

/* void load (in AUTF8String aKey); */
nsresult purpleAccount::Load(const nsACString & aKey,
                             purpleIProtocol *aPrpl,
                             PRBool aIsValidProtocol)
{
  NS_ENSURE_TRUE(aPrpl, NS_ERROR_INVALID_ARG);

  nsresult rv = SetId(aKey);
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("loading account : %s\n", mKey.get()));

  rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Get the name */
  rv = mPrefBranch->GetCharPref(PREF_NAME, getter_Copies(mName));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!mName.IsEmpty(), NS_ERROR_UNEXPECTED);
  LOG(("name = %s\n", mName.get()));

  /* Get the prpl */
  nsCString prpl;
  rv = mPrefBranch->GetCharPref(PREF_PRPL, getter_Copies(prpl));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!prpl.IsEmpty(), NS_ERROR_UNEXPECTED);
  LOG(("prpl = %s\n", prpl.get()));

  rv = ensureStored(mName, prpl);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Init the mProtocol member */
  mProtocol = aPrpl;
  mHasValidProtocol = aIsValidProtocol;
  if (aIsValidProtocol) {
    rv = mProtocol->CreatePurpleAccount(mName, this, &mAccount);
    NS_ENSURE_SUCCESS(rv, rv);

    /* We previously crashed while connecting an account individually. */
    PRInt16 currentState;
    if (NS_SUCCEEDED(GetFirstConnectionState(&currentState)) &&
        (currentState == FIRST_CONNECTION_PENDING ||
         currentState == FIRST_CONNECTION_CRASHED)) {
      SetFirstConnectionState(FIRST_CONNECTION_CRASHED);
      mConnectionErrorReason = (PurpleConnectionError) ERROR_CRASHED;
    }
  }
  else {
    mConnectionErrorReason = (PurpleConnectionError) ERROR_UNKNOWN_PRPL;
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

  purpleIConversation *result = (purpleIConversation *)conv->ui_data;
  NS_ENSURE_TRUE(result, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*aResult = result);

  return NS_OK;
}

/* void addBuddy (in purpleITag aTag, in AUTF8String aName); */
NS_IMETHODIMP purpleAccount::AddBuddy(purpleITag *aTag,
                                      const nsACString& aName)
{
  PURPLE_ENSURE_INIT(mProtocol && mAccount);
  NS_ENSURE_ARG_POINTER(aTag);
  NS_ENSURE_ARG(!aName.IsEmpty());

  purpleTag *tag = static_cast<purpleTag *>(aTag);
  PurpleBuddy *buddy = purple_buddy_new(mAccount, PromiseFlatCString(aName).get(), NULL);
  tag->addBuddy(buddy);
  purple_account_add_buddy(mAccount, buddy);
  return NS_OK;
}

/* void LoadBuddy (in purpleIBuddy aBuddy, in AUTF8String aName,
                   in AUTF8String aAlias, in AUTF8String aServerAlias,
                   in purpleITag aTag); */
NS_IMETHODIMP purpleAccount::LoadBuddy(purpleIBuddy *aBuddy,
                                       const nsACString & aName,
                                       const nsACString & aAlias,
                                       const nsACString & aServerAlias,
                                       purpleITag *aTag)
{
  PURPLE_ENSURE_INIT(mProtocol && mAccount);

  purpleTag *tag = static_cast<purpleTag *>(aTag);
  PurpleBuddy *buddy =
    purple_buddy_new(mAccount, PromiseFlatCString(aName).get(),
                     aAlias.IsEmpty() ? NULL : PromiseFlatCString(aAlias).get());

  // Ignore server aliases that are identical to the name.
  // (We used to mistakenly store the name in the srv_alias column
  // when there was no server alias.)
  if (!aServerAlias.IsEmpty() && !aServerAlias.Equals(aName)) {
    buddy->server_alias =
      purple_utf8_strip_unprintables(PromiseFlatCString(aServerAlias).get());
  }
  tag->addBuddy(buddy);

  static_cast<purpleBuddy *>(aBuddy)->AddAccount(this, buddy, tag);
  return NS_OK;
}

/* nsISimpleEnumerator getChatRoomFields (); */
NS_IMETHODIMP purpleAccount::GetChatRoomFields(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mProtocol && mAccount);
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  NS_ENSURE_TRUE(gc, NS_ERROR_FAILURE);

  PurplePluginProtocolInfo *prplInfo;
  nsresult rv = mProtocol->GetInfo(&prplInfo);
  NS_ENSURE_SUCCESS(rv, rv);

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

  PurplePluginProtocolInfo *prplInfo;
  nsresult rv = mProtocol->GetInfo(&prplInfo);
  NS_ENSURE_SUCCESS(rv, rv);

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

  PurplePluginProtocolInfo *prplInfo;
  nsresult rv = mProtocol->GetInfo(&prplInfo);
  NS_ENSURE_SUCCESS(rv, rv);

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
