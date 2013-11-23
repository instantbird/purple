/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_PROXY_INFO_H_
# define PURPLE_PROXY_INFO_H_

#pragma GCC visibility push(default)
#include <libpurple/proxy.h>
#pragma GCC visibility pop

#include "purpleIProxy.h"
#include <nsStringAPI.h>
#include <nsIClassInfo.h>

// 41d3137c-ccb6-4678-890a-4f7a622b7bf8
#define PURPLE_PROXY_INFO_CID                                   \
  { 0x41d3137c, 0xccb6, 0x4678,                                 \
      { 0x89, 0x0a, 0x4f, 0x7a, 0x62, 0x2b, 0x7b, 0xf8 }        \
  }

#define PURPLE_PROXY_INFO_CONTRACTID \
    "@instantbird.org/purple/proxyinfo;1"


#define PROXY_KEY_GLOBAL "global"
#define PROXY_KEY_NONE   "none"
#define PROXY_KEY_ENVVAR "envvar"

class purpleProxyInfo MOZ_FINAL : public purpleIProxyInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PURPLEIPROXYINFO

  purpleProxyInfo();

private:
  ~purpleProxyInfo();

protected:
  PRInt16   mType;
};

#endif /* !PURPLE_PROXY_INFO_H_ */
