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

#include "nsIGenericFactory.h"
#include "nsICategoryManager.h"
#include "nsIClassInfoImpl.h"
#include "nsServiceManagerUtils.h"
#include "purpleCoreService.h"
#include "purpleContentPolicy.h"
#include "purpleConvIM.h"
#include "purpleConvChat.h"
#include "purpleOverrideProtocol.h"
#include "purpleProtocol.h"
#include "purpleUnknownProtocol.h"
#include "purpleAccount.h"
#include "purpleBuddy.h"
#include "purpleTag.h"
#include "purpleAccountBuddy.h"
#include "purpleMessage.h"
#include "purpleProxy.h"
#include "purpleProxyInfo.h"
#ifdef XP_MACOSX
# include "nsDockTile.h"
#endif

#define NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(_class) \
  NS_GENERIC_FACTORY_CONSTRUCTOR(_class)                     \
  NS_DECL_CLASSINFO(_class)

NS_GENERIC_FACTORY_CONSTRUCTOR(purpleContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleAccount)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleAccountBuddy)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleBuddy)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleConvChat)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleConvIM)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleCoreService)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleMessage)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleOverrideProtocol)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleProtocol)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleProxyInfo)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleTag)
NS_GENERIC_FACTORY_CONSTRUCTOR_AND_CLASSINFO(purpleUnknownProtocol)
#ifdef XP_MACOSX
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDockTile)
#endif

static NS_METHOD RegisterPurpleContentPolicy(nsIComponentManager *aCompMgr,
                                             nsIFile* aPath,
                                             const char* aRegistryLocation,
                                             const char* aComponentType,
                                             const nsModuleComponentInfo* aInfo)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString previous;
  return catman->AddCategoryEntry(NS_CONTENTPOLICY_CATEGORY,
                                  PURPLE_CONTENT_POLICY_CONTRACTID,
                                  PURPLE_CONTENT_POLICY_CONTRACTID,
                                  PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_METHOD UnregisterPurpleContentPolicy(nsIComponentManager *aCompMgr,
                                               nsIFile *aPath,
                                               const char *registryLocation,
                                               const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return catman->DeleteCategoryEntry(NS_CONTENTPOLICY_CATEGORY,
                                     PURPLE_CONTENT_POLICY_CONTRACTID,
                                     PR_TRUE);
}

#define PURPLE_MODULE_CI(_class, _macroname, _name)     \
  {                                                     \
    "PURPLE "_name,                                     \
    PURPLE_##_macroname##_CID,                          \
    PURPLE_##_macroname##_CONTRACTID,                   \
    purple##_class##Constructor,                        \
    nsnull,                                             \
    nsnull,                                             \
    nsnull,                                             \
    NS_CI_INTERFACE_GETTER_NAME(purple##_class),        \
    nsnull,                                             \
    &NS_CLASSINFO_NAME(purple##_class),                 \
    0                                                   \
  }

static nsModuleComponentInfo components[] =
{
  PURPLE_MODULE_CI(Account, ACCOUNT, "account"),
  PURPLE_MODULE_CI(AccountBuddy, ACCOUNTBUDDY, "account buddy"),
  PURPLE_MODULE_CI(Buddy, BUDDY, "buddy"),
  PURPLE_MODULE_CI(ConvChat, CONV_CHAT, "chat conversation"),
  PURPLE_MODULE_CI(ConvIM, CONV_IM, "IM conversation"),
  PURPLE_MODULE_CI(CoreService, CORE_SERVICE, "core service"),
  PURPLE_MODULE_CI(Message, MESSAGE, "message"),
  PURPLE_MODULE_CI(OverrideProtocol, OVERRIDE_PROTOCOL, "override protocol"),
  PURPLE_MODULE_CI(Protocol, PROTOCOL, "protocol"),
  PURPLE_MODULE_CI(Proxy, PROXY, "proxy"),
  PURPLE_MODULE_CI(ProxyInfo, PROXY_INFO, "proxy info"),
  PURPLE_MODULE_CI(Tag, TAG, "tag"),
  PURPLE_MODULE_CI(UnknownProtocol, UNKNOWN_PROTOCOL, "unknown protocol"),
  { "PURPLE conversation content policy",
    PURPLE_CONTENT_POLICY_CID,
    PURPLE_CONTENT_POLICY_CONTRACTID,
    purpleContentPolicyConstructor,
    RegisterPurpleContentPolicy,
    UnregisterPurpleContentPolicy
  }
#ifdef XP_MACOSX
  ,{
    "Dock Badge Service",
    NSDOCKBADGESERVICE_CID,
    NSDOCKBADGESERVICE_CONTRACTID,
    nsDockTileConstructor
  }
#endif
};

NS_IMPL_NSGETMODULE(purpleModule, components)
