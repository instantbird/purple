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
#include "purpleCoreService.h"
#include "purpleStorage.h"
#include <nsIPrefService.h>
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsMemory.h>
#include <nsNetUtil.h>
#include <prprf.h>

NS_IMPL_ISUPPORTS1(purpleAccount, purpleIAccount)

#define PREF_PREFIX             "messenger.account."
#define PREF_OPTIONS            "options."
#define PREF_PRPL               "prpl"
#define PREF_NAME               "name"
#define PREF_ALIAS              "alias"
#define PREF_PROXY              "proxy"
#define PREF_AUTOLOGIN          "autoLogin"
#define PREF_PASSWORD           "password"
#define PREF_REMEMBER_PASSWORD  "rememberPassword"

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleAccount:5
//
static PRLogModuleInfo *gPurpleAccountLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleAccountLog, PR_LOG_DEBUG, args)

purpleAccount::purpleAccount()
  : mAccount(NULL),
    mReconnectAttempts(0),
    mTimeOfNextReconnect(0)
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

void purpleAccount::UnInit()
{
  NS_PRECONDITION(mAccount, "Uninitializing uninitialized purpleAccount\n");
  NS_ENSURE_TRUE(mAccount,);

  LOG(("uninitializing purpleAccount %s (%s)\n",
       purple_account_get_username(mAccount), mProtocol->GetId()));

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  mAccount->ui_data = NULL;
  // This will call purple_proxy_info_destroy if there was a proxy
  purple_account_set_proxy_info(mAccount, NULL);
  purple_accounts_delete(mAccount);
  mAccount = NULL;
}

