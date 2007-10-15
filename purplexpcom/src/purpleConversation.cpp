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

#include "purpleConversation.h"
#include "purpleUtils.h"
#include "nsXPCOM.h"
#include<nsStringAPI.h>
#include<nsEmbedString.h>

#include<nsCOMPtr.h>
#include<stdio.h>

//#define DEBUG_PURPLE_CONVERSATION

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
NS_IMETHODIMP purpleConversation::GetId(PRInt32 *aId)
{
  *aId = (PRInt32) mConv;

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
