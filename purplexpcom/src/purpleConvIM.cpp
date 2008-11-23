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

#include "purpleConvIM.h"
#include "purpleIAccountBuddy.h"

#include <nsXPCOM.h>
#include <nsStringAPI.h>
#include <nsCOMPtr.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleConversation:5
//
extern PRLogModuleInfo *gPurpleConvLog;
#endif
#define LOG(args) PR_LOG(gPurpleConvLog, PR_LOG_DEBUG, args)

//NS_IMPL_ISUPPORTS2(purpleConvIM, purpleIConversation, purpleIConvIM)
NS_INTERFACE_MAP_BEGIN(purpleConvIM)
  NS_INTERFACE_MAP_ENTRY(purpleIConvIM)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(purpleIConversation, purpleConversation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, purpleIConvIM)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(purpleConvIM)
NS_IMPL_RELEASE(purpleConvIM)

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

/* void sendMsg (in AUTF8String aMsg); */
NS_IMETHODIMP purpleConvIM::SendMsg(const nsACString& aMsg)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PromiseFlatCString flatMsg(aMsg);
  PRBool handled;
  nsresult rv = DoCommand(flatMsg.get(), handled);
  NS_ENSURE_SUCCESS(rv, rv);

  if (handled) {
    return SendTyping(0);
  }

  purple_conv_im_send(mConv->u.im, flatMsg.get());
  LOG(("IM message sent: %s", flatMsg.get()));
  mSentTyping = PR_FALSE;
  return NS_OK;
}

/* readonly attribute purpleIAccountBuddy buddy; */
NS_IMETHODIMP purpleConvIM::GetBuddy(purpleIAccountBuddy * *aBuddy)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleBuddy *buddy = purple_find_buddy(purple_conversation_get_account(mConv),
					 purple_conversation_get_name(mConv));
  if (buddy && buddy->node.ui_data)
    NS_IF_ADDREF(*aBuddy = (purpleIAccountBuddy *)buddy->node.ui_data);
  else
    *aBuddy = nsnull;

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

/* void sendTyping (in unsigned long aLength); */
NS_IMETHODIMP purpleConvIM::SendTyping(PRUint32 aLength)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleConvIm *im = PURPLE_CONV_IM(mConv);
  NS_ENSURE_TRUE(im, NS_ERROR_UNEXPECTED);

  LOG(("purpleConvIM::SendTyping Length = %u", aLength));

  // First, stop the typed timeout
  purple_conv_im_stop_send_typed_timeout(im);

  if (!aLength) {
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
