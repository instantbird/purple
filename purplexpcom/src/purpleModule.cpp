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

#include "mozilla/ModuleUtils.h"
#include "nsICategoryManager.h"
#include "nsIClassInfoImpl.h"
#include "nsServiceManagerUtils.h"
#include "purpleCoreService.h"
#include "purpleConvIM.h"
#include "purpleConvChat.h"
#include "purpleProtocol.h"
#include "purpleUnknownProtocol.h"
#include "purpleAccountBase.h"
#include "purpleBuddy.h"
#include "purpleTag.h"
#include "purpleTagsService.h"
#include "purpleAccountBuddy.h"
#include "purpleMessage.h"
#include "purpleProxy.h"
#include "purpleProxyInfo.h"
#ifdef XP_MACOSX
# include "nsDockTile.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(purpleAccountBase)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleAccountBuddy)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleBuddy)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleConvChat)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleConvIM)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleCoreService)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleMessage)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProtocol)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProxyInfo)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleTag)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleTagsService)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleUnknownProtocol)
#ifdef XP_MACOSX
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDockTile)
#endif

NS_DEFINE_NAMED_CID(PURPLE_ACCOUNT_CID);
NS_DEFINE_NAMED_CID(PURPLE_ACCOUNTBUDDY_CID);
NS_DEFINE_NAMED_CID(PURPLE_BUDDY_CID);
NS_DEFINE_NAMED_CID(PURPLE_CONV_CHAT_CID);
NS_DEFINE_NAMED_CID(PURPLE_CONV_IM_CID);
NS_DEFINE_NAMED_CID(PURPLE_CORE_SERVICE_CID);
NS_DEFINE_NAMED_CID(PURPLE_MESSAGE_CID);
NS_DEFINE_NAMED_CID(PURPLE_PROTOCOL_CID);
NS_DEFINE_NAMED_CID(PURPLE_PROXY_CID);
NS_DEFINE_NAMED_CID(PURPLE_PROXY_INFO_CID);
NS_DEFINE_NAMED_CID(PURPLE_TAG_CID);
NS_DEFINE_NAMED_CID(PURPLE_TAGS_SERVICE_CID);
NS_DEFINE_NAMED_CID(PURPLE_UNKNOWN_PROTOCOL_CID);
#ifdef XP_MACOSX
NS_DEFINE_NAMED_CID(NSDOCKBADGESERVICE_CID);
#endif

static const mozilla::Module::CIDEntry kPurpleCIDs[] = {
  { &kPURPLE_ACCOUNT_CID, false, NULL, purpleAccountBaseConstructor },
  { &kPURPLE_ACCOUNTBUDDY_CID, false, NULL, purpleAccountBuddyConstructor },
  { &kPURPLE_BUDDY_CID, false, NULL, purpleBuddyConstructor },
  { &kPURPLE_CONV_CHAT_CID, false, NULL, purpleConvChatConstructor },
  { &kPURPLE_CONV_IM_CID, false, NULL, purpleConvIMConstructor },
  { &kPURPLE_CORE_SERVICE_CID, true, NULL, purpleCoreServiceConstructor },
  { &kPURPLE_MESSAGE_CID, false, NULL, purpleMessageConstructor },
  { &kPURPLE_PROTOCOL_CID, false, NULL, purpleProtocolConstructor },
  { &kPURPLE_PROXY_CID, false, NULL, purpleProxyConstructor },
  { &kPURPLE_PROXY_INFO_CID, false, NULL, purpleProxyInfoConstructor },
  { &kPURPLE_TAG_CID, false, NULL, purpleTagConstructor },
  { &kPURPLE_TAGS_SERVICE_CID, true, NULL, purpleTagsServiceConstructor },
  { &kPURPLE_UNKNOWN_PROTOCOL_CID, false, NULL,
    purpleUnknownProtocolConstructor },
#ifdef XP_MACOSX
  { &kNSDOCKBADGESERVICE_CID, true, NULL, nsDockTileConstructor },
#endif
  { NULL }
};

static const mozilla::Module::ContractIDEntry kPurpleContracts[] = {
  { PURPLE_ACCOUNT_CONTRACTID, &kPURPLE_ACCOUNT_CID },
  { PURPLE_ACCOUNTBUDDY_CONTRACTID, &kPURPLE_ACCOUNTBUDDY_CID },
  { PURPLE_BUDDY_CONTRACTID, &kPURPLE_BUDDY_CID },
  { PURPLE_CONV_CHAT_CONTRACTID, &kPURPLE_CONV_CHAT_CID },
  { PURPLE_CONV_IM_CONTRACTID, &kPURPLE_CONV_IM_CID },
  { PURPLE_CORE_SERVICE_CONTRACTID, &kPURPLE_CORE_SERVICE_CID },
  { PURPLE_MESSAGE_CONTRACTID, &kPURPLE_MESSAGE_CID },
  { PURPLE_PROTOCOL_CONTRACTID, &kPURPLE_PROTOCOL_CID },
  { PURPLE_PROXY_CONTRACTID, &kPURPLE_PROXY_CID },
  { PURPLE_PROXY_INFO_CONTRACTID, &kPURPLE_PROXY_INFO_CID },
  { PURPLE_TAG_CONTRACTID, &kPURPLE_TAG_CID },
  { PURPLE_TAGS_SERVICE_CONTRACTID, &kPURPLE_TAGS_SERVICE_CID },
  { PURPLE_UNKNOWN_PROTOCOL_CONTRACTID, &kPURPLE_UNKNOWN_PROTOCOL_CID },
#ifdef XP_MACOSX
  { NSDOCKBADGESERVICE_CONTRACTID, &kNSDOCKBADGESERVICE_CID },
#endif
  { NULL }
};

static const mozilla::Module kPurpleModule = {
  mozilla::Module::kVersion,
  kPurpleCIDs,
  kPurpleContracts
};

NSMODULE_DEFN(purpleModule) = &kPurpleModule;
