#include "purpleIConversation.h"
#include <libpurple/conversation.h>

// 10f8465f-b9d5-41b1-a7a9-247a576f4492
#define PURPLE_CONVERSATION_CID \
  { 0x10f8465f, 0xb9d5, 0x41b1,				 \
      { 0xa7, 0xa9, 0x24, 0x7a, 0x57, 0x6f, 0x44, 0x92 } \
  }

#define PURPLE_CONVERSATION_CONTRACTID \
    "@instantbird.org/purple/conversation;1"

class purpleConversation : public purpleIConversation
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEICONVERSATION

  purpleConversation();

  void SetConv(PurpleConversation *aConv);

private:
  ~purpleConversation();

protected:
  PurpleConversation *mConv;
};
