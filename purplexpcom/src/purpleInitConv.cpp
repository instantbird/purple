/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma GCC visibility push(default)
#include <libpurple/conversation.h>
#pragma GCC visibility pop

#include "purpleConversation.h"
#include "purpleConvChatBuddy.h"
#include "purpleMessage.h"
#include "imIConversationsService.h"
#include <nsIObserverService.h>
#include <nsIPrefBranch2.h>
#include <nsIPrefService.h>
#include <nsISupportsPrimitives.h>
#include <nsArrayEnumerator.h>
#include <nsComponentManagerUtils.h>
#include <nsServiceManagerUtils.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleInit:5
//
extern PRLogModuleInfo *gPurpleInitLog;
#endif
#define LOG(args) PR_LOG(gPurpleInitLog, PR_LOG_DEBUG, args)

/*** Conversation uiops ***/
static void create_conv(PurpleConversation *aConv)
{
  LOG(("Create conversation with name: %s", purple_conversation_get_name(aConv)));

  nsCOMPtr<purpleConversation> conv;
  PurpleConversationType type = purple_conversation_get_type(aConv);
  const char *contractid = nullptr;
  switch (type) {
    case PURPLE_CONV_TYPE_IM:
      contractid = "@instantbird.org/purple/convim;1";
      break;
    case PURPLE_CONV_TYPE_CHAT:
      contractid = "@instantbird.org/purple/convchat;1";
      break;
    default:
      NS_WARNING("purpleInit: create_conv, Unknown conversation type\n");
      return;
  }
  conv = do_CreateInstance(contractid);
  NS_ENSURE_TRUE(conv, );
  conv->SetConv(aConv);

  nsCOMPtr<imIConversationsService> convs =
    do_GetService("@mozilla.org/chat/conversations-service;1");
  NS_ENSURE_TRUE(convs, );
  convs->AddConversation(conv);
}

static void destroy_conv(PurpleConversation *aConv)
{
  LOG(("destroy_conv uiop called"));

  NS_ENSURE_TRUE(aConv,);

  purpleConversation *conversation = purpleConversation::fromPurpleConv(aConv);
  NS_ENSURE_TRUE(conversation,);

  nsCOMPtr<imIConversationsService> convs =
    do_GetService("@mozilla.org/chat/conversations-service;1");
  NS_ENSURE_TRUE(convs, );
  convs->RemoveConversation(conversation);
}

static void notify_conv(PurpleConversation *aConv, nsISupports *aSubject,
                        const char *aTopic, const PRUnichar *aData = nullptr)
{
  NS_ENSURE_TRUE(aConv,);

  purpleConversation *conversation = purpleConversation::fromPurpleConv(aConv);
  NS_ENSURE_TRUE(conversation,);

  conversation->NotifyObservers(aSubject, aTopic, aData);
}

static void write_conv(PurpleConversation *conv, const char *who,
                       const char *alias, const char *message,
                       PurpleMessageFlags flags, time_t mtime)
{
  nsresult rv;
  nsCOMPtr<purpleMessage> msg =
    do_CreateInstance(PURPLE_MESSAGE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,);

  msg->Init(conv, who, alias, message, flags, mtime);
  notify_conv(conv, msg, "new-text");
}

static void conv_add_chat_users(PurpleConversation *conv,
                                GList *cbuddies,
                                gboolean new_arrivals)
{
  nsCOMArray<prplIConvChatBuddy> cBuddiesArray;

  while (cbuddies) {
    purpleConvChatBuddy *buddy = new purpleConvChatBuddy();
    buddy->Init((PurpleConvChatBuddy *)cbuddies->data);
    cBuddiesArray.AppendObject(buddy);
    cbuddies = cbuddies->next;
  }
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  NS_NewArrayEnumerator(getter_AddRefs(enumerator), cBuddiesArray);
  notify_conv(conv, enumerator, "chat-buddy-add");
}

static void conv_remove_chat_users(PurpleConversation *conv,
                                   GList *users)
{
  nsCOMArray<nsISupportsString> stringsArray;
  while (users) {
    nsCOMPtr<nsISupportsString> string =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
    string->SetData(NS_ConvertUTF8toUTF16((const char *)users->data));
    stringsArray.AppendObject(string);
    users = users->next;
  }
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  NS_NewArrayEnumerator(getter_AddRefs(enumerator), stringsArray);
  notify_conv(conv, enumerator, "chat-buddy-remove");
}

