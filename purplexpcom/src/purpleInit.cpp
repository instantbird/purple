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


#define CUSTOM_PLUGIN_PATH     "protocols"

#pragma GCC visibility push(default)
#include <libpurple/account.h>
#include <libpurple/accountopt.h>
#include <libpurple/cmds.h>
#include <libpurple/conversation.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/eventloop.h>
#include <libpurple/ft.h>
#include <libpurple/gettext.h>
#include <libpurple/idle.h>
#include <libpurple/log.h>
#include <libpurple/notify.h>
#include <libpurple/prefs.h>
#include <libpurple/prpl.h>
#include <libpurple/pounce.h>
#include <libpurple/savedstatuses.h>
#include <libpurple/sound.h>
#include <libpurple/status.h>
#include <libpurple/util.h>
#include <libpurple/whiteboard.h>
#include <libpurple/dnsquery.h>
#pragma GCC visibility pop

#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "purpleConversation.h"
#include "purpleConvChatBuddy.h"
#include "purpleCoreService.h"
#include "purpleGetText.h"
#include "purpleMessage.h"
#include "purpleTimer.h"
#include "purpleSockets.h"
#include "purpleDNS.h"
#include <purpleIPlugin.h>
#include <xpcom-config.h>
#include <nsServiceManagerUtils.h>
#include <nsIObserverService.h>
#include <nsCOMPtr.h>
#include <nsStringAPI.h>
#include <nsNetCID.h>
#include <nsEmbedString.h>
#include <nsICategoryManager.h>
#include <nsIDOMWindowInternal.h>
#include <nsIIdleService.h>
#include <nsIPromptService.h>
#include <nsIPrefBranch2.h>
#include <nsIPrefService.h>
#include <nsIStringBundle.h>
#include <nsISupportsPrimitives.h>
#include <nsIWindowMediator.h>
#include <nsIXULAppInfo.h>
#include <nsComponentManagerUtils.h>
#include <nsDirectoryServiceUtils.h>
#include <nsAppDirectoryServiceDefs.h>
#include <nsDirectoryServiceDefs.h>

#define PURPLE_PROTOCOL_PLUGIN_CATEGORY   "purple-protocol-plugin"

#define DEBUG_PURPLE_CONSOLE
#ifdef DEBUG_PURPLE_CONSOLE
# include <nsIConsoleService.h>
#endif

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleInit:5
//
static PRLogModuleInfo *gPurpleInitLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleInitLog, PR_LOG_DEBUG, args)

static PurpleEventLoopUiOps eventloops =
{
  purpleTimer::AddTimeout, /* hack to prevent from writting .xml files to disk */
  purpleTimer::CancelTimer,
  purpleSocketWatcher::AddWatch,
  purpleSocketWatcher::CancelWatch,
  NULL,

  /* padding */
  NULL, NULL, NULL, NULL
};
/*** End of the eventloop functions. ***/

/*** Connection uiops ***/
#define IMPL_GET_ACCOUNT                                        \
  PurpleAccount *pAccount = purple_connection_get_account(gc);  \
  NS_ENSURE_TRUE(pAccount,);                                    \
  purpleAccount *account = (purpleAccount *)pAccount->ui_data;  \
  NS_ENSURE_TRUE(account,)

static void connect_progress(PurpleConnection *gc, const char *text,
                             size_t step, size_t step_count)
{
  IMPL_GET_ACCOUNT;
  account->SetConnectionStateMsg(text);

  nsCOMPtr<purpleCoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->NotifyObservers(account, "account-connect-progress", nsnull);
}

static void connected(PurpleConnection *gc)
{
  IMPL_GET_ACCOUNT;
  account->Connected();
}

static void report_disconnect_reason(PurpleConnection *gc,
                                     PurpleConnectionError reason,
                                     const char *text)
{
  IMPL_GET_ACCOUNT;
  account->SetDisconnectReason(reason, text);

  nsCOMPtr<purpleCoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->NotifyObservers(account, "account-connect-error", nsnull);
}

