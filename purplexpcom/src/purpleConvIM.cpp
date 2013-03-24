/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleConvIM.h"
#include "purpleAccountBuddy.h"

#include <nsIObserverService.h>
#include <nsServiceManagerUtils.h>
#include <nsXPCOM.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleConversation:5
//
extern PRLogModuleInfo *gPurpleConvLog;
#endif
#define LOG(args) PR_LOG(gPurpleConvLog, PR_LOG_DEBUG, args)

//NS_IMPL_ISUPPORTS2_CI(purpleConvIM, prplIConversation, prplIConvIM)
NS_IMPL_CLASSINFO(purpleConvIM, NULL, 0, PURPLE_CONV_IM_CID)
NS_INTERFACE_MAP_BEGIN(purpleConvIM)
  NS_INTERFACE_MAP_ENTRY(prplIConvIM)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(prplIConversation, purpleConversation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, prplIConvIM)
  NS_IMPL_QUERY_CLASSINFO(purpleConvIM)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(purpleConvIM)
NS_IMPL_RELEASE(purpleConvIM)

NS_IMPL_CI_INTERFACE_GETTER2(purpleConvIM, prplIConversation, prplIConvIM)

purpleConvIM::purpleConvIM()
  : mSentTyping(PR_FALSE)
{
  /* member initializers and constructor code */
  LOG(("new purpleConvIM with id %i", mId));
}

purpleConvIM::~purpleConvIM()
{
  /* destructor code */
  LOG(("destructing IM conversation with id %i", mId));
}

/* readonly attribute boolean isChat; */
NS_IMETHODIMP purpleConvIM::GetIsChat(bool *aIsChat)
{
  *aIsChat = PR_FALSE;
  return NS_OK;
}

/* readonly attribute AUTF8String normalizedName; */
NS_IMETHODIMP purpleConvIM::GetNormalizedName(nsACString& aNormalizedName)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  aNormalizedName = purple_normalize(purple_conversation_get_account(mConv),
                                     purple_conversation_get_name(mConv));
  return NS_OK;
}

/* void sendMsg (in AUTF8String aMsg); */
NS_IMETHODIMP purpleConvIM::SendMsg(const nsACString& aMsg)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  purpleAccountScoper scoper(purpleConversation::GetAccount());
  PromiseFlatCString flatMsg(aMsg);
  purple_conv_im_send(mConv->u.im, flatMsg.get());
  LOG(("IM message sent: %s", flatMsg.get()));

  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->NotifyObservers(static_cast<prplIConvIM *>(this), "im-sent",
                           NS_ConvertUTF8toUTF16(aMsg).get());
  NS_ENSURE_SUCCESS(rv, rv);

  mSentTyping = PR_FALSE;
  return NS_OK;
}

/* readonly attribute imIAccountBuddy buddy; */
NS_IMETHODIMP purpleConvIM::GetBuddy(imIAccountBuddy * *aBuddy)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleBuddy *buddy =
    purple_find_buddy(purple_conversation_get_account(mConv),
                      purple_conversation_get_name(mConv));
  if (!buddy) {
    *aBuddy = nullptr;
    return NS_OK;
  }

  imIAccountBuddy *pab = purpleAccountBuddy::fromPurpleBuddy(buddy);
  NS_ENSURE_TRUE(pab, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aBuddy = pab);
  return NS_OK;
}

/* readonly attribute short typingState; */
NS_IMETHODIMP purpleConvIM::GetTypingState(PRInt16 *aTypingState)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleConvIm *im = PURPLE_CONV_IM(mConv);
  NS_ENSURE_TRUE(im, NS_ERROR_UNEXPECTED);

  *aTypingState = purple_conv_im_get_typing_state(im);
  return NS_OK;
}

/* long sendTyping (in AUTF8String aString); */
NS_IMETHODIMP purpleConvIM::SendTyping(const nsACString & aString, int32_t *_retval)
{
  *_retval = purpleConversation::NO_TYPING_LIMIT;
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  if (!purple_prefs_get_bool("/purple/conversations/im/send_typing"))
    return NS_OK;

  PurpleConvIm *im = PURPLE_CONV_IM(mConv);
  NS_ENSURE_TRUE(im, NS_ERROR_UNEXPECTED);

  LOG(("purpleConvIM::SendTyping Length = %u", aString.Length()));
  purpleAccountScoper scoper(purpleConversation::GetAccount());

  // First, stop the typed timeout
  purple_conv_im_stop_send_typed_timeout(im);

  if (aString.IsEmpty()) {
    if (!mSentTyping)
      return NS_OK;

    // All the content was removed, send a NOT_TYPING message
    serv_send_typing(purple_conversation_get_gc(mConv),
                     purple_conversation_get_name(mConv),
                     PURPLE_NOT_TYPING);
    LOG(("purpleConvIM::SendTyping Send PURPLE_NOT_TYPING"));
    mSentTyping = PR_FALSE;
    return NS_OK;
  }

  // At this point, we are actually typing, so reset the typed timeout
  purple_conv_im_start_send_typed_timeout(im);

  // Check if we need to send a TYPING message to the server.
  // We need to send a message if we just typed the first character
  // or if the type_again value is not null and is less than the
  // current timestamp
  if (!mSentTyping || (purple_conv_im_get_type_again(im) != 0 &&
                       time(NULL) > purple_conv_im_get_type_again(im))) {
    unsigned int timeout = serv_send_typing(purple_conversation_get_gc(mConv),
                                            purple_conversation_get_name(mConv),
                                            PURPLE_TYPING);
    LOG(("purpleConvIM::SendTyping sent PURPLE_TYPING"));
    purple_conv_im_set_type_again(im, timeout);
    mSentTyping = PR_TRUE;
  }

  return NS_OK;
}
