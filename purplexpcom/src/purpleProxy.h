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

#ifndef PURPLE_PROXY_H_
# define PURPLE_PROXY_H_

#pragma GCC visibility push(default)
#include <libpurple/proxy.h>
#pragma GCC visibility pop

#include "purpleIProxy.h"
#include <nsIPrefBranch.h>
#include <nsStringAPI.h>
#include "nsCOMPtr.h"

// 48293b02-4972-424a-a1c0-72b413a8b5d9
#define PURPLE_PROXY_CID                                        \
  { 0x48293b02, 0x4972, 0x424a,                                 \
      { 0xa1, 0xc0, 0x72, 0xb4, 0x13, 0xa8, 0xb5, 0xd9 }        \
  }

#define PURPLE_PROXY_CONTRACTID \
  "@instantbird.org/purple/proxy;1"

#define PREF_MESSENGER_GLOBAL_PROXY "messenger.globalProxy"
#define PREF_MESSENGER_PROXIES      "messenger.proxies"
#define PROXY_KEY                   "proxy"
#define PROXY_PREF_ROOT     "messenger.proxy."
#define PROXY_PREF_HOSTNAME "host"
#define PROXY_PREF_PORT     "port"
#define PROXY_PREF_USERNAME "username"
#define PROXY_PREF_PASSWORD "password"
#define PROXY_PREF_TYPE    "type"
#define PROXY_TYPE_HTTP   "http"
#define PROXY_TYPE_SOCKS4 "socks4"
#define PROXY_TYPE_SOCKS5 "socks5"

class purpleProxy : public purpleIProxy
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIPROXYINFO
  NS_DECL_PURPLEIPROXY

  purpleProxy();

  // This method is used to load a proxy from the preferences
  nsresult Init(nsCString& aKey);

  // This method is used when adding a new proxy. The method will
  // initialize the purpleProxy members, and store the values in the
  // preferences.
  nsresult Init(nsCString& aKey, PRInt16 aType,
                const nsACString& aHostname, PRUint32 aPort,
                const nsACString& aUsername, const nsACString& aPassword);

  PurpleProxyInfo *getPurpleProxy();

  const nsCString& GetKey() { return mKey; }

  PRBool Equals(PRInt16 aType, const nsACString& aHostname, PRUint32 aPort,
                const nsACString& aUsername, const nsACString& aPassword);

private:
  ~purpleProxy();
  nsresult GetPrefBranch(nsCOMPtr<nsIPrefBranch>& aPrefBranch);

protected:
  PRInt16   mType;
  nsCString mHostname;
  PRUint32  mPort;
  nsCString mUsername;
  nsCString mPassword;

  // Keep the text address of the pref branch where we can store the related data
  // We won't need it anymore when the password will be in a separate password manager.
  nsCString mKey;
};

#endif /* !PURPLE_PROXY_H_ */