PurpleConnectionUiOps connection_uiops = {
  connect_progress,
  connected,
  NULL, /* disconnected */
  NULL, /* notice = dead code ? */
  NULL, /* report_disconnect: deprecated in favour of report_disconnect_reason */
  NULL, /* network_connected */
  NULL, /* network_disconnected */
  report_disconnect_reason,

  NULL, NULL, NULL
};

/*** Account uiops ***/
#pragma GCC visibility push(default)
typedef void (*GCallback) (void);
#pragma GCC visibility pop

static void *request_authorize(PurpleAccount *account, const char *remote_user,
                               const char *id, const char *alias,
                               const char *message, gboolean on_list, 
                               PurpleAccountRequestAuthorizationCb authorize_cb,
                               PurpleAccountRequestAuthorizationCb deny_cb,
                               void *user_data)
{
  nsresult rv;
  nsCOMPtr<nsIPromptService> promptService =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, NULL);

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, NULL);

  nsCOMPtr<nsIStringBundle> stringBundle;
  rv = bundleService->CreateBundle("chrome://instantbird/locale/accounts.properties",
                                   getter_AddRefs(stringBundle));
  NS_ENSURE_SUCCESS(rv, NULL);

  nsString title;
  rv = stringBundle->GetStringFromName(NS_LITERAL_STRING("requestAuthorizeTitle").get(),
                                       getter_Copies(title));
  NS_ENSURE_SUCCESS(rv, NULL);

  nsString allow;
  rv = stringBundle->GetStringFromName(NS_LITERAL_STRING("requestAuthorizeAllow").get(),
                                       getter_Copies(allow));
  NS_ENSURE_SUCCESS(rv, NULL);

  nsString deny;
  rv = stringBundle->GetStringFromName(NS_LITERAL_STRING("requestAuthorizeDeny").get(),
                                       getter_Copies(deny));
  NS_ENSURE_SUCCESS(rv, NULL);

  nsCString cRemoteUser(remote_user);
  nsString who;
  NS_CStringToUTF16(cRemoteUser, NS_CSTRING_ENCODING_UTF8, who);
  const PRUnichar *whoFlat = who.get();

  nsString msg;
  rv = stringBundle->FormatStringFromName(NS_LITERAL_STRING("requestAuthorizeText").get(),
                                          &whoFlat, 1, getter_Copies(msg));

  nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  NS_ENSURE_TRUE(wm, NULL);
  nsCOMPtr<nsIDOMWindowInternal> window;
  wm->GetMostRecentWindow(NS_LITERAL_STRING("Messenger:blist").get(),
			  getter_AddRefs(window));

  PRInt32 result;
  PRUint32 flags = (nsIPromptService::BUTTON_POS_0 +
                    nsIPromptService::BUTTON_POS_1) *
                   nsIPromptService::BUTTON_TITLE_IS_STRING;
  promptService->ConfirmEx(window, title.get(), msg.get(), flags, allow.get(),
                           deny.get(), nsnull, nsnull, nsnull, &result);
  NS_ENSURE_SUCCESS(rv, NULL);

  if (result) {
    // Index 1 means "deny"
    deny_cb(user_data);
  }
  else {
    authorize_cb(user_data);
    //FIXME Prompt to add to blist
  }

  return NULL;
}

PurpleAccountUiOps account_uiops = {
  NULL, /* notify_added, nearly dead code ? */
  NULL, /* status_changed */
  NULL, /* request_add, nearly dead */
  request_authorize,
  NULL, /* close_account_request --> FIXME */

  NULL, NULL, NULL, NULL
};

/*** Conversation uiops ***/
static void create_conv(PurpleConversation *aConv)
{
  LOG(("Create conversation with name: %s", purple_conversation_get_name(aConv)));

  nsCOMPtr<purpleConversation> conv;
  PurpleConversationType type = purple_conversation_get_type(aConv);
  const char *contractid = nsnull;
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

  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->AddConversation(conv);
}

static void destroy_conv(PurpleConversation *aConv)
{
  LOG(("destroy_conv uiop called"));

  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->RemoveConversation(aConv);
}

