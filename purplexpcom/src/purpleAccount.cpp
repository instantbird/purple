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
#include "purpleUtils.h"
#include "purpleCoreService.h"
#include "purpleStorage.h"
#include <pref/nsIPrefService.h>
#include <nsServiceManagerUtils.h>
#include <prprf.h>

NS_IMPL_ISUPPORTS1(purpleAccount, purpleIAccount)

//#define DEBUG_PURPLE_ACCOUNT

#define PREF_PREFIX		"messenger.account."
#define PREF_OPTIONS		"options."
#define PREF_PRPL		"prpl"
#define PREF_NAME		"name"
#define PREF_ALIAS		"alias"
#define PREF_AUTOLOGIN          "autoLogin"
#define PREF_PASSWORD		"password"
#define PREF_REMEMBER_PASSWORD	"rememberPassword"

purpleAccount::purpleAccount()
{
  /* member initializers and constructor code */
  mAccount = NULL;
#ifdef DEBUG_PURPLE_ACCOUNT
  printf("Creating purpleAccount\n");
#endif
}

purpleAccount::~purpleAccount()
{
  /* destructor code */
  UnInit();
#ifdef DEBUG_PURPLE_ACCOUNT
  printf("destructing purpleAccount\n");
#endif
}

void purpleAccount::UnInit()
{
  if (mAccount) {
#ifdef DEBUG_PURPLE_ACCOUNT
    printf("uninitializing purpleAccount %s (%s)\n",
	   purple_account_get_username(mAccount), mProtocol->GetId());
#endif
    purple_accounts_delete(mAccount);
    mAccount = NULL;
  }
#ifdef DEBUG_PURPLE_ACCOUNT
  else printf("uninitializing uninitialized purpleAccount\n");
#endif
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


PURPLE_IMPL_GETPREF(Bool, bool, PRBool)
PURPLE_IMPL_GETPREF(Char, string, char *)
PURPLE_IMPL_GETPREF(Int, int, PRInt32)

nsresult purpleAccount::create(const char *aName, purpleProtocol *aPrpl,
			       const nsCString& aKey)
{
  NS_ENSURE_ARG_POINTER(aPrpl);
  if (!aName || !*aName)
    return NS_ERROR_UNEXPECTED;

  mAccount = purple_account_new(aName, aPrpl->GetId());
  if (!mAccount) {
    return NS_ERROR_FAILURE;
  }
  purple_accounts_add(mAccount);

  nsresult rv = SetId(aKey);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mProtocol = aPrpl;

  rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  mPrefBranch->SetCharPref(PREF_NAME, aName);
  mPrefBranch->SetCharPref(PREF_PRPL, aPrpl->GetId());

  nsCString name(aName);
  nsCString prpl(aPrpl->GetId());
  rv = storeAccount(mId, name, prpl);
  if (NS_FAILED(rv)) {
    mPrefBranch->DeleteBranch("");
  }
  return rv;
}

nsresult purpleAccount::load(const nsCString& aKey)
{
#ifdef DEBUG_PURPLE_ACCOUNT
  printf("loading account : %s\n", aKey.get());
#endif
  nsresult rv = SetId(aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Get the name */
  nsCString name;
  mPrefBranch->GetCharPref(PREF_NAME, getter_Copies(name));
  if (name.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }
#ifdef DEBUG_PURPLE_ACCOUNT
  printf("name = %s\n", name.get());
#endif

  /* Get the prpl */
  nsCString prpl;
  mPrefBranch->GetCharPref(PREF_PRPL, getter_Copies(prpl));
  if (prpl.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }
#ifdef DEBUG_PURPLE_ACCOUNT
  printf("prpl = %s\n", prpl.get());
#endif

  rv = ensureStored(name, prpl);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Init the mProtocol member */
  nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  rv = pcs->GetProtoById(prpl.get(), getter_AddRefs(mProtocol));
#ifdef DEBUG_PURPLE_ACCOUNT
  if (NS_FAILED(rv)) {
    printf("GetProtoById failed!\n");
  }
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  mAccount = purple_account_new(name.get(), prpl.get());
  if (!mAccount) {
    return NS_ERROR_FAILURE;
  }
  purple_accounts_add(mAccount);

  /* Load the alias if any */
  nsCString alias;
  rv = mPrefBranch->GetCharPref(PREF_ALIAS, getter_Copies(alias));
  if (NS_SUCCEEDED(rv) && alias.Length()) {
    purple_account_set_alias(mAccount, alias.get());
#ifdef DEBUG_PURPLE_ACCOUNT
    printf("alias = %s\n", alias.get());
#endif
  }

  /* Load the password if it was saved */
  nsCString pass;
  rv = mPrefBranch->GetCharPref(PREF_PASSWORD, getter_Copies(pass));
  if (NS_SUCCEEDED(rv)) {
    purple_account_set_password(mAccount, pass.get());
#ifdef DEBUG_PURPLE_ACCOUNT
    printf("pass = %s\n", pass.get());
#endif
  }

  /* Load the rememberPassword pref */
  PRBool rememberPass;
  rv = mPrefBranch->GetBoolPref(PREF_REMEMBER_PASSWORD, &rememberPass);
  if (NS_SUCCEEDED(rv)) {
    purple_account_set_remember_password(mAccount, rememberPass);
#ifdef DEBUG_PURPLE_ACCOUNT
    printf("rememberPassword = %s\n", rememberPass ? "true" : "false");
#endif
  }

  //get spec options
  PRUint32 count;
  char **prefs;
  rv = mPrefOptBranch->GetChildList("", &count, &prefs);
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef DEBUG_PURPLE_ACCOUNT
  printf("number of specific pref: %i\n", count);
#endif
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

  return NS_OK;
}

nsresult purpleAccount::checkAutoLogin()
{
  /* Check if we should connect the account automatically */
  PRBool autoLogin;
  nsresult rv = mPrefBranch->GetBoolPref(PREF_AUTOLOGIN, &autoLogin);
  if (NS_FAILED(rv) || autoLogin) {
#ifdef DEBUG_PURPLE_ACCOUNT
    printf("autologin\n");
#endif
    Connect();
  }

  mPrefBranch = nsnull;
  return NS_OK;
}

nsresult purpleAccount::ensureStored(const nsCString& aName, const nsCString& aPrpl)
{
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

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

    if (!name.Equals(aName) || !prpl.Equals(aPrpl)) {
#ifdef DEBUG_PURPLE_ACCOUNT
      printf("checking account %u failed\n", mId);
      printf(" aName = %s; name = %s\n", aName.get(), name.get());
      printf(" aPrpl = %s; prpl = %s\n", aPrpl.get(), prpl.get());
#endif
      return NS_ERROR_FAILURE;
    }

#ifdef DEBUG_PURPLE_ACCOUNT
    printf("checking account (%u, %s, %s) OK\n", mId, name.get(), prpl.get());
#endif
    return NS_OK;
  }

#ifdef DEBUG_PURPLE_ACCOUNT
  printf("account not found in mozStorage\n");
#endif

  return storeAccount(mId, aName, aPrpl);
}

nsresult purpleAccount::storeAccount(PRUint32 aKey, const nsCString& aName, const nsCString& aPrpl)
{
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
#ifdef DEBUG_PURPLE_ACCOUNT
  if (NS_FAILED(rv)) {
    printf("adding account %u (%s, %s) failed\n", aKey, aName.get(), aPrpl.get());
  }
  else {
    printf("account %u (%s, %s) added\n", aKey, aName.get(), aPrpl.get());
  }
#endif
  return rv;
}

nsresult purpleAccount::unstoreAccount()
{
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
#ifdef DEBUG_PURPLE_ACCOUNT
  if (NS_FAILED(rv)) {
    printf("removing account %u from mozStorage failed\n", mId);
  }
  else {
    printf("account %u removed from mozStorage\n", mId);
  }
#endif

  return rv;
}

nsresult purpleAccount::remove()
{
  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_ACCOUNT
  printf("deleting purpleAccount %s (%s)\n",
	 mAccount ? purple_account_get_username(mAccount) : "(mAccount = NULL)",
	 mProtocol ? mProtocol->GetId() : "(mProtocol = NULL)");
#endif

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
  PURPLE_ENSURE_INIT(mAccount)

#ifdef DEBUG_PURPLE_ACCOUNT
  printf("attempting to connect %s\n", mAccount->username);
#endif
  if (purple_account_get_enabled(mAccount, UI_ID)) {
    /* The account is already enabled (= we have already connected it once) */
#ifdef DEBUG_PURPLE_ACCOUNT
    printf("account already enabled\n");
#endif
    purple_account_connect(mAccount);
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
  PURPLE_ENSURE_INIT(mAccount)

  /* FIXME: this is a hack so that it is possible to reconnect after
     disconnecting an account. When libpurple disconnects an account,
     if !rememberPassword, the password is removed from the
     PurpleAccount structure.

     This hack should be removed when the UI for requesting a password
     from the user will be there
  */
  purple_account_set_remember_password(mAccount, TRUE);

  purple_account_disconnect(mAccount);
  return NS_OK;
}

/* purpleIConversation createConversation (in string aName); */
NS_IMETHODIMP purpleAccount::CreateConversation(const char *aName,
						purpleIConversation **aResult)
{
  PurpleConversation *conv;
  conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, this->mAccount, aName);
  if (!conv) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  return pcs->GetConversationById((PRInt32) conv, aResult);
}

/* readonly attribute string name; */
NS_IMETHODIMP purpleAccount::GetName(char * *aName)
{
  PURPLE_ENSURE_INIT(mProtocol)

  return returnString(aName, purple_account_get_username(mAccount));
}

/* readonly attribute string id; */
NS_IMETHODIMP purpleAccount::GetId(char * *aId)
{
  if (mKey.IsEmpty()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aId = ToNewCString(mKey);
  return NS_OK;
}

/* private */
inline nsresult purpleAccount::SetId(const nsCString& aId)
{
  /* Save the id of the account */
  mKey.Assign(aId);

  if (PR_sscanf(aId.get(), ACCOUNT_KEY "%u", &mId) != 1)
    return NS_ERROR_FAILURE;

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
  PURPLE_ENSURE_INIT(mProtocol)

  *aProtocol = mProtocol;
  NS_ADDREF(*aProtocol);
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

/* attribute string password; */
NS_IMETHODIMP purpleAccount::GetPassword(char * *aPassword)
{
  PURPLE_ENSURE_INIT(mAccount)

  return returnString(aPassword, purple_account_get_password(mAccount));
}
NS_IMETHODIMP purpleAccount::SetPassword(const char * aPassword)
{
  PURPLE_ENSURE_INIT(mAccount)

  purple_account_set_password(mAccount, aPassword);
  SetStringPref(PREF_PASSWORD, aPassword);

  return NS_OK;
}

/* attribute boolean rememberPassword; */
NS_IMETHODIMP purpleAccount::GetRememberPassword(PRBool *aRememberPassword)
{
  PURPLE_ENSURE_INIT(mAccount)

  *aRememberPassword = purple_account_get_remember_password(mAccount);
  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetRememberPassword(PRBool aRememberPassword)
{
  PURPLE_ENSURE_INIT(mAccount)

  purple_account_set_remember_password(mAccount, aRememberPassword);
  SetBoolPref(PREF_REMEMBER_PASSWORD, aRememberPassword);

  return NS_OK;
}

/* attribute string alias; */
NS_IMETHODIMP purpleAccount::GetAlias(char * *aAlias)
{
  PURPLE_ENSURE_INIT(mAccount)

  return returnString(aAlias, purple_account_get_alias(mAccount));
}
NS_IMETHODIMP purpleAccount::SetAlias(const char * aAlias)
{
  PURPLE_ENSURE_INIT(mAccount)

  if (*aAlias) {
    purple_account_set_alias(mAccount, aAlias);
    SetStringPref(PREF_ALIAS, aAlias);
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

#define PURPLE_IMPL_SET(prettyType, purpleType, PRType, prefType)       \
  NS_IMETHODIMP purpleAccount::Set##prettyType(const char *aName,	\
					       PRType aVal)		\
  {									\
    PURPLE_ENSURE_INIT(mAccount)					\
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
  if (!mPrefBranch) {
    return NS_ERROR_UNEXPECTED;
  }

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
NS_IMETHODIMP purpleAccount::SetConnectionStateMsg(const nsACString &aConnectionStateMsg)
{
  mConnectionStateMsg = aConnectionStateMsg;
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
