/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_PROXY_H_
# define PURPLE_PROXY_H_

#pragma GCC visibility push(default)
#include <libpurple/proxy.h>
#pragma GCC visibility pop

#include "purpleIProxy.h"
#include <nsIPrefBranch.h>
#include <nsIClassInfo.h>
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

class purpleProxy MOZ_FINAL : public purpleIProxy
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
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

  bool Equals(PRInt16 aType, const nsACString& aHostname, PRUint32 aPort,
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