static void notify_conv(PurpleConversation *aConv, nsISupports *aSubject,
                        const char *aTopic, const PRUnichar *aData)
{
  NS_ENSURE_TRUE(aConv,);

  nsCOMPtr<purpleConversation> conversation =
    static_cast<purpleConversation *>(aConv->ui_data);
  NS_ENSURE_TRUE(conversation,);

  conversation->NotifyObservers(aSubject, aTopic, aData);
}

static void write_conv(PurpleConversation *conv, const char *who,
                       const char *alias, const char *message,
                       PurpleMessageFlags flags, time_t mtime)
{
  nsresult rv;
  nsCOMPtr<purpleMessage> msg = do_CreateInstance(PURPLE_MESSAGE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,);

  msg->Init(conv, who, alias, message, flags, mtime);

  nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID, &rv);
  pcs->NotifyObservers(msg, "new-text", nsnull);

  notify_conv(conv, msg, "new-text", nsnull);
}

static void conv_add_chat_users(PurpleConversation *conv,
                                GList *cbuddies,
                                gboolean new_arrivals)
{
  while (cbuddies) {
    nsCOMPtr<purpleConvChatBuddy> buddy = new purpleConvChatBuddy();
    buddy->Init((PurpleConvChatBuddy *)cbuddies->data);
    notify_conv(conv, buddy, "chat-buddy-add", nsnull);
    cbuddies = cbuddies->next;
  }
}

static void conv_remove_chat_users(PurpleConversation *conv,
                                   GList *users)
{
  while (users) {
    notify_conv(conv, nsnull, "chat-buddy-remove",
                NS_ConvertUTF8toUTF16((const char *)users->data).get());
    users = users->next;
  }
}

static void conv_update_chat_usr(PurpleConversation *conv, const char *user,
                                 const PRUnichar *aData = nsnull)
{
  PurpleConvChatBuddy *prplBuddy = purple_conv_chat_cb_find(PURPLE_CONV_CHAT(conv), user);
  NS_ENSURE_TRUE(prplBuddy,);

  nsCOMPtr<purpleConvChatBuddy> buddy = new purpleConvChatBuddy();
  buddy->Init(prplBuddy);
  notify_conv(conv, buddy, "chat-buddy-update", aData);
}

static void conv_update_chat_user(PurpleConversation *conv, const char *user)
{
  conv_update_chat_usr(conv, user, nsnull);
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

static void account_signals(PurpleAccount *account, const char *signal)
{
  LOG(("%s: %s (%s)\n", signal,
       account->username, account->protocol_id));

  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->FireAccountEvent(account, signal);
}

static void connection_signals(PurpleConnection *gc, const char *signal)
{
  account_signals(purple_connection_get_account(gc), signal);
}

#define PURPLE_CONNECT_CONNECTION_SIGNAL(aSignal, aName)        \
  purple_signal_connect(instance, aSignal, &handle,             \
                        PURPLE_CALLBACK(connection_signals),    \
                        (void *)aName)

static void
connect_to_connections_signals()
{
  int handle;
  void *instance = purple_connections_get_handle();
  PURPLE_CONNECT_CONNECTION_SIGNAL("signed-on", "account-connected");
  PURPLE_CONNECT_CONNECTION_SIGNAL("signing-on", "account-signing-on");
  PURPLE_CONNECT_CONNECTION_SIGNAL("signed-off", "account-disconnected");
  PURPLE_CONNECT_CONNECTION_SIGNAL("signing-off", "account-disconnecting");
  LOG(("Connecting to connections signals"));
}

static void
connect_to_accounts_signals()
{
  int handle;
  void *instance = purple_accounts_get_handle();
  purple_signal_connect(instance, "account-connecting", &handle,
                        PURPLE_CALLBACK(account_signals),
                        (void *)"account-connecting");

  LOG(("Connecting to accounts signals"));
}

static void buddy_signals(PurpleBuddy *aBuddy, const char *aSignal)
{
  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->FireBuddyEvent(aBuddy, aSignal);
}

#define PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER(aSignal, aHandler)  \
  purple_signal_connect(instance, aSignal, &handle,             \
                        PURPLE_CALLBACK(aHandler),              \
                        (void *)aSignal)

#define PURPLE_CONNECT_BUDDY_SIGNAL(aSignal)                    \
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER(aSignal, buddy_signals)

static void buddy_removed(PurpleBuddy *buddy, void *null)
{
  // This is the buddy-removed purple signal. It is fired when a buddy
  // is removed permanenty (ie from the server list or if the user
  // request the deletion of a buddy in her list)

  // send the signal first, before actually removing the buddy
  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->FireBuddyEvent(buddy, "buddy-deleted");
  pcs->PurpleBuddyRemoved(buddy);
}

static void buddy_away(PurpleBuddy *aBuddy,
                       PurpleStatus *old_status, PurpleStatus *status)
{
  buddy_signals(aBuddy, "buddy-away");
}

static void buddy_idle(PurpleBuddy *aBuddy, gboolean old_idle, gboolean idle)
{
  buddy_signals(aBuddy, "buddy-idle");
}

void
connect_to_blist_signals()
{
  int handle;
  void *instance = purple_blist_get_handle();
  PURPLE_CONNECT_BUDDY_SIGNAL("buddy-added");
  PURPLE_CONNECT_BUDDY_SIGNAL("buddy-signed-on");
  PURPLE_CONNECT_BUDDY_SIGNAL("buddy-signed-off");
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-removed", buddy_removed);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-status-changed", buddy_away);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-idle-changed", buddy_idle);

  LOG(("Connecting to blist signals"));
}

