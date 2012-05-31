/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleProxyInfo.h"
#include <nsServiceManagerUtils.h>
#include <nsIPrefService.h>
#include <nsIProgrammingLanguage.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>

NS_IMPL_CLASSINFO(purpleProxyInfo, NULL, 0, PURPLE_PROXY_INFO_CID)
NS_IMPL_THREADSAFE_CI(purpleProxyInfo)
NS_IMPL_ISUPPORTS1_CI(purpleProxyInfo, purpleIProxyInfo)

purpleProxyInfo::purpleProxyInfo()
  : mType(-2)
{
  /* member initializers and constructor code */
}

purpleProxyInfo::~purpleProxyInfo()
{
  /* destructor code */
}

/* attribute short type; */
NS_IMETHODIMP purpleProxyInfo::GetType(PRInt16 *aType)
{
  NS_ENSURE_TRUE(mType != -2, NS_ERROR_NOT_INITIALIZED);

  *aType = mType;
  return NS_OK;
}
NS_IMETHODIMP purpleProxyInfo::SetType(PRInt16 aType)
{
  NS_ENSURE_TRUE(mType == -2, NS_ERROR_ALREADY_INITIALIZED);

  NS_ENSURE_TRUE(aType != httpProxy && aType != socks4Proxy &&
                 aType != socks5Proxy, NS_ERROR_INVALID_ARG);

  mType = aType;
  return NS_OK;
}

/* readonly attribute ACString key; */
NS_IMETHODIMP purpleProxyInfo::GetKey(nsACString & aKey)
{
  NS_ENSURE_TRUE(mType != -2, NS_ERROR_NOT_INITIALIZED);

  if (mType == purpleIProxyInfo::useGlobal) {
    aKey = PROXY_KEY_GLOBAL;
  } else if (mType == purpleIProxyInfo::noProxy) {
    aKey = PROXY_KEY_NONE;
  } else if (mType == purpleIProxyInfo::useEnvVar) {
    aKey = PROXY_KEY_ENVVAR;
  } else {
    NS_NOTREACHED("unknown proxy type...");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/* [noscript] purpleNativeProxyInfo getPurpleProxy (); */
NS_IMETHODIMP purpleProxyInfo::GetPurpleProxy(PurpleProxyInfo * *aResult)
{
  NS_ENSURE_TRUE(mType != -2, NS_ERROR_NOT_INITIALIZED);

  if (mType == useGlobal) {
    *aResult = NULL;
  }
  else {
    *aResult = purple_proxy_info_new();
    purple_proxy_info_set_type(*aResult, (PurpleProxyType)mType);
  }

  return NS_OK;
}
