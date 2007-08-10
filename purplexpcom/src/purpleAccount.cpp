#include "purpleAccount.h"
#include "purpleUtils.h"

NS_IMPL_ISUPPORTS1(purpleAccount, purpleIAccount)

//#define DEBUG_PURPLE_ACCOUNT

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
  if (mAccount) {
#ifdef DEBUG_PURPLE_ACCOUNT
    printf("destructing purpleAccount %s (%s)\n",
	   purple_account_get_username(mAccount), mProtocol->GetId());
#endif
    purple_account_destroy(mAccount);
  }
#ifdef DEBUG_PURPLE_ACCOUNT
  else
    printf("destruction uninitialized purpleAccount\n");
#endif
}

nsresult purpleAccount::create(const char *name, purpleProtocol *prpl)
{
  NS_ENSURE_ARG_POINTER(prpl);
  mAccount = purple_account_new(name, prpl->GetId());
  if (!mAccount) {
    return NS_ERROR_FAILURE;
  }

  mProtocol = prpl;

  return NS_OK;
}

/* void connect (); */
NS_IMETHODIMP purpleAccount::Connect()
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

#ifdef DEBUG_PURPLE_ACCOUNT
  printf("attempting to connect %s\n", mAccount->username);
#endif
  purple_account_set_enabled(mAccount, UI_ID, TRUE);
  //  purple_account_connect(mAccount);

  return NS_OK;
}

/* void disconnect (); */
NS_IMETHODIMP purpleAccount::Disconnect()
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  purple_account_disconnect(mAccount);
  return NS_OK;
}

/* readonly attribute string name; */
NS_IMETHODIMP purpleAccount::GetName(char * *aName)
{
  if (!mProtocol) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return returnString(aName, purple_account_get_username(mAccount));
}

/* readonly attribute string accountId; */
NS_IMETHODIMP purpleAccount::GetAccountId(char * *aAccountId)
{
  if (mKey.IsEmpty()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aAccountId = ToNewCString(mKey);
  return NS_OK;
}

/* private */
inline void purpleAccount::SetAccountId(const nsCString& aAccountId)
{
  mKey.Assign(aAccountId);
}

/* readonly attribute purpleIProtocol protocol; */
NS_IMETHODIMP purpleAccount::GetProtocol(purpleIProtocol * *aProtocol)
{
  if (!mProtocol) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(mProtocol);
  *aProtocol = mProtocol;
  return NS_OK;
}

/* attribute string password; */
NS_IMETHODIMP purpleAccount::GetPassword(char * *aPassword)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return returnString(aPassword, purple_account_get_password(mAccount));
}
NS_IMETHODIMP purpleAccount::SetPassword(const char * aPassword)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  purple_account_set_password(mAccount, aPassword);
  SetStringPref("password", aPassword);

  return NS_OK;
}

/* attribute boolean rememberPassword; */
NS_IMETHODIMP purpleAccount::GetRememberPassword(PRBool *aRememberPassword)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aRememberPassword = purple_account_get_remember_password(mAccount);
  return NS_OK;
}
NS_IMETHODIMP purpleAccount::SetRememberPassword(PRBool aRememberPassword)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  purple_account_set_remember_password(mAccount, aRememberPassword);
  SetBoolPref("rememberPassword", aRememberPassword);

  return NS_OK;
}

/* attribute string alias; */
NS_IMETHODIMP purpleAccount::GetAlias(char * *aAlias)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return returnString(aAlias, purple_account_get_alias(mAccount));
}
NS_IMETHODIMP purpleAccount::SetAlias(const char * aAlias)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  purple_account_set_alias(mAccount, aAlias);
  SetStringPref("alias", aAlias);
  return NS_OK;
}

/* void setBool (in string aName, in boolean aVal); */
NS_IMETHODIMP purpleAccount::SetBool(const char *aName, PRBool aVal)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  purple_account_set_bool(mAccount, aName, aVal);
  SetBoolPref(aName, aVal);
  return NS_OK;
}

/* void setInt (in string aName, in long aVal); */
NS_IMETHODIMP purpleAccount::SetInt(const char *aName, PRInt32 aVal)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  purple_account_set_int(mAccount, aName, aVal);
  SetIntPref(aName, aVal);
  return NS_OK;
}

/* void setString (in string aName, in string aVal); */
NS_IMETHODIMP purpleAccount::SetString(const char *aName, const char *aVal)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  purple_account_set_string(mAccount, aName, aVal);
  SetStringPref(aName, aVal);
  return NS_OK;
}

void purpleAccount::SetBoolPref(const char *aName, PRBool aValue)
{

}

void purpleAccount::SetIntPref(const char *aName, PRInt32 aValue)
{

}

void purpleAccount::SetStringPref(const char *aName, const char *aValue)
{

}