static void remove_blist_node(PurpleBuddyList *aList, PurpleBlistNode *aNode)
{
  // This is the removed blist uiop. It is fired when a possibly
  // visible element of the buddy list is removed (because the account
  // got disconnected)

  // For now, we are only interested by buddy removal
  if (!PURPLE_BLIST_NODE_IS_BUDDY(aNode))
    return;

  PurpleBuddy *buddy = (PurpleBuddy *) aNode;
  LOG(("purple uiop : remove_blist_node, name = %s", buddy->name));
  buddy_signals(buddy, "buddy-removed");
}

static PurpleBlistUiOps blist_uiops = {
  NULL, /* new_list */
  NULL, /* new_node FIXME: should use it */
  NULL, /* show */
  NULL, /* update */
  remove_blist_node, /* remove */
  NULL, /* destroy */
  NULL, /* set_visible */
  NULL, /* request_add_buddy */
  NULL, /* request_add_chat */
  NULL, /* request_add_group */

  NULL, NULL, NULL, NULL
};


static void
received_im_msg(PurpleAccount *account, char *sender, char *message,
                PurpleConversation *conv, PurpleMessageFlags flags)
{
  LOG(("Received_im_msg"));

  nsCOMPtr<nsIObserverService> os = do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  NS_ASSERTION(os, "nsCOMPtr<nsIObserverService> == null");

  if (!conv) {
    conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, sender);
  }
  purpleIConversation *prplConv = (purpleIConversation *) conv->ui_data;
  NS_ASSERTION(prplConv, "received IM for conversation without purpleConversation");

  //FIXME Why is this different on Windows?
#ifdef XP_WIN
  os->NotifyObservers(prplConv, "new message", nsnull);
#else
  const nsEmbedCString cMsg(message);
  nsEmbedString        msg;

  NS_CStringToUTF16(cMsg, NS_CSTRING_ENCODING_UTF8, msg);
  os->NotifyObservers(prplConv, "new message", msg.get());
#endif
}

static void update_chat_topic(PurpleConversation *aConv,
                              const char *aWho,
       	                      const char *aNew)
{
  notify_conv(aConv, nsnull, "chat-update-topic", nsnull);
}

static void conversation_updated(PurpleConversation *aConv, PurpleConvUpdateType aType)
{
  if (aType == PURPLE_CONV_UPDATE_TYPING)
    notify_conv(aConv, nsnull, "update-typing", nsnull);
}