#define PURPLE_IMPL_GETPREF(prettyType, purpleType, PRType)             \
  inline void purpleAccount::Get##prettyType##Pref(const char *aName)	\
  {									\
    PRType val;								\
    nsresult rv = mPrefOptBranch->Get##prettyType##Pref(aName, &val);	\
    if (NS_SUCCEEDED(rv)) {						\
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

nsresult purpleAccount::create(const nsACString& aName, purpleProtocol *aPrpl,
                               const nsCString& aKey)
{
  NS_ENSURE_ARG_POINTER(aPrpl);
  NS_ENSURE_TRUE(!aName.IsEmpty(), NS_ERROR_UNEXPECTED);

  PromiseFlatCString flatName(aName);
  if (purple_accounts_find(flatName.get(), aPrpl->GetId())) {
    // adding a duplicate account would confuse libpurple
    // XXX: Should this test be included inside purple_account_new?
    NS_WARNING("Attempted to create a duplicate account!");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mAccount = purple_account_new(flatName.get(), aPrpl->GetId());
  NS_ENSURE_TRUE(mAccount, NS_ERROR_FAILURE);
  mAccount->ui_data = this;
  purple_accounts_add(mAccount);

  nsresult rv = SetId(aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  mProtocol = aPrpl;

  rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  mPrefBranch->SetCharPref(PREF_NAME, flatName.get());
  mPrefBranch->SetCharPref(PREF_PRPL, aPrpl->GetId());

  nsCString prpl(aPrpl->GetId());
  rv = storeAccount(mId, flatName, prpl);
  if (NS_FAILED(rv)) {
    mPrefBranch->DeleteBranch("");
  }
  return rv;
}

nsresult purpleAccount::load(const nsCString& aKey)
{
  LOG(("loading account : %s\n", aKey.get()));
  nsresult rv = SetId(aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Get the name */
  nsCString name;
  mPrefBranch->GetCharPref(PREF_NAME, getter_Copies(name));
  NS_ENSURE_TRUE(!name.IsEmpty(), NS_ERROR_UNEXPECTED);
  LOG(("name = %s\n", name.get()));

  /* Get the prpl */
  nsCString prpl;
  mPrefBranch->GetCharPref(PREF_PRPL, getter_Copies(prpl));
  NS_ENSURE_TRUE(!prpl.IsEmpty(), NS_ERROR_UNEXPECTED);
  LOG(("prpl = %s\n", prpl.get()));

  rv = ensureStored(name, prpl);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Init the mProtocol member */
  nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  rv = pcs->GetProtoById(prpl, getter_AddRefs(mProtocol));
  NS_ENSURE_SUCCESS(rv, rv);

  mAccount = purple_account_new(name.get(), prpl.get());
  NS_ENSURE_TRUE(mAccount, NS_ERROR_FAILURE);
  mAccount->ui_data = this;
  purple_accounts_add(mAccount);

  /* Load the alias if any */
  nsCString alias;
  rv = mPrefBranch->GetCharPref(PREF_ALIAS, getter_Copies(alias));
  if (NS_SUCCEEDED(rv) && !alias.IsEmpty()) {
    purple_account_set_alias(mAccount, alias.get());
    LOG(("alias = %s\n", alias.get()));
  }

  /* Load proxy settings */
  nsCString proxyKey;
  rv = mPrefBranch->GetCharPref(PREF_PROXY, getter_Copies(proxyKey));
  /* Init mProxy */
  if (NS_SUCCEEDED(rv) &&
      StringBeginsWith(proxyKey, NS_LITERAL_CSTRING(PROXY_KEY))) {
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

nsresult purpleAccount::checkAutoLogin()
{
  /* Check if we should connect the account automatically */
  PRBool autoLogin;
  nsresult rv = mPrefBranch->GetBoolPref(PREF_AUTOLOGIN, &autoLogin);
  if (NS_FAILED(rv) || autoLogin) {
    LOG(("autologin\n"));
    Connect();
  }

  mPrefBranch = nsnull;
  return NS_OK;
}

nsresult purpleAccount::ensureStored(const nsCString& aName, const nsCString& aPrpl)
{
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  LOG(("Checking that the account with the id %u in mozStorage has the name %s and the prpl %s\n",
       mId, aName.get(), aPrpl.get()));
  mozIStorageStatement *statement = storageInstance->mEnsureAccountStoredStatement;
  mozStorageStatementScoper scoper(statement);
  nsresult rv = statement->BindInt32Parameter(0, mId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMoreData;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsCString name;
    rv = statement->GetUTF8String(0, name);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString prpl;
    rv = statement->GetUTF8String(1, prpl);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("Account at the id %u in mozStorage has the name %s and the prpl %s\n",
         mId, name.get(), prpl.get()));
    NS_ENSURE_TRUE(name.Equals(aName) && prpl.Equals(aPrpl), NS_ERROR_FAILURE);

    LOG(("Checking account (%u, %s, %s) OK\n", mId, name.get(), prpl.get()));
    return NS_OK;
  }

  LOG(("account (%u, %s, %s) not found in mozStorage, attempting to store it\n",
       mId, aName.get(), aPrpl.get()));

  return storeAccount(mId, aName, aPrpl);
}

nsresult purpleAccount::storeAccount(PRUint32 aKey, const nsCString& aName, const nsCString& aPrpl)
{
  LOG(("Adding account %u (%s, %s) in mozStorage\n", aKey, aName.get(), aPrpl.get()));
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO accounts (id, name, prpl) VALUES(?1, ?2, ?3)"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  statement->BindInt32Parameter(0, aKey);
  statement->BindUTF8StringParameter(1, aName);
  statement->BindUTF8StringParameter(2, aPrpl);

  rv = statement->Execute();
  NS_ASSERTION(NS_SUCCEEDED(rv), "adding account failed");
  return rv;
}

nsresult purpleAccount::unstoreAccount()
{
  LOG(("Removing account %u from mozStorage\n", mId));
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM accounts WHERE id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, mId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ASSERTION(NS_SUCCEEDED(rv), "removing account failed");
  NS_ENSURE_SUCCESS(rv, rv);

  // removing the account from the accounts table is not enought,
  // we need to remove all the associated account_buddy entries too
  LOG(("Removing account_buddy entries of account %u from mozStorage\n", mId));
  rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM account_buddy WHERE account_id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, mId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ASSERTION(NS_SUCCEEDED(rv), "removing account_buddy failed");

  return rv;
}

nsresult purpleAccount::remove()
{
  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Deleting purpleAccount %s (%s)\n",
       mAccount ? purple_account_get_username(mAccount) : "(mAccount = NULL)",
       mProtocol ? mProtocol->GetId() : "(mProtocol = NULL)"));

  /* Remove data from the pref branch */
  rv = mPrefBranch->DeleteBranch("");
  NS_ENSURE_SUCCESS(rv, rv); // FIXME Should we really return here?

  /* Remove the data from the accounts table in mozStorage */
  unstoreAccount();

  /* Remove the account from libpurple */
  UnInit();
  return NS_OK;
}

/* void connect (); */
NS_IMETHODIMP purpleAccount::Connect()
{
  PURPLE_ENSURE_INIT(mAccount);
  LOG(("Attempting to connect %s\n", mAccount->username));
  NS_ENSURE_TRUE(!NS_IsOffline(), NS_ERROR_FAILURE);

  if (purple_account_get_enabled(mAccount, UI_ID)) {
    /* The account is already enabled. This means that we have already
       connected it and we haven't disconnected it. If it isn't
       connected then it's because of a connection failure */
    LOG(("Account already enabled\n"));
    mConnectionErrorReason = (PurpleConnectionError)NO_ERROR;
    mConnectionErrorMessage.Truncate();
    purple_account_connect(mAccount);

    if (mTimeOfNextReconnect - PR_Now() > 1000) {
      // This is a manual reconnection, reset the auto-reconnect stuff
      mReconnectAttempts = 0;
      if (mTimer)
        mTimer->Cancel();
    }
  }
  else {
    /* enabling an account in libpurple connects it automatically */
    purple_account_set_enabled(mAccount, UI_ID, TRUE);
  }

  return NS_OK;
}

/* void disconnect (); */
NS_IMETHODIMP purpleAccount::Disconnect()
{
  PURPLE_ENSURE_INIT(mAccount);

  /* FIXME: this is a hack so that it is possible to reconnect after
     disconnecting an account. When libpurple disconnects an account,
     if !rememberPassword, the password is removed from the
     PurpleAccount structure.

     This hack should be removed when the UI for requesting a password
     from the user will be there
  */
  purple_account_set_remember_password(mAccount, TRUE);

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

/* purpleIConversation joinChat (in AUTF8String aName); */
NS_IMETHODIMP purpleAccount::JoinChat(const nsACString& aName,
                                      purpleIConversation **aResult)
{
  PURPLE_ENSURE_INIT(mAccount);
  PurpleConnection *gc = purple_account_get_connection(mAccount);
  NS_ENSURE_TRUE(gc, NS_ERROR_FAILURE);

  PromiseFlatCString flatName(aName);
  GHashTable *components = g_hash_table_new(g_str_hash, g_str_equal);
  g_hash_table_insert(components, const_cast<char *>("channel"),
                      const_cast<char *>(flatName.get()));
  serv_join_chat(gc, components);
  g_hash_table_destroy(components);

  PurpleConversation *conv =
    purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, flatName.get(), mAccount);
  if (conv) {
    purpleIConversation *result = (purpleIConversation *)conv->ui_data;
    NS_ENSURE_TRUE(result, NS_ERROR_NOT_INITIALIZED);
    NS_ADDREF(*aResult = result);
    return NS_OK;
  }

  *aResult = nsnull;
  return NS_OK;
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleAccount::GetName(nsACString& aName)
{
  PURPLE_ENSURE_INIT(mProtocol);

  aName = purple_account_get_username(mAccount);
  return NS_OK;
}

/* readonly attribute AUTF8String id; */
NS_IMETHODIMP purpleAccount::GetId(nsACString& aId)
{
  NS_ENSURE_TRUE(!mKey.IsEmpty(), NS_ERROR_NOT_INITIALIZED);

  aId = mKey;
  return NS_OK;
}

/* private */
inline nsresult purpleAccount::SetId(const nsCString& aId)
{
  /* Save the id of the account */
  mKey.Assign(aId);

  PRInt32 count = PR_sscanf(aId.get(), ACCOUNT_KEY "%u", &mId);
  NS_ENSURE_TRUE(count == 1, NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult purpleAccount::sendUpdateNotification()
{
  nsCOMPtr<purpleCoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  return pcs->NotifyObservers(this, "account-updated");
}

inline nsresult purpleAccount::GetPrefBranch()
{
  /* if we already have the pref branch, return early */
  if (mPrefBranch) {
    return NS_OK;
  }

  /* create the string for the root of the pref branch */
  nsCString root(PREF_PREFIX);
  root.Append(mKey);
  root.Append('.');

  /* get the branch */
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  nsresult rv = prefs->GetBranch(root.get(), getter_AddRefs(mPrefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  /* get the options pref branch */
  root.Append(PREF_OPTIONS);
  return prefs->GetBranch(root.get(), getter_AddRefs(mPrefOptBranch));
}

/* readonly attribute purpleIProtocol protocol; */
NS_IMETHODIMP purpleAccount::GetProtocol(purpleIProtocol * *aProtocol)
{
  PURPLE_ENSURE_INIT(mProtocol);

  NS_ADDREF(*aProtocol = mProtocol);
  return NS_OK;
}

/* attribute boolean autoLogin; */
NS_IMETHODIMP purpleAccount::GetAutoLogin(PRBool *aAutoLogin)
{
  PURPLE_ENSURE_INIT(mAccount);

  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrefBranch->GetBoolPref(PREF_AUTOLOGIN, aAutoLogin);
  /* If the pref isn't set, assume we want to sign-on at startup */
  if (NS_FAILED(rv)) {
    *aAutoLogin = PR_TRUE;
  }

  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetAutoLogin(PRBool aAutoLogin)
{
  PURPLE_ENSURE_INIT(mAccount);

  nsresult rv = SetBoolPref(PREF_AUTOLOGIN, aAutoLogin);
  NS_ENSURE_SUCCESS(rv, rv);

  return sendUpdateNotification();
}

/* attribute AUTF8String password; */
NS_IMETHODIMP purpleAccount::GetPassword(nsACString& aPassword)
{
  PURPLE_ENSURE_INIT(mAccount);

  aPassword = purple_account_get_password(mAccount);
  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetPassword(const nsACString& aPassword)
{
  PURPLE_ENSURE_INIT(mAccount);

  PromiseFlatCString flatPassword(aPassword);
  const char *pass = flatPassword.get();
  purple_account_set_password(mAccount, pass);
  SetStringPref(PREF_PASSWORD, pass);

  return NS_OK;
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
  PURPLE_ENSURE_INIT(mAccount);

  aAlias = purple_account_get_alias(mAccount);
  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetAlias(const nsACString& aAlias)
{
  PURPLE_ENSURE_INIT(mAccount);

  if (!aAlias.IsEmpty()) {
    PromiseFlatCString flatCString(aAlias);
    purple_account_set_alias(mAccount, flatCString.get());
    SetStringPref(PREF_ALIAS, flatCString.get());
  }
  else {
    purple_account_set_alias(mAccount, NULL);
    nsresult rv = GetPrefBranch();
    if(NS_SUCCEEDED(rv)) {
      mPrefBranch->DeleteBranch(PREF_ALIAS);
    }
  }

  return sendUpdateNotification();
}

/* attribute purpleIProxyInfo proxyInfo; */
NS_IMETHODIMP purpleAccount::GetProxyInfo(purpleIProxyInfo * *aProxyInfo)
{
  PURPLE_ENSURE_INIT(mAccount);

  NS_ADDREF(*aProxyInfo = mProxy);
  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetProxyInfo(purpleIProxyInfo * aProxyInfo)
{
  PURPLE_ENSURE_INIT(mAccount);
  NS_ENSURE_ARG(aProxyInfo);

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

  purple_account_set_proxy_info(mAccount, info);

  return NS_OK;
}

#define PURPLE_IMPL_SET(prettyType, purpleType, PRType, prefType)       \
  NS_IMETHODIMP purpleAccount::Set##prettyType(const char *aName,	\
					       PRType aVal)		\
  {									\
    PURPLE_ENSURE_INIT(mAccount);					\
									\
    /* Give that pref to libpurple */					\
    purple_account_set_##purpleType(mAccount, aName, aVal);		\
    									\
    /* Save the pref in our preference system */			\
    nsresult rv = GetPrefBranch();					\
    NS_ENSURE_SUCCESS(rv, rv);						\
									\
    return mPrefOptBranch->Set##prefType##Pref(aName, aVal);		\
  }

/* void setBool (in string aName, in boolean aVal); */
PURPLE_IMPL_SET(Bool, bool, PRBool, Bool)
/* void setInt (in string aName, in long aVal); */
PURPLE_IMPL_SET(Int, int, PRInt32, Int)
/* void setString (in string aName, in string aVal); */
PURPLE_IMPL_SET(String, string, const char *, Char)

/* void savePrefs (); */
NS_IMETHODIMP purpleAccount::Save()
{
  NS_ENSURE_TRUE(mPrefBranch, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  nsresult rv = prefs->SavePrefFile(nsnull);
  mPrefBranch = nsnull;
  return rv;
}

#define PURPLE_IMPL_SETPREF(prettyType, prefType, PRType)		\
  nsresult purpleAccount::Set##prettyType##Pref(const char *aName,	\
						PRType aValue)		\
  {									\
    nsresult rv = GetPrefBranch();					\
    NS_ENSURE_SUCCESS(rv, rv);						\
									\
    return mPrefBranch->Set##prefType##Pref(aName, aValue);		\
  }

// nsresult purpleAccount::SetBoolPref(const char *aName, PRBool aValue)
PURPLE_IMPL_SETPREF(Bool, Bool, PRBool)
// nsresult purpleAccount::SetIntPref(const char *aName, PRInt32 aValue)
PURPLE_IMPL_SETPREF(Int, Int, PRInt32)
// nsresult purpleAccount::SetStringPref(const char *aName, const char *aValue)
PURPLE_IMPL_SETPREF(String, Char, const char *)


/* attribute string connectionStateMsg; */
NS_IMETHODIMP purpleAccount::GetConnectionStateMsg(nsACString &aConnectionStateMsg)
{
  aConnectionStateMsg = mConnectionStateMsg;
  return NS_OK;
}

void purpleAccount::SetConnectionStateMsg(const char *aConnectionStateMsg)
{
  mConnectionStateMsg = aConnectionStateMsg;
  mConnectionErrorReason = (PurpleConnectionError) NO_ERROR;
  mConnectionErrorMessage.Truncate();
}

void purpleAccount::SetDisconnectReason(PurpleConnectionError reason,
                                        const char *text)

{
  mConnectionErrorReason = reason;
  mConnectionErrorMessage = text;
  mConnectionStateMsg.Truncate();

  if (purple_connection_error_is_fatal(reason))
    return;

  // Start the auto-reconnect process
  if (!mTimer)
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);

  nsCString timers;
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, );
  nsresult rv = prefs->GetCharPref("messenger.accounts.reconnectTimer",
                                   getter_Copies(timers));
  NS_ENSURE_SUCCESS(rv, );

  char *newStr = timers.BeginWriting();
  nsCAutoString time;
  PRUint32 i = 0;
  for (char *token = NS_strtok(",", &newStr);
       i <= mReconnectAttempts && token;
       ++i, token = NS_strtok(",", &newStr)) {
    time = token;
    time.StripWhitespace();
  }

  LOG(("time: %s, attempt %i", time.get(), mReconnectAttempts + 1));
  PRInt32 timer = strtol(time.get(), NULL, 10) * 1000;
  if (timer <= 0) {
    LOG(("No auto-reconnect"));
    return;
  }
  ++mReconnectAttempts;
  mTimeOfNextReconnect = PR_Now() + timer * 1000;
  mTimer->InitWithFuncCallback(reconnectTimerCallback, this,
                               timer, nsITimer::TYPE_ONE_SHOT);
  LOG(("timer initialized with %i s", timer / 1000));
}

void purpleAccount::Connected()
{
  mReconnectAttempts = 0;

  // FIXME: this is probably obsolete
  /* Remove the current connection state message so that the UI
     doesn't interpret it as an error / the cause of the disconnection
     We used to get a bogus error message when the user disconnected
     manually an account while it was connecting (see bug 88) */
  SetConnectionStateMsg("");
}

void purpleAccount::reconnectTimerCallback(nsITimer *aTimer, void *aClosure)
{
  LOG(("running reconnectTimerCallback"));
  static_cast<purpleAccount *>(aClosure)->Connect();
}

/* readonly attribute short connectionErrorReason; */
NS_IMETHODIMP purpleAccount::GetConnectionErrorReason(PRInt16 *aConnectionErrorReason)
{
  *aConnectionErrorReason = mConnectionErrorReason;
  return NS_OK;
}

/* readonly attribute AUTF8String connectionErrorMessage; */
NS_IMETHODIMP purpleAccount::GetConnectionErrorMessage(nsACString & aConnectionErrorMessage)
{
  aConnectionErrorMessage = mConnectionErrorMessage;
  return NS_OK;
}

/* readonly attribute long timeUntilReconnect; */
NS_IMETHODIMP purpleAccount::GetTimeOfNextReconnect(PRInt64 *aTimeOfNextReconnect)
{
  *aTimeOfNextReconnect = mTimeOfNextReconnect / 1000;
  return NS_OK;
}

#define PURPLE_IMPL_GETBOOL(aName, aVal)			\
  NS_IMETHODIMP purpleAccount::Get##aName(PRBool *a##aName)	\
  {								\
    PURPLE_ENSURE_INIT(mAccount);				\
    								\
    *a##aName = aVal;						\
    return NS_OK;						\
  }


/* readonly attribute boolean disconnecting; */
PURPLE_IMPL_GETBOOL(Disconnecting, mAccount->disconnecting)

/* readonly attribute boolean disconnected; */
PURPLE_IMPL_GETBOOL(Disconnected,
		    !mAccount->gc || mAccount->gc->state == PURPLE_DISCONNECTED)

/* readonly attribute boolean connected; */
PURPLE_IMPL_GETBOOL(Connected,
		    mAccount->gc && mAccount->gc->state == PURPLE_CONNECTED)

/* readonly attribute boolean connecting; */
PURPLE_IMPL_GETBOOL(Connecting,
		    mAccount->gc && mAccount->gc->state == PURPLE_CONNECTING)

/* Get flag from PurpleConnection->flags */
#define PURPLE_IMPL_GETFLAG(aName, aFlag)				\
  NS_IMETHODIMP purpleAccount::Get##aName(PRBool *a##aName)		\
  {									\
    PURPLE_ENSURE_INIT(mAccount);					\
    PURPLE_ENSURE_INIT(mAccount->gc);					\
    PurpleConnectionFlags flags = mAccount->gc->flags;			\
    *a##aName = (flags & PURPLE_CONNECTION_##aFlag) ? PR_TRUE : PR_FALSE; \
    return NS_OK;							\
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
