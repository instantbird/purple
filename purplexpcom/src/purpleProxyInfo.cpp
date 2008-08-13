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
 * 2008.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#include "purpleProxyInfo.h"
#include <nsServiceManagerUtils.h>
#include <pref/nsIPrefService.h>

NS_IMPL_ISUPPORTS1(purpleProxyInfo, purpleIProxyInfo)

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
