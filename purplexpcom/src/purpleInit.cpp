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

#ifdef XP_WIN
# define _USE_32BIT_TIME_T
#endif

#pragma GCC visibility push(default)
#include <libpurple/account.h>
#include <libpurple/accountopt.h>
#include <libpurple/cmds.h>
#include <libpurple/conversation.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/eventloop.h>
#include <libpurple/ft.h>
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
#include "purpleMessage.h"
#include "purpleTimer.h"
#include "purpleSockets.h"
#include "purpleDNS.h"
#include <xpcom-config.h>
#include <nsServiceManagerUtils.h>
#include <nsIObserverService.h>
#include <nsCOMPtr.h>
#include <nsStringAPI.h>
#include <nsEmbedString.h>
#include <nsIIdleService.h>
#include <nsIPromptService.h>
#include <nsIStringBundle.h>
#include <nsIXULAppInfo.h>
#include <nsComponentManagerUtils.h>
#include <nsDirectoryServiceUtils.h>
#include <nsAppDirectoryServiceDefs.h>
#include <nsDirectoryServiceDefs.h>

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
static void setAccountConnectionStateMsg(PurpleConnection *gc, const char *text,
                                         const char *aEvent)
{
  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  nsCOMPtr<purpleIAccount> account;
  nsresult rv = pcs->GetAccountByPurpleAccount(purple_connection_get_account(gc),
                                               getter_AddRefs(account));
  NS_ENSURE_SUCCESS(rv,);

  nsCString msg(text);
  account->SetConnectionStateMsg(msg);
  pcs->NotifyObservers(account, aEvent);
}

static void connect_progress(PurpleConnection *gc, const char *text,
                             size_t step, size_t step_count)
{
  setAccountConnectionStateMsg(gc, text, "account-connect-progress");
}

static void report_disconnect(PurpleConnection *gc, const char *text)
{
  setAccountConnectionStateMsg(gc, text, "account-connect-error");
}

PurpleConnectionUiOps connection_uiops = {
  connect_progress,
  NULL, /* connected */
  NULL, /* disconnected */
  NULL, /* notice = dead code ? */
  report_disconnect,
  NULL, /* network_connected */
  NULL, /* network_disconnected */

  NULL, NULL, NULL, NULL
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

  PRInt32 result;
  PRUint32 flags = (nsIPromptService::BUTTON_POS_0 +
                    nsIPromptService::BUTTON_POS_1) *
                   nsIPromptService::BUTTON_TITLE_IS_STRING;
  promptService->ConfirmEx(nsnull, title.get(), msg.get(), flags, allow.get(),
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
  pcs->NotifyObservers(msg, "new-text");

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

static void connect_to_conversations_signals()
{
  static int handle;
  purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", &handle,
                        PURPLE_CALLBACK(received_im_msg), NULL);

  purple_signal_connect(purple_conversations_get_handle(), "chat-topic-changed", &handle,
                        PURPLE_CALLBACK(update_chat_topic), NULL);

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

static PurpleCoreUiOps core_uiops =
{
  NULL, NULL, NULL,
  ui_quit,
  ui_get_info,
 
  /* padding */
  NULL, NULL, NULL
};

nsresult
init_libpurple()
{
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

  /* Add CUSTOM_PLUGIN_PATH as search folder. On some plateform (for
   * example MACOSX), we can't hope that the current working directory
   * of libpurple will be set to a correct folder, so we need to find
   * the absolute path ourselves
   */
  rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                              getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = folder->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF16toUTF8(path, cPath);
  cPath.Append("/" CUSTOM_PLUGIN_PATH);
  purple_plugins_add_search_path(cPath.get());
  LOG(("Setting custom plugin path to:  %s", cPath.get()));

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

  return NS_OK;
}
