#include "purpleMessage.h"
#include "purpleUtils.h"
#include "nsCOMPtr.h"
#include "purpleCoreService.h"
#include<nsServiceManagerUtils.h>

//#define DEBUG_PURPLE_MESSAGE

NS_IMPL_ISUPPORTS1(purpleMessage, purpleIMessage)

purpleMessage::purpleMessage()
{
  /* member initializers and constructor code */
#ifdef DEBUG_PURPLE_MESSAGE
  printf("Constructing purpleMessage\n");
#endif
}

purpleMessage::~purpleMessage()
{
  /* destructor code */
#ifdef DEBUG_PURPLE_MESSAGE
  printf("Destructing purpleMessage\n");
#endif
}

nsresult purpleMessage::Init(PurpleConversation *conv, const char *who, const char *alias,
			     const char *message, PurpleMessageFlags flags, time_t mtime)
{
#ifdef DEBUG_PURPLE_MESSAGE
  printf("Initializing purpleMessage\n");
#endif

  mConv = conv;
  mWho = who;
  mAlias = alias;
  const nsCString cMsg(message);
  NS_CStringToUTF16(cMsg, NS_CSTRING_ENCODING_UTF8, mMessage);

  mFlags = flags;
  mTime = mtime;
  return NS_OK;
}

#define PURPLE_IMPL_GETSTRING(aName, aString)			\
  NS_IMETHODIMP purpleMessage::Get##aName(char * *a##aName)	\
  {								\
    return returnString(a##aName, aString);			\
  }

/* readonly attribute string who; */
PURPLE_IMPL_GETSTRING(Who, mWho.get())

/* readonly attribute string alias; */
PURPLE_IMPL_GETSTRING(Alias, mAlias.get())

/* readonly attribute string message; */
//PURPLE_IMPL_GETSTRING(Message, ToNewUTF8String(mMessage))

/* readonly attribute string time; */
PURPLE_IMPL_GETSTRING(Time, 
		      purple_utf8_strftime("(%H:%M:%S)", localtime(&mTime)))

/* readonly attribute wstring message; */
NS_IMETHODIMP purpleMessage::GetMessage(PRUnichar * *aMessage)
{
  *aMessage = ToNewUnicode(mMessage);
  return NS_OK;
}

/* readonly attribute purpleIConversation conversation; */
NS_IMETHODIMP purpleMessage::GetConversation(purpleIConversation * *aConversation)
{
  nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);

  purpleIConversation *prplConv = nsnull;
  nsresult rv = pcs->GetConversationById((PRInt32) mConv, &prplConv);
  NS_ENSURE_SUCCESS(rv, rv);

  *aConversation = prplConv;
  NS_ADDREF(*aConversation);
  return NS_OK;
}

#define PURPLE_IMPL_GETBOOL(aName, aFlag)				\
  NS_IMETHODIMP purpleMessage::Get##aName(PRBool *a##aName)		\
  {									\
    *a##aName = (mFlags & PURPLE_MESSAGE_##aFlag) ? PR_TRUE : PR_FALSE;	\
    return NS_OK;							\
  }

/* readonly attribute boolean outgoing; */
PURPLE_IMPL_GETBOOL(Outgoing, SEND)

/* readonly attribute boolean incoming; */
PURPLE_IMPL_GETBOOL(Incoming, RECV)

/* readonly attribute boolean system; */
PURPLE_IMPL_GETBOOL(System, SYSTEM)

/* readonly attribute boolean autoResponse; */
PURPLE_IMPL_GETBOOL(AutoResponse, AUTO_RESP)

/* readonly attribute boolean containsNick; */
PURPLE_IMPL_GETBOOL(ContainsNick, NICK)

/* readonly attribute boolean noLog; */
PURPLE_IMPL_GETBOOL(NoLog, NO_LOG)

/* readonly attribute boolean whispered; */
PURPLE_IMPL_GETBOOL(Whispered, WHISPER)

/* readonly attribute boolean error; */
PURPLE_IMPL_GETBOOL(Error, ERROR)

/* readonly attribute boolean delayed; */
PURPLE_IMPL_GETBOOL(Delayed, DELAYED)

/* readonly attribute boolean noFormat; */
PURPLE_IMPL_GETBOOL(NoFormat, RAW)

/* readonly attribute boolean containsImages; */
PURPLE_IMPL_GETBOOL(ContainsImages, IMAGES)

/* readonly attribute boolean notification; */
PURPLE_IMPL_GETBOOL(Notification, NOTIFY)

/* readonly attribute boolean noLinkification; */
PURPLE_IMPL_GETBOOL(NoLinkification, NO_LINKIFY)
