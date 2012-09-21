/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleProxy.h"
#include <nsServiceManagerUtils.h>
#include <nsIPrefService.h>
#include <nsIProgrammingLanguage.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>

NS_IMPL_CLASSINFO(purpleProxy, NULL, 0, PURPLE_PROXY_CID)
NS_IMPL_THREADSAFE_CI(purpleProxy)
NS_IMPL_ISUPPORTS2_CI(purpleProxy, purpleIProxy, purpleIProxyInfo)

purpleProxy::purpleProxy()
  : mType(0),
  mHostname(nullptr), mPort(0),
  mUsername(nullptr), mPassword(nullptr),
  mKey(nullptr)
{
  /* member initializers and constructor code */
}

purpleProxy::~purpleProxy()
{
  /* destructor code */
}

nsresult purpleProxy::GetPrefBranch(nsCOMPtr<nsIPrefBranch>& aPrefBranch)
{
  nsCString prefRoot = NS_LITERAL_CSTRING(PROXY_PREF_ROOT);
  prefRoot.Append(mKey);
  prefRoot.Append(".");

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return prefs->GetBranch(prefRoot.get(), getter_AddRefs(aPrefBranch));
}

nsresult purpleProxy::Init(nsCString& aKey)
{
  NS_ENSURE_TRUE(!aKey.IsEmpty(), NS_ERROR_INVALID_ARG);

  mKey = aKey;
  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsresult rv = GetPrefBranch(prefBranch);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString type;
  rv = prefBranch->GetCharPref(PROXY_PREF_TYPE, getter_Copies(type));
  NS_ENSURE_SUCCESS(rv, rv);

  if (type.Equals(PROXY_TYPE_HTTP))
    mType = purpleIProxy::httpProxy;
  else if (type.Equals(PROXY_TYPE_SOCKS4))
    mType = purpleIProxy::socks4Proxy;
  else if (type.Equals(PROXY_TYPE_SOCKS5))
    mType = purpleIProxy::socks5Proxy;
  else {
    NS_NOTREACHED("Loading an unknown proxy type...");
    return NS_ERROR_FAILURE;
  }

  rv = prefBranch->GetCharPref(PROXY_PREF_HOSTNAME, getter_Copies(mHostname));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prefBranch->GetIntPref(PROXY_PREF_PORT, (PRInt32 *)&mPort);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prefBranch->GetCharPref(PROXY_PREF_USERNAME, getter_Copies(mUsername));
  if (NS_FAILED(rv)) {
    mUsername = EmptyCString();
  }

  rv = prefBranch->GetCharPref(PROXY_PREF_PASSWORD, getter_Copies(mPassword));
  if (NS_FAILED(rv)) {
    mPassword = EmptyCString();
  }

  return NS_OK;
}

