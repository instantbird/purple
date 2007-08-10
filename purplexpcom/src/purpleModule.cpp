#include "nsIGenericFactory.h"
#include "purpleCoreService.h"
#include "purpleConversation.h"
#include "purpleProtocol.h"
#include "purplePref.h"
#include "purpleAccount.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(purpleCoreService);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleConversation);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleProtocol);
NS_GENERIC_FACTORY_CONSTRUCTOR(purplePref);
NS_GENERIC_FACTORY_CONSTRUCTOR(purpleAccount);

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
  }
};

/* prevent this dll from actually being loaded by Mozilla
 * (the platform-specific loader is used to get this instead)
 */
//#define NSGetModule REAL_NSGetModule

NS_IMPL_NSGETMODULE(purpleModule, components)
