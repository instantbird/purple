#include "purpleConversation.h"
#include "purpleUtils.h"
#include "nsXPCOM.h"

// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include<nsStringAPI.h>
#include<nsEmbedString.h>

#include<nsCOMPtr.h>
#include<stdio.h>

NS_IMPL_ISUPPORTS1(purpleConversation, purpleIConversation)

purpleConversation::purpleConversation()
{
  /* member initializers and constructor code */
#ifdef DEBUG_PURPLE_CONVERSATION
  printf("new purpleConversation\n");
#endif
}

purpleConversation::~purpleConversation()
{
  /* destructor code */
#ifdef DEBUG_PURPLE_CONVERSATION
  printf("destructing conversation with ptr %i\n", (int)mConv);
#endif
}

void purpleConversation::SetConv(PurpleConversation *aConv)
{
  mConv = aConv;
#ifdef DEBUG_PURPLE_CONVERSATION
  printf("conversion ptr set to : %i\n",(int) mConv);
#endif
}

/* readonly attribute string name; */
NS_IMETHODIMP purpleConversation::GetName(char * *aName)
{
  return returnString(aName, purple_conversation_get_name(mConv));
}

/* readonly attribute string title; */
NS_IMETHODIMP purpleConversation::GetTitle(char * *aTitle)
{
  return returnString(aTitle, purple_conversation_get_title(mConv));
}

/* readonly attribute long idConv; */
NS_IMETHODIMP purpleConversation::GetIdConv(PRInt32 *aIdConv)
{
  *aIdConv = (PRInt32) mConv;

  return NS_OK;
}

/* void sendMsg (in string aMsg); */
NS_IMETHODIMP purpleConversation::SendMsg(const PRUnichar *aMsg)
{
  const nsEmbedString msg(aMsg);
  nsEmbedCString cMsg;
  CopyUTF16toUTF8(msg, cMsg);

  if (mConv->type == PURPLE_CONV_TYPE_IM) {
    purple_conv_im_send(mConv->u.im, cMsg.get());
#ifdef DEBUG_PURPLE_CONVERSATION
    printf("message sent: %s\n", cMsg.get());
#endif
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