nsresult purpleProxy::Init(nsCString& aKey, PRInt16 aType,
                           const nsACString& aHostname, PRUint32 aPort,
                           const nsACString& aUsername, const nsACString& aPassword)
{
  NS_ENSURE_TRUE(!aKey.IsEmpty(), NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aType >= purpleIProxy::httpProxy &&
                 aType <= purpleIProxy::socks5Proxy,
                 NS_ERROR_INVALID_ARG);

  mKey = aKey;
  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsresult rv = GetPrefBranch(prefBranch);
  NS_ENSURE_SUCCESS(rv, rv);

  mType = aType;
  nsCString type;
  switch(aType) {
    case purpleIProxy::httpProxy:
      type = PROXY_TYPE_HTTP;
      break;
    case purpleIProxy::socks4Proxy:
      type = PROXY_TYPE_SOCKS4;
      break;
    case purpleIProxy::socks5Proxy:
      type = PROXY_TYPE_SOCKS5;
      break;
    default:
      NS_NOTREACHED("Creating Unknown Proxy Type ?!?");
      return NS_ERROR_FAILURE;
  }
  rv = prefBranch->SetCharPref(PROXY_PREF_TYPE, type.get());
  NS_ENSURE_SUCCESS(rv, rv);

  mHostname = aHostname;
  rv = prefBranch->SetCharPref(PROXY_PREF_HOSTNAME,
                               PromiseFlatCString(aHostname).get());
  NS_ENSURE_SUCCESS(rv, rv);

  mPort = aPort;
  rv = prefBranch->SetIntPref(PROXY_PREF_PORT, aPort);
  NS_ENSURE_SUCCESS(rv, rv);

  mUsername = aUsername;
  if (!aUsername.IsEmpty()) {
    rv = prefBranch->SetCharPref(PROXY_PREF_USERNAME,
                                 PromiseFlatCString(aUsername).get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mPassword = aPassword;
  if (!aPassword.IsEmpty()) {
    rv = prefBranch->SetCharPref(PROXY_PREF_PASSWORD,
                                 PromiseFlatCString(aPassword).get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

bool purpleProxy::Equals(PRInt16 aType, const nsACString& aHostname, PRUint32 aPort,
                         const nsACString& aUsername, const nsACString& aPassword)
{
  return (aType == mType &&
          aPort == mPort &&
          mHostname.Equals(aHostname) &&
          mUsername.Equals(aUsername) &&
          mPassword.Equals(aPassword));
}

/* [noscript] purpleNativeProxyInfo getPurpleProxy (); */
NS_IMETHODIMP purpleProxy::GetPurpleProxy(PurpleProxyInfo * *aResult)
{
  NS_ENSURE_TRUE(mType >= purpleIProxyInfo::httpProxy &&
                 mType <= purpleIProxyInfo::socks5Proxy,
                 NS_ERROR_FAILURE);

  PurpleProxyInfo *result = purple_proxy_info_new();

  purple_proxy_info_set_type(result, (PurpleProxyType)mType);
  purple_proxy_info_set_host(result, mHostname.get());
  purple_proxy_info_set_port(result, mPort);
  purple_proxy_info_set_username(result, mUsername.get());
  purple_proxy_info_set_password(result, mPassword.get());

  *aResult = result;
  return NS_OK;
}

/* readonly attribute ACString key; */
NS_IMETHODIMP purpleProxy::GetKey(nsACString & aKey)
{
  NS_ENSURE_TRUE(!mKey.IsEmpty(), NS_ERROR_INVALID_ARG);

  aKey = mKey;
  return NS_OK;
}

/* readonly attribute short type; */
NS_IMETHODIMP purpleProxy::GetType(PRInt16 *aType)
{
  NS_ENSURE_TRUE(mType, NS_ERROR_NOT_INITIALIZED);

  *aType = mType;
  return NS_OK;
}
NS_IMETHODIMP purpleProxy::SetType(PRInt16 aType)
{
  NS_ENSURE_TRUE(mType, NS_ERROR_NOT_INITIALIZED);

  if (aType == mType) {
    NS_WARNING("Useless call purpleProxy::SetType");
    return NS_OK;
  }

  return NS_ERROR_ALREADY_INITIALIZED;
}

/* readonly attribute AUTF8String host; */
NS_IMETHODIMP purpleProxy::GetHost(nsACString & aHost)
{
  NS_ENSURE_TRUE(mType, NS_ERROR_NOT_INITIALIZED);

  aHost = mHostname;
  return NS_OK;
}

/* readonly attribute long port; */
NS_IMETHODIMP purpleProxy::GetPort(PRUint32 *aPort)
{
  NS_ENSURE_TRUE(mType, NS_ERROR_NOT_INITIALIZED);

  *aPort = mPort;
  return NS_OK;
}

/* readonly attribute AUTF8String username; */
NS_IMETHODIMP purpleProxy::GetUsername(nsACString & aUsername)
{
  NS_ENSURE_TRUE(mType, NS_ERROR_NOT_INITIALIZED);

  aUsername = mUsername;
  return NS_OK;
}

/* attribute AUTF8String password; */
NS_IMETHODIMP purpleProxy::GetPassword(nsACString & aPassword)
{
  NS_ENSURE_TRUE(mType, NS_ERROR_NOT_INITIALIZED);

  aPassword = mPassword;
  return NS_OK;
}
NS_IMETHODIMP purpleProxy::SetPassword(const nsACString & aPassword)
{
  mPassword = aPassword;

  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsresult rv = GetPrefBranch(prefBranch);
  NS_ENSURE_SUCCESS(rv, rv);

//   if (aPassword.IsEmpty())
//     return prefBranch->DeleteBranch(PROXY_PREF_PASSWORD);

  return prefBranch->SetCharPref(PROXY_PREF_PASSWORD, mPassword.get());
}
