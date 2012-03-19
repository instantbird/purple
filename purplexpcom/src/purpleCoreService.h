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

class purpleCoreService : public purpleICoreService, public nsIObserver
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
