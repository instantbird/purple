#include "purpleAccount.h"
#include "purpleUtils.h"
#include "purpleCoreService.h"
#include <nsIPrefService.h>
#include <nsServiceManagerUtils.h>

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

#define PURPLE_IMPL_GETPREF(prettyType, purpleType, PRType)			\
  inline void purpleAccount::Get##prettyType##Pref(const char *aName)	\
  {									\
    PRType val;								\
    nsresult rv = mPrefBranch->Get##prettyType##Pref(aName, &val);	\
    if (NS_SUCCEEDED(rv)) {						\
      purple_account_set_##purpleType(mAccount, aName, val);		\
    }									\
  }


PURPLE_IMPL_GETPREF(Bool, bool, PRBool)
PURPLE_IMPL_GETPREF(Char, string, char *)
PURPLE_IMPL_GETPREF(Int, int, PRInt32)

nsresult purpleAccount::create(const char *name, purpleProtocol *prpl,
			       const nsCString& aKey)
{
  NS_ENSURE_ARG_POINTER(prpl);
  if (!name || !*name)
    return NS_ERROR_UNEXPECTED;

  mAccount = purple_account_new(name, prpl->GetId());
  if (!mAccount) {
    return NS_ERROR_FAILURE;
  }

  SetId(aKey);
  mProtocol = prpl;

  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  mPrefBranch->SetCharPref(PREF_NAME, name);
  mPrefBranch->SetCharPref(PREF_PRPL, prpl->GetId());
  return NS_OK;
}

nsresult purpleAccount::load(const nsCString& aKey)
{
  SetId(aKey);
#ifdef DEBUG_PURPLE_ACCOUNT
  printf("loading account : %s\n", aKey.get());
#endif

  nsresult rv = GetPrefBranch();
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

  /* Load the alias if any */
  nsCString alias;
  rv = mPrefBranch->GetCharPref(PREF_ALIAS, getter_Copies(alias));
  if (NS_SUCCEEDED(rv)) {
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

  /* FIXME: this is a hack so that it is possible to reconnecte after
     disconnecting an account. When libpurple disconnects an account,
     if !rememberSavedPassword, the password is removed from the
     PurpleAccount structure.

     This hack should be removed when the UI for requesting a password
     from the user will be there
  */
  purple_account_set_remember_password(mAccount, TRUE);

  //get spec options
  PRUint32 count;
  char **prefs;
  rv = mPrefBranch->GetChildList(PREF_OPTIONS, &count, &prefs);
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef DEBUG_PURPLE_ACCOUNT
  printf("number of specific pref: %i\n", count);
#endif
  while (count--) {
    PRInt32 type;
    rv = mPrefBranch->GetPrefType(prefs[count], &type);
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

  /* Check if we should connect the account automatically */
  PRBool autoLogin;
  rv = mPrefBranch->GetBoolPref(PREF_AUTOLOGIN, &autoLogin);
  if (NS_FAILED(rv) || autoLogin) {
#ifdef DEBUG_PURPLE_ACCOUNT
    printf("autologin\n");
#endif
    Connect();
  }

  mPrefBranch = nsnull;
  return NS_OK;
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

  rv = mPrefBranch->DeleteBranch("");
  NS_ENSURE_SUCCESS(rv, rv);

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

  purple_account_disconnect(mAccount);
  return NS_OK;
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
inline void purpleAccount::SetId(const nsCString& aId)
{
  /* Save the id of the account */
  mKey.Assign(aId);
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
  return prefs->GetBranch(root.get(), getter_AddRefs(mPrefBranch));
}

/* readonly attribute purpleIProtocol protocol; */
NS_IMETHODIMP purpleAccount::GetProtocol(purpleIProtocol * *aProtocol)
{
  PURPLE_ENSURE_INIT(mProtocol)

  NS_ADDREF(mProtocol);
  *aProtocol = mProtocol;
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

  return SetBoolPref(PREF_AUTOLOGIN, aAutoLogin);
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

  purple_account_set_alias(mAccount, aAlias);
  SetStringPref(PREF_ALIAS, aAlias);
  return NS_OK;
}

#define PURPLE_IMPL_SET(prettyType, purpleType, PRType)			\
  NS_IMETHODIMP purpleAccount::Set##prettyType(const char *aName,	\
					       PRType aVal)		\
  {									\
    PURPLE_ENSURE_INIT(mAccount)					\
									\
    /* Give that pref to libpurple */					\
    purple_account_set_##purpleType(mAccount, aName, aVal);		\
    									\
    /* Save the pref in our preference system */			\
    nsCString key(PREF_OPTIONS);					\
    key.Append(aName);							\
    return Set##prettyType##Pref(key.get(), aVal);			\
  }

/* void setBool (in string aName, in boolean aVal); */
PURPLE_IMPL_SET(Bool, bool, PRBool)
/* void setInt (in string aName, in long aVal); */
PURPLE_IMPL_SET(Int, int, PRInt32)
/* void setString (in string aName, in string aVal); */
PURPLE_IMPL_SET(String, string, const char *)

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