static void conv_update_chat_usr(PurpleConversation *conv, const char *user,
                                 const PRUnichar *aData = nullptr)
{
  PurpleConvChatBuddy *prplBuddy = purple_conv_chat_cb_find(PURPLE_CONV_CHAT(conv), user);
  NS_ENSURE_TRUE(prplBuddy,);

  purpleConvChatBuddy *buddy = new purpleConvChatBuddy();
  buddy->Init(prplBuddy);
  notify_conv(conv, static_cast<prplIConvChatBuddy *>(buddy),
              "chat-buddy-update", aData);
}

static void conv_update_chat_user(PurpleConversation *conv, const char *user)
{
  conv_update_chat_usr(conv, user, nullptr);
}

static void conv_rename_chat_user(PurpleConversation *conv, const char *old_name,
                                  const char *new_name, const char *new_alias)
{
  conv_update_chat_usr(conv, new_name, NS_ConvertUTF8toUTF16(old_name).get());
}

static PurpleConversationUiOps conversation_uiops =
{
  create_conv,               /* create_conversation  */
  destroy_conv,              /* destroy_conversation */
  NULL,                      /* write_chat           */
  NULL,                      /* write_im             */
  write_conv,                /* write_conv           */
  conv_add_chat_users,       /* chat_add_users       */
  conv_rename_chat_user,     /* chat_rename_user     */
  conv_remove_chat_users,    /* chat_remove_users    */
  conv_update_chat_user,     /* chat_update_user     */
  NULL,                      /* present              */
  NULL,                      /* has_focus            */
  NULL,                      /* custom_smiley_add    */
  NULL,                      /* custom_smiley_write  */
  NULL,                      /* custom_smiley_close  */
  NULL,                      /* send_confirm         */

  NULL, NULL, NULL, NULL
};

static void update_chat_topic(PurpleConversation *aConv,
                              const char *aWho,
                              const char *aNew)
{
  notify_conv(aConv, nullptr, "chat-update-topic");
}

static void conversation_updated(PurpleConversation *aConv, PurpleConvUpdateType aType)
{
  switch (aType) {
    case PURPLE_CONV_UPDATE_TYPING:
      notify_conv(aConv, nullptr, "update-typing");
      break;

    case PURPLE_CONV_UPDATE_TITLE:
      // The test prevents a warning caused by libpurple stupidly
      // calling purple_conversation_autoset_title before the
      // create_conversation uiop.
      if (aConv && aConv->ui_data)
        notify_conv(aConv, nullptr, "update-conv-title");
      break;

    case PURPLE_CONV_UPDATE_CHATLEFT:
      {
        purpleConversation *conversation =
          purpleConversation::fromPurpleConv(aConv);
        nsCOMPtr<nsIObserverService> os =
          do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
        if (conversation && os)
          os->NotifyObservers(conversation, "conversation-left-chat", nullptr);
      }
      notify_conv(aConv, nullptr, "update-conv-chatleft");
      break;

    default:
      ;
  }
}

static gint chat_invited(PurpleAccount *aAccount,
                         const char *aWho,
                         const char *aName,
                         const char *aMessage,
                         GHashTable *data)
{
  LOG(("Invited to a chat"));

  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, 0);

  PRInt32 value = 0;
  nsresult rv =
    prefs->GetIntPref("messenger.conversations.autoAcceptChatInvitations",
                      &value);
  NS_ENSURE_SUCCESS(rv, 0);

  return value;
}

void init_libpurple_conversations()
{
  purple_conversations_set_ui_ops(&conversation_uiops);

  // The handle is only used for disconnecting the handler, and even then only
  // the address is used; since we never actually do that, we can just pass in
  // random junk.
  static int handle;
  purple_signal_connect(purple_conversations_get_handle(), "chat-topic-changed", &handle,
                        PURPLE_CALLBACK(update_chat_topic), NULL);

  purple_signal_connect(purple_conversations_get_handle(), "conversation-updated", &handle,
                        PURPLE_CALLBACK(conversation_updated), NULL);

  purple_signal_connect(purple_conversations_get_handle(), "chat-invited", &handle,
                        PURPLE_CALLBACK(chat_invited), NULL);

  LOG(("Connecting to conversation signals"));
}
