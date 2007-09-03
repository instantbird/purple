#ifndef PURPLE_MESSAGE_H_
#define PURPLE_MESSAGE_H_

#ifdef XP_WIN
# define _USE_32BIT_TIME_T
#endif

#include "purpleIMessage.h"
#include <libpurple/conversation.h>
// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include "nsStringAPI.h"

// 0ca0b0a3-ab52-434a-9176-a7d7daa74bad
#define PURPLE_MESSAGE_CID					\
  { 0x0ca0b0a3, 0xab52, 0x434a,					\
      { 0x91, 0x76, 0xa7, 0xd7, 0xda, 0xa7, 0x4b, 0xad }	\
  }

#define PURPLE_MESSAGE_CONTRACTID		\
  "@instantbird.org/purple/message;1"

class purpleMessage : public purpleIMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIMESSAGE

  purpleMessage();
  nsresult Init(PurpleConversation *conv, const char *who, const char *alias,
		const char *message, PurpleMessageFlags flags, time_t mtime);

private:
  ~purpleMessage();

protected:
  /* additional members */
  PurpleConversation *mConv;
  nsString mMessage;
  nsCString mWho, mAlias;
  PurpleMessageFlags mFlags;
  time_t mTime;
};

#endif /* !PURPLE_MESSAGE_H_ */
