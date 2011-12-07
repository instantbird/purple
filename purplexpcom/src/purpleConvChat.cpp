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

#include "purpleConvChat.h"
#include "purpleConvChatBuddy.h"
#include "purpleGListEnumerator.h"

#include <nsXPCOM.h>
#include <nsStringAPI.h>
#include <nsCOMPtr.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleConversation:5
//
extern PRLogModuleInfo *gPurpleConvLog;
#endif
#define LOG(args) PR_LOG(gPurpleConvLog, PR_LOG_DEBUG, args)

//NS_IMPL_ISUPPORTS2_CI(purpleConvChat, prplIConversation, prplIConvChat)
NS_IMPL_CLASSINFO(purpleConvChat, NULL, 0, PURPLE_CONV_CHAT_CID)
NS_INTERFACE_MAP_BEGIN(purpleConvChat)
  NS_INTERFACE_MAP_ENTRY(prplIConvChat)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(prplIConversation, purpleConversation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, prplIConvChat)
  NS_IMPL_QUERY_CLASSINFO(purpleConvChat)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(purpleConvChat)
NS_IMPL_RELEASE(purpleConvChat)

NS_IMPL_CI_INTERFACE_GETTER2(purpleConvChat, prplIConversation, prplIConvChat)

purpleConvChat::purpleConvChat()
{
  /* member initializers and constructor code */
  LOG(("new purpleConvChat with id %i", mId));
}

purpleConvChat::~purpleConvChat()
{
  /* destructor code */
  LOG(("destructing chat conversation with id %i", mId));
}

/* readonly attribute boolean isChat; */
NS_IMETHODIMP purpleConvChat::GetIsChat(PRBool *aIsChat)
{
  *aIsChat = PR_TRUE;
  return NS_OK;
}

/* readonly attribute AUTF8String normalizedName; */
NS_IMETHODIMP purpleConvChat::GetNormalizedName(nsACString& aNormalizedName)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  aNormalizedName = purple_normalize(purple_conversation_get_account(mConv),
                                     purple_conversation_get_name(mConv));
  aNormalizedName.Append(".chat");
  return NS_OK;
}

/* void sendMsg (in AUTF8String aMsg); */
NS_IMETHODIMP purpleConvChat::SendMsg(const nsACString& aMsg)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  purpleAccountScoper scoper(purpleConversation::GetAccount());
  PromiseFlatCString flatMsg(aMsg);
  purple_conv_chat_send(mConv->u.chat, flatMsg.get());
  LOG(("chat message sent: %s", flatMsg.get()));

  return NS_OK;
}

/* nsISimpleEnumerator GetParticipants (); */
NS_IMETHODIMP purpleConvChat::GetParticipants(nsISimpleEnumerator **_retval NS_OUTPARAM)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);
  LOG(("GetParticipants of conversation with id %i", mId));

  PurpleConvChat *chat = purple_conversation_get_chat_data(mConv);
  NS_ENSURE_TRUE(chat, NS_ERROR_FAILURE);

  purpleGListEnumerator *enumerator = new purpleGListEnumerator();
  enumerator->Init(purple_conv_chat_get_users(chat),
                   purpleTypeToInterface<purpleConvChatBuddy,
                                         prplIConvChatBuddy,
                                         PurpleConvChatBuddy>);
  NS_ADDREF(*_retval = enumerator);
  return NS_OK;
}

/* AUTF8String getNormalizedChatBuddyName (in AUTF8String aChatBuddyName); */
NS_IMETHODIMP purpleConvChat::GetNormalizedChatBuddyName(const nsACString & aChatBuddyName,
                                                         nsACString &aNormalizedName)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  aNormalizedName = aChatBuddyName;

  PurpleConnection *gc = purple_conversation_get_gc(mConv);
  if (!gc)
    return NS_OK;

  PurplePluginProtocolInfo *prplInfo = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
  if (!prplInfo || !prplInfo->get_cb_real_name)
    return NS_OK;

  gchar *realName =
    prplInfo->get_cb_real_name(gc,
                                purple_conv_chat_get_id(PURPLE_CONV_CHAT(mConv)),
                                PromiseFlatCString(aChatBuddyName).get());
  if (realName && *realName)
    aNormalizedName = realName;
  g_free(realName);
  return NS_OK;
}

/*          attribute AUTF8String topic; */
NS_IMETHODIMP purpleConvChat::GetTopic(nsACString & aTopic)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleConvChat *chat = purple_conversation_get_chat_data(mConv);
  NS_ENSURE_TRUE(chat, NS_ERROR_FAILURE);

  aTopic = purple_conv_chat_get_topic(chat);
  return NS_OK;
}
NS_IMETHODIMP purpleConvChat::SetTopic(const nsACString & aTopic)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleConnection *gc = purple_conversation_get_gc(mConv);
  NS_ENSURE_TRUE(gc, NS_ERROR_NOT_AVAILABLE);

  PurplePluginProtocolInfo *prplInfo = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
  NS_ENSURE_TRUE(prplInfo, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(prplInfo->set_chat_topic, NS_ERROR_NOT_IMPLEMENTED);

  purpleAccountScoper scoper(purpleConversation::GetAccount());
  prplInfo->set_chat_topic(gc,
                           purple_conv_chat_get_id(PURPLE_CONV_CHAT(mConv)),
                           PromiseFlatCString(aTopic).get());
  return NS_OK;
}

/* readonly attribute AUTF8String topicSetter; */
NS_IMETHODIMP purpleConvChat::GetTopicSetter(nsACString & aTopicSetter)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleConvChat *chat = purple_conversation_get_chat_data(mConv);
  NS_ENSURE_TRUE(chat, NS_ERROR_FAILURE);

  aTopicSetter = chat->who;
  return NS_OK;
}

/* readonly attribute boolean topicSettable; */
NS_IMETHODIMP purpleConvChat::GetTopicSettable(PRBool *aTopicSettable)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurplePluginProtocolInfo *prplInfo = NULL;
  PurpleConnection *gc = purple_conversation_get_gc(mConv);
  if (!gc || !(prplInfo = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)))
    *aTopicSettable = PR_FALSE;
  else
    *aTopicSettable = prplInfo->set_chat_topic != NULL;
  return NS_OK;
}

/* readonly attribute AUTF8String nick; */
NS_IMETHODIMP purpleConvChat::GetNick(nsACString & aNick)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleConvChat *chat = purple_conversation_get_chat_data(mConv);
  NS_ENSURE_TRUE(chat, NS_ERROR_FAILURE);

  aNick = purple_conv_chat_get_nick(chat);
  return NS_OK;
}

/* readonly attribute boolean left; */
NS_IMETHODIMP purpleConvChat::GetLeft(PRBool *aLeft)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleConvChat *chat = purple_conversation_get_chat_data(mConv);
  NS_ENSURE_TRUE(chat, NS_ERROR_FAILURE);

  *aLeft = purple_conv_chat_has_left(chat);
  return NS_OK;
}