static void connect_to_conversations_signals()
{
  static int handle;
  purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", &handle,
                        PURPLE_CALLBACK(received_im_msg), NULL);

  purple_signal_connect(purple_conversations_get_handle(), "chat-topic-changed", &handle,
                        PURPLE_CALLBACK(update_chat_topic), NULL);

  purple_signal_connect(purple_conversations_get_handle(), "conversation-updated", &handle,
                        PURPLE_CALLBACK(conversation_updated), NULL);

  LOG(("Connecting to conversation signals"));
}

#ifdef DEBUG_PURPLE_CONSOLE
static void debug_print(PurpleDebugLevel level, const char *category,
                        const char *arg)
{
  LOG(("pp-%s: %s", category, arg));
  nsCOMPtr<nsIConsoleService> consoleService = 
    do_GetService( "@mozilla.org/consoleservice;1" );

  nsCString message(category);
  message.Append(": ");
  message.Append(arg);

  nsString msg;
  NS_CStringToUTF16(message, NS_CSTRING_ENCODING_UTF8, msg);

  consoleService->LogStringMessage(msg.get());
}

static gboolean debug_enabled(PurpleDebugLevel level,
                              const char *category)
{
  return true;
}

static PurpleDebugUiOps debugconsoleuiops = {
  debug_print,
  debug_enabled,
  NULL,NULL,NULL,NULL
};
#endif

static PurpleDnsQueryUiOps dnsUiOps = {
  purpleDNS::Resolve,
  purpleDNS::Cancel,

  NULL,NULL,NULL,NULL
};

// Taken from pidgin/gtkconv.c (GPL, ...)
static PurpleCmdRet say_command_cb(PurpleConversation *conv,
                                   const char *cmd, char **args,
                                   char **error, void *data)
{
  if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
    purple_conv_im_send(PURPLE_CONV_IM(conv), args[0]);
  else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
    purple_conv_chat_send(PURPLE_CONV_CHAT(conv), args[0]);

  return PURPLE_CMD_RET_OK;
}


static PurpleCmdRet away_command_cb(PurpleConversation *conv,
                                    const char *cmd, char **args,
                                    char **error, void *data)
{
  nsCString message(args[0]);

  nsCOMPtr<purpleCoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->Away(message);

  return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet back_command_cb(PurpleConversation *conv,
                                    const char *cmd, char **args,
                                    char **error, void *data)
{
  nsCString message(args[0]);

  nsCOMPtr<purpleCoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->Back(message);

  return PURPLE_CMD_RET_OK;
}


// Idle ui ops
static time_t idle_time()
{
  nsCOMPtr<nsIIdleService> idleService =
    do_GetService("@mozilla.org/widget/idleservice;1");
  NS_ENSURE_TRUE(idleService, 0);

  PRUint32 idleTime;
  nsresult rv = idleService->GetIdleTime(&idleTime);
  NS_ENSURE_SUCCESS(rv, 0);

  LOG(("IdleTime is %u seconds\n", idleTime / 1000));
  return idleTime / 1000;
}

static PurpleIdleUiOps idle_uiops =
{
  idle_time,
 
  /* padding */
  NULL, NULL, NULL, NULL
};

// GetText ui ops
PurpleGetTextUiOps gettext_uiops = {
  purpleGetText::GetText,
  purpleGetText::GetPluralText,

  NULL, NULL, NULL, NULL
};

class prefsObserver : public nsIObserver
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    prefsObserver(void *aCallback) : callback(aCallback) {}

  private:
    gpointer callback;
};

NS_IMPL_ISUPPORTS1(prefsObserver, nsIObserver)

NS_IMETHODIMP prefsObserver::Observe(nsISupports *aSubject,
				     const char *aTopic,
				     const PRUnichar *someData)
{
  LOG(("prefsObserver::Observe topic = %s\n", aTopic));
  purple_prefs_observe(callback);

  return NS_OK;
}

