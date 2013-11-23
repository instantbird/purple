/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_CORE_SERVICE_H_
#define PURPLE_CORE_SERVICE_H_

#include "purpleProxy.h"
#include "purpleProxyInfo.h"
#include "purpleICoreService.h"
#include <nsIMutableArray.h>
#include <nsCOMArray.h>
#include <nsCOMPtr.h>
#include <nsIPrefBranch2.h>
#include <nsIObserver.h>

#define PURPLE_ENSURE_INIT(x)                   \
  NS_ENSURE_TRUE(x, NS_ERROR_NOT_INITIALIZED)

class purpleCoreService MOZ_FINAL : public purpleICoreService,
                                    public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_PURPLEICORESERVICE

  purpleCoreService();

private:
  ~purpleCoreService();
  nsresult InitProxies();
  nsCOMPtr<nsIPrefBranch> mPrefService;
  nsCOMPtr<nsIPrefBranch2> mPrefBranch2;

protected:
  bool mInitialized;
  bool mQuitting;
  nsCOMArray<purpleProxy> mProxies;
  nsCOMPtr<purpleIProxyInfo> mGlobalProxy;
};

#define PURPLE_CORE_SERVICE_CID \
    /* {3E8A9D5E-4774-465a-9733-EA81FD67F161} */ \
    { 0x3e8a9d5e, 0x4774, 0x465a, \
        { 0x97, 0x33, 0xea, 0x81, 0xfd, 0x67, 0xf1, 0x61 } \
    }

#define PURPLE_CORE_SERVICE_CONTRACTID \
    "@instantbird.org/libpurple/core;1"

#endif /* !PURPLE_CORE_SERVICE_H_ */
