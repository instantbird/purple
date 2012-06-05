/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsICategoryManager.h"
#include "nsIClassInfoImpl.h"
#include "nsServiceManagerUtils.h"
#include "purpleCoreService.h"
#include "purpleConvIM.h"
#include "purpleConvChat.h"
#include "purpleProtocol.h"
#include "purpleMessage.h"
#include "purpleProxy.h"
#include "purpleProxyInfo.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(purpleConvChat)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleConvIM)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleCoreService)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleMessage)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProtocol)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProxyInfo)

NS_DEFINE_NAMED_CID(PURPLE_CONV_CHAT_CID);
NS_DEFINE_NAMED_CID(PURPLE_CONV_IM_CID);
NS_DEFINE_NAMED_CID(PURPLE_CORE_SERVICE_CID);
NS_DEFINE_NAMED_CID(PURPLE_MESSAGE_CID);
NS_DEFINE_NAMED_CID(PURPLE_PROTOCOL_CID);
NS_DEFINE_NAMED_CID(PURPLE_PROXY_CID);
NS_DEFINE_NAMED_CID(PURPLE_PROXY_INFO_CID);

static const mozilla::Module::CIDEntry kPurpleCIDs[] = {
  { &kPURPLE_CONV_CHAT_CID, false, NULL, purpleConvChatConstructor },
  { &kPURPLE_CONV_IM_CID, false, NULL, purpleConvIMConstructor },
  { &kPURPLE_CORE_SERVICE_CID, true, NULL, purpleCoreServiceConstructor },
  { &kPURPLE_MESSAGE_CID, false, NULL, purpleMessageConstructor },
  { &kPURPLE_PROTOCOL_CID, false, NULL, purpleProtocolConstructor },
  { &kPURPLE_PROXY_CID, false, NULL, purpleProxyConstructor },
  { &kPURPLE_PROXY_INFO_CID, false, NULL, purpleProxyInfoConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kPurpleContracts[] = {
  { PURPLE_CONV_CHAT_CONTRACTID, &kPURPLE_CONV_CHAT_CID },
  { PURPLE_CONV_IM_CONTRACTID, &kPURPLE_CONV_IM_CID },
  { PURPLE_CORE_SERVICE_CONTRACTID, &kPURPLE_CORE_SERVICE_CID },
  { PURPLE_MESSAGE_CONTRACTID, &kPURPLE_MESSAGE_CID },
  { PURPLE_PROTOCOL_CONTRACTID, &kPURPLE_PROTOCOL_CID },
  { PURPLE_PROXY_CONTRACTID, &kPURPLE_PROXY_CID },
  { PURPLE_PROXY_INFO_CONTRACTID, &kPURPLE_PROXY_INFO_CID },
  { NULL }
};

static const mozilla::Module kPurpleModule = {
  mozilla::Module::kVersion,
  kPurpleCIDs,
  kPurpleContracts
};

NSMODULE_DEFN(purpleModule) = &kPurpleModule;
