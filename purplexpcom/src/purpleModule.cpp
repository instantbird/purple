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

NS_GENERIC_FACTORY_CONSTRUCTOR(purpleCoreService);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleConversation);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProtocol);
NS_GENERIC_FACTORY_CONSTRUCTOR(purplePref);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleAccount);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleBuddy);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleMessage);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleSocket);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleTag);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleAccountBuddy);

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
  }
};

NS_IMPL_NSGETMODULE(purpleModule, components)