// Prefs ui ops
#define DEFINE_mozName_from_name(name)		\
  NS_ENSURE_TRUE(name && *name == '/',);	\
						\
  char *mozName = g_strdup(name + 1);		\
  for (int i = 0; mozName[i]; ++i)		\
    if (mozName[i] == '/')			\
      mozName[i] = '.';

#define DEFINE_mozName_from_name_with_default(name, default)	\
  NS_ENSURE_TRUE(name && *name == '/', default);		\
  								\
  char *mozName = g_strdup(name + 1);				\
  for (int i = 0; mozName[i]; ++i)				\
    if (mozName[i] == '/')					\
      mozName[i] = '.';

void prefs_add_none(const char *name)
{
  LOG(("add pref none %s\n", name));
}

void prefs_add_bool(const char *name, gboolean value)
{
  LOG(("add pref bool %s=%i\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);

  if (type == nsIPrefBranch::PREF_INVALID) {
#ifdef DEBUG
    printf("pref(\"%s\", %s);\n", mozName, value ? "true" : "false");
#endif
    prefs->SetBoolPref(mozName, value);
  }
}

void prefs_add_int(const char *name, int value)
{
  LOG(("add pref int %s=%i\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);

  if (type == nsIPrefBranch::PREF_INVALID) {
#ifdef DEBUG
    printf("pref(\"%s\", %i);\n", mozName, value);
#endif
    prefs->SetIntPref(mozName, value);
  }
}

void prefs_add_string(const char *name, const char *value)
{
  LOG(("add pref string %s=%s\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);

  if (type == nsIPrefBranch::PREF_INVALID) {
#ifdef DEBUG
    printf("pref(\"%s\", \"%s\");\n", mozName, value);
#endif
    prefs->SetCharPref(mozName, value);
  }
}

void prefs_remove(const char *name)
{
  LOG(("remove pref %s\n", name));
  NS_ENSURE_TRUE(name && *name, );
  DEFINE_mozName_from_name(name);
  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->DeleteBranch(name);
}

void prefs_set_bool(const char *name, gboolean value)
{
  LOG(("set bool pref %s=%i\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->SetBoolPref(mozName, value);
}

void prefs_set_int(const char *name, int value)
{
  LOG(("set int pref %s=%i\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->SetIntPref(mozName, value);
}

void prefs_set_string(const char *name, const char *value)
{
  LOG(("set string pref %s=%s\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->SetCharPref(mozName, value);
}


gboolean prefs_exists(const char *name)
{
  LOG(("pref exists *%s\n", name));
  DEFINE_mozName_from_name_with_default(name, false);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);

  return type != nsIPrefBranch::PREF_INVALID;
}

PurplePrefType prefs_get_type(const char *name)
{
  LOG(("pref get type %s\n", name));
  DEFINE_mozName_from_name_with_default(name, PURPLE_PREF_NONE);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);

  switch (type) {
    case nsIPrefBranch::PREF_INT:
      return PURPLE_PREF_INT;
    case nsIPrefBranch::PREF_BOOL:
      return PURPLE_PREF_BOOLEAN;
    case nsIPrefBranch::PREF_STRING:
      return PURPLE_PREF_STRING;
    default:
      return PURPLE_PREF_NONE;
  }
}


gboolean prefs_get_bool(const char *name)
{
  LOG(("get bool pref %s\n", name));
  DEFINE_mozName_from_name_with_default(name, PR_FALSE);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRBool result = PR_FALSE;
  prefs->GetBoolPref(mozName, &result);
  return result;
}

int prefs_get_int(const char *name)
{
  LOG(("get int pref %s\n", name));
  DEFINE_mozName_from_name_with_default(name, 0);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 result = 0;
  prefs->GetIntPref(mozName, &result);
  return result;
}

const char *prefs_get_string(const char *name)
{
  LOG(("get string pref %s\n", name));
  DEFINE_mozName_from_name_with_default(name, 0);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  char *result = NULL;
  prefs->GetCharPref(mozName, &result);

  purple_debug_warning("prefs", "leaking the value of %s\n", name);
  return result;
}

GList *prefs_get_children_names(const char *name)
{
  LOG(("prefs get children name %s\n", name));
  purple_debug_error("prefs", "call to get_children_names, not implemented yet\n");

  return NULL;
}

void *prefs_add_observer(const char *name, gpointer data)
{
  LOG(("prefs add observer %s\n", name));
  DEFINE_mozName_from_name_with_default(name, NULL);

  prefsObserver *observer = new prefsObserver(data);
  nsCOMPtr<nsIPrefBranch2> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, NULL);
  prefs->AddObserver(mozName, observer, false);

  return (void *)observer;
}

void prefs_remove_observer(const char *name, void *observer)
{
  LOG(("remove observer @%x\n", observer));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch2> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, );
  prefs->RemoveObserver(mozName, (nsIObserver *)observer);
}

void prefs_save()
{
  LOG(("prefs save\n"));
  nsCOMPtr<nsIPrefService> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, );
  prefs->SavePrefFile(nsnull);
}

PurplePrefsUiOps prefs_uiops = {
  prefs_add_none,
  prefs_add_bool,
  prefs_add_int,
  prefs_add_string,

  prefs_set_bool,
  prefs_set_int,
  prefs_set_string,

  prefs_get_bool,
  prefs_get_int,
  prefs_get_string,

  prefs_get_type,
  prefs_get_children_names,

  prefs_exists,
  prefs_remove,

  prefs_save,

  prefs_add_observer,
  prefs_remove_observer,

  NULL, NULL, NULL, NULL
};

// Core ui ops (ui info)
static GHashTable *ui_info = NULL;
static nsCString *ui_name = NULL;
static nsCString *ui_version = NULL;

static void ui_quit()
{
  if (ui_info) {
    g_hash_table_destroy(ui_info);
    delete ui_name;
    delete ui_version;
  }
}

static GHashTable *ui_get_info(void)
{
  if (!ui_info) {
    ui_info = g_hash_table_new(g_str_hash, g_str_equal);

    nsCOMPtr<nsIXULAppInfo> xai =
      do_GetService("@mozilla.org/xre/app-info;1");

    // Ugly, ugly, ugly...
    ui_name = new nsCString();
    if (xai && NS_SUCCEEDED(xai->GetName(*ui_name)))
      g_hash_table_insert(ui_info, const_cast<char *>("name"),
                          const_cast<char *>(ui_name->get()));
    else
      g_hash_table_insert(ui_info, const_cast<char *>("name"),
                          const_cast<char *>("Instantbird"));

    ui_version = new nsCString();
    if (xai && NS_SUCCEEDED(xai->GetVersion(*ui_version)))
      g_hash_table_insert(ui_info, const_cast<char *>("version"),
                          const_cast<char *>(ui_version->get()));
    else
      g_hash_table_insert(ui_info, const_cast<char *>("version"),
                          const_cast<char *>("Unknown"));

    g_hash_table_insert(ui_info, const_cast<char *>("website"),
                        const_cast<char *>("http://www.instantbird.com"));
    g_hash_table_insert(ui_info, const_cast<char *>("dev_website"),
                        const_cast<char *>("http://www.instantbird.org"));
  }

  return ui_info;
}

static void ui_register_plugins()
{
  LOG(("ui_register_plugins"));

  nsCOMPtr<nsICategoryManager> catMgr =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(catMgr, );

  // Get contract IDs of all purple protocol plugin registrated as
  // XPCOM components and register them with libpurple
  nsCOMPtr<nsISimpleEnumerator> catEntries;
  nsresult rv = catMgr->EnumerateCategory(PURPLE_PROTOCOL_PLUGIN_CATEGORY,
					  getter_AddRefs(catEntries));
  NS_ENSURE_SUCCESS(rv, );

  PRBool hasMore;
  while (NS_SUCCEEDED(catEntries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    rv = catEntries->GetNext(getter_AddRefs(elem));
    NS_ENSURE_SUCCESS(rv, );

    nsCOMPtr<nsISupportsCString> entry = do_QueryInterface(elem, &rv);
    NS_ENSURE_SUCCESS(rv, );

    nsCString contractId;
    rv = entry->GetData(contractId);
    NS_ENSURE_SUCCESS(rv, );

    LOG(("Attempting to register %s", contractId.get()));
    nsCOMPtr<purpleIPlugin> plugin = do_CreateInstance(contractId.get());
    if (!plugin) {
      NS_WARNING("Failed attempt to register a protocol plugin");
      continue;
    }

    plugin->RegisterSelf();
  }
}

static PurpleCoreUiOps core_uiops =
{
  NULL, NULL, NULL,
  ui_quit,
  ui_get_info,
  ui_register_plugins,

  /* padding */
  NULL, NULL, NULL
};

nsresult
init_libpurple()
{
  /* First initialize the translation stuff, it can probably be
     needed early be potential error messages */
  purple_gettext_set_ui_ops(&gettext_uiops);

  purple_prefs_set_ui_ops(&prefs_uiops);

  /* Set the instantbird profile folder as libpurple custom user
     directory */

  nsCOMPtr<nsIFile> folder;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);
  nsString path;
  rv = folder->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  if (!gPurpleInitLog)
    gPurpleInitLog = PR_NewLogModule("purpleInit");
#endif

  nsCString cPath;
  CopyUTF16toUTF8(path, cPath);
  LOG(("Setting custom user directory to:  %s", cPath.get()));
  purple_util_set_user_dir(cPath.get());

  purple_debug_set_enabled(FALSE);

  purple_core_set_ui_ops(&core_uiops);
  purple_conversations_set_ui_ops(&conversation_uiops);
  purple_connections_set_ui_ops(&connection_uiops);
  purple_accounts_set_ui_ops(&account_uiops);
  purple_blist_set_ui_ops(&blist_uiops);
  purple_idle_set_ui_ops(&idle_uiops);

#ifdef DEBUG_PURPLE_CONSOLE
  purple_debug_set_ui_ops(&debugconsoleuiops);
#endif

  purple_dnsquery_set_ui_ops(&dnsUiOps);

  /* The eventloop uiops use nsTimer and
   * nsSocketTransportService. purpleTimer and purpleSocket are proxy
   * so that we can have the right prototypes for libpurple.
   */
  purple_eventloop_set_ui_ops(&eventloops);

  /* Here "instantbird" stands for the UI_ID. libpurple
   * sometimes need this id (for example for some plugins and
   * some preferences)
   */
  gboolean purpleCoreInit = purple_core_init("instantbird");
  NS_ASSERTION(purpleCoreInit, "purple_core_init failed!!!");
  NS_ENSURE_TRUE(purpleCoreInit, NS_ERROR_FAILURE);

  /* Enable all logs by default. */
  purple_prefs_set_bool("/purple/logging/log_ims", TRUE);
  purple_prefs_set_bool("/purple/logging/log_chats", TRUE);
  purple_prefs_set_bool("/purple/logging/log_system", TRUE);

  /* Load the pounces. */
  purple_pounces_load();

  /* Now, to connect the account(s), create a status and activate it. */
  PurpleSavedStatus *status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
  purple_savedstatus_activate(status);

  connect_to_connections_signals();
  connect_to_accounts_signals();
  //  connect_to_blist_signals();
  connect_to_conversations_signals();

  purple_cmd_register("say", "S", PURPLE_CMD_P_DEFAULT,
                      (PurpleCmdFlag) (PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM),
                      NULL, say_command_cb,
                      "say &lt;message&gt;:  Send a message normally as if you weren't using a command.",
                      NULL);
  purple_cmd_register("away", "S", PURPLE_CMD_P_DEFAULT,
                      (PurpleCmdFlag) (PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS),
                      NULL, away_command_cb,
                      "away &lt;message&gt;:  Get Away and use |message| as status message.",
                      NULL);
  purple_cmd_register("back", "S", PURPLE_CMD_P_DEFAULT,
                      (PurpleCmdFlag) (PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS),
                      NULL, back_command_cb,
                      "back &lt;message&gt;:  Get Back and use |message| as status message.",
                      NULL);

  return NS_OK;
}
