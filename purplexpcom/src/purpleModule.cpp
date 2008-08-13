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
#include "purpleCoreService.h"
#include "purpleConversation.h"
#include "purpleProtocol.h"
#include "purplePref.h"
#include "purpleAccount.h"
#include "purpleBuddy.h"
#include "purpleTag.h"
#include "purpleAccountBuddy.h"
#include "purpleMessage.h"
#include "purpleSockets.h"
#include "purpleProxy.h"
#include "purpleProxyInfo.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(purpleCoreService)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleConversation)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProtocol)
NS_GENERIC_FACTORY_CONSTRUCTOR(purplePref)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleAccount)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleBuddy)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleMessage)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleSocket)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleTag)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleAccountBuddy)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProxyInfo)

static nsModuleComponentInfo components[] =
{
  {
    "PURPLE core service",
    PURPLE_CORE_SERVICE_CID,
    PURPLE_CORE_SERVICE_CONTRACTID,
    purpleCoreServiceConstructor
  },
  {
    "PURPLE conversation",
    PURPLE_CONVERSATION_CID,
    PURPLE_CONVERSATION_CONTRACTID,
    purpleConversationConstructor
  },
  {
    "PURPLE protocol",
    PURPLE_PROTOCOL_CID,
    PURPLE_PROTOCOL_CONTRACTID,
    purpleProtocolConstructor
  },
  {
    "PURPLE pref",
    PURPLE_PREF_CID,
    PURPLE_PREF_CONTRACTID,
    purplePrefConstructor
  },
  {
    "PURPLE account",
    PURPLE_ACCOUNT_CID,
    PURPLE_ACCOUNT_CONTRACTID,
    purpleAccountConstructor
  },
  {
    "PURPLE buddy",
    PURPLE_BUDDY_CID,
    PURPLE_BUDDY_CONTRACTID,
    purpleBuddyConstructor
  },
  {
    "PURPLE tag",
    PURPLE_TAG_CID,
    PURPLE_TAG_CONTRACTID,
    purpleTagConstructor
  },
  {
    "PURPLE account buddy",
    PURPLE_ACCOUNTBUDDY_CID,
    PURPLE_ACCOUNTBUDDY_CONTRACTID,
    purpleAccountBuddyConstructor
  },
  {
    "PURPLE message",
    PURPLE_MESSAGE_CID,
    PURPLE_MESSAGE_CONTRACTID,
    purpleMessageConstructor
  },
  {
    "PURPLE socket",
    PURPLE_SOCKET_CID,
    PURPLE_SOCKET_CONTRACTID,
    purpleSocketConstructor
  },
  {
    "PURPLE proxy",
    PURPLE_PROXY_CID,
    PURPLE_PROXY_CONTRACTID,
    purpleProxyConstructor
  },
  {
    "PURPLE proxy info",
    PURPLE_PROXY_INFO_CID,
    PURPLE_PROXY_INFO_CONTRACTID,
    purpleProxyInfoConstructor
  }
};

NS_IMPL_NSGETMODULE(purpleModule, components)
