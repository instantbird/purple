/*
 * pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#define CUSTOM_PLUGIN_PATH     "libraries/prpl"
#define PLUGIN_SAVE_PREF       "/purple/nullclient/plugins/saved"

#ifdef XP_WIN
# define _USE_32BIT_TIME_T
#endif

#include <libpurple/account.h>
#include <libpurple/accountopt.h>
#include <libpurple/conversation.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/eventloop.h>
#include <libpurple/ft.h>
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

#include <glib.h>

#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <xpcom-config.h>

#include<nsServiceManagerUtils.h>
#include<nsIObserverService.h>
#include<nsCOMPtr.h>

//#define PURPLE_MOZ_DNS

#ifdef PURPLE_MOZ_DNS
# include <libpurple/dnsquery.h>
# include"include/nsIDNSService.h"
# include"include/nsIDNSRecord.h"
# include<prio.h>
#endif
// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include<nsStringAPI.h>
#include<nsEmbedString.h>
#include"purpleConversation.h"
#include"purpleCoreService.h"
#include"purpleBuddy.h"
#include"purpleMessage.h"
#include"purpleTimer.h"
#include"purpleSockets.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#define NS_APP_USER_PROFILE_50_DIR              "ProfD"
#define NS_XPCOM_CURRENT_PROCESS_DIR            "XCurProcD"

//#define DEBUG_PURPLE_INIT
//#define DEBUG_PURPLE_STDERR
//#define DEBUG_PURPLE_CONSOLE
#ifdef DEBUG_PURPLE_CONSOLE
# include "include/nsIConsoleService.h"
#endif

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

/*** Conversation uiops ***/
static void
null_create_conv(PurpleConversation *aConv)
{
#ifdef DEBUG_PURPLE_INIT
  printf("create conversation with name: %s\n", purple_conversation_get_name(aConv));
#endif

  nsCOMPtr<purpleConversation> conv;
  conv = do_CreateInstance("@instantbird.org/purple/conversation;1");

  conv->SetConv(aConv);

  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->AddConversation(conv);
}

static void
null_write_conv(PurpleConversation *conv, const char *who, const char *alias,
		const char *message, PurpleMessageFlags flags, time_t mtime)
{
  nsresult rv;
  nsCOMPtr<purpleMessage> msg = do_CreateInstance(PURPLE_MESSAGE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  msg->Init(conv, who, alias, message, flags, mtime);

  nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID, &rv);
  pcs->NotifyObservers(msg, "new-text");
}

static PurpleConversationUiOps null_conv_uiops =
{
	null_create_conv,          /* create_conversation  */
	NULL,                      /* destroy_conversation */
	NULL,                      /* write_chat           */
	NULL,                      /* write_im             */
	null_write_conv,           /* write_conv           */
	NULL,                      /* chat_add_users       */
	NULL,                      /* chat_rename_user     */
	NULL,                      /* chat_remove_users    */
	NULL,                      /* chat_update_user     */
	NULL,                      /* present              */
	NULL,                      /* has_focus            */
	NULL,                      /* custom_smiley_add    */
	NULL,                      /* custom_smiley_write  */
	NULL,                      /* custom_smiley_close  */
	NULL,                      /* send_confirm         */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
null_ui_init()
{
	/**
	 * This should initialize the UI components for all the modules. Here we
	 * just initialize the UI for conversations.
	 */
	purple_conversations_set_ui_ops(&null_conv_uiops);
}

static PurpleCoreUiOps null_core_uiops =
{
	NULL,
	NULL,
	null_ui_init,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};


static void account_signals(PurpleAccount *account, const char *signal)
{
#ifdef DEBUG_PURPLE_INIT
  printf("%s: %s (%s)\n", signal,
	 account->username, account->protocol_id);
#endif
  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->FireAccountEvent(account, signal);
}

static void connection_signals(PurpleConnection *gc, const char *signal)
{
  account_signals(purple_connection_get_account(gc), signal);
}

#define PURPLE_CONNECT_CONNECTION_SIGNAL(aSignal, aName)	\
  purple_signal_connect(instance, aSignal, &handle,		\
			PURPLE_CALLBACK(connection_signals),	\
			(void *)aName)

static void
connect_to_connections_signals()
{
  static int handle;
  void *instance = purple_connections_get_handle();
  PURPLE_CONNECT_CONNECTION_SIGNAL("signed-on", "account-connected");
  PURPLE_CONNECT_CONNECTION_SIGNAL("signing-on", "account-signing-on");
  PURPLE_CONNECT_CONNECTION_SIGNAL("signed-off", "account-disconnected");
  PURPLE_CONNECT_CONNECTION_SIGNAL("signing-off", "account-disconnecting");
#ifdef DEBUG_PURPLE_INIT
  printf("connecting to connections signals\n");
#endif
}

static void
connect_to_accounts_signals()
{
  static int handle;
  void *instance = purple_accounts_get_handle();
  purple_signal_connect(instance, "account-connecting", &handle,
			PURPLE_CALLBACK(account_signals),
			(void *)"account-connecting");
#ifdef DEBUG_PURPLE_INIT
  printf("connecting to accounts signals\n");
#endif
}

static void buddy_signals(PurpleBuddy *buddy, const char *signal)
{
  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  pcs->FireBuddyEvent(buddy, signal);
}

#define PURPLE_CONNECT_BUDDY_SIGNAL(aSignal)			\
  purple_signal_connect(instance, aSignal, &handle,		\
			PURPLE_CALLBACK(buddy_signals),		\
			(void *)aSignal)

static void
connect_to_blist_signals()
{
  static int handle;
  void *instance = purple_blist_get_handle();
  PURPLE_CONNECT_BUDDY_SIGNAL("buddy-added");
  //  PURPLE_CONNECT_BUDDY_SIGNAL("buddy-removed");
  PURPLE_CONNECT_BUDDY_SIGNAL("buddy-signed-on");
  PURPLE_CONNECT_BUDDY_SIGNAL("buddy-signed-off");
#ifdef DEBUG_PURPLE_INIT
  printf("connecting to blist signals\n");
#endif
}


static void
received_im_msg(PurpleAccount *account, char *sender, char *message,
		PurpleConversation *conv, PurpleMessageFlags flags)
{
#ifdef DEBUG_PURPLE_INIT
  printf("received_im_msg\n");
#endif

  nsresult rv;
  nsCOMPtr<nsIObserverService> os = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    printf("nsCOMPtr<nsIObserverService> == null\n");
    return;
  }

  const nsEmbedCString cMsg(message);
  nsEmbedString        msg;

  NS_CStringToUTF16(cMsg, NS_CSTRING_ENCODING_UTF8, msg);

  nsCOMPtr<purpleICoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID, &rv);

  if (!conv) {
    conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, sender);
  }
  purpleIConversation *prplConv = nsnull;
  rv = pcs->GetConversationById((PRInt32) conv, &prplConv);
#ifdef DEBUG_PURPLE_INIT
  if (NS_FAILED(rv))
    printf("FindConversationById failed \n");
  else
    printf("FindConversationById OK \n");
  printf("id : %i\n", (int)conv);
#endif
#ifdef XP_WIN
  os->NotifyObservers(prplConv, "new message", nsnull);
#else
  os->NotifyObservers(prplConv, "new message", msg.get());
#endif
}

static void
connect_to_signals_for_demonstration_purposes_only()
{
  static int handle;
  purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", &handle,
			PURPLE_CALLBACK(received_im_msg), NULL);
#ifdef DEBUG_PURPLE_INIT
  printf("connecting to signals\n");
#endif
}

#ifdef DEBUG_PURPLE_CONSOLE
static void debug_print(PurpleDebugLevel level, const char *category,
			const char *arg)
{
# ifdef DEBUG_PURPLE_INIT
  printf("pp-%s: %s", category, arg);
# endif
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

#ifdef PURPLE_MOZ_DNS
static gboolean resolve(PurpleDnsQueryData *query_data,
			        PurpleDnsQueryResolvedCallback resolved_cb,
			        PurpleDnsQueryFailedCallback failed_cb)
{
  //FIXME this really shouldn't be blocking!
  nsCString host(purple_dnsquery_get_host(query_data));
#ifdef DEBUG_PURPLE_INIT
  printf("resolving with moz: %s\n", host.get());
#endif
  nsCOMPtr<nsIDNSService> dns = do_GetService("@mozilla.org/network/dns-service;1");
  nsCOMPtr<nsIDNSRecord> record;
  dns->Resolve(host, 0, getter_AddRefs(record));
  GSList *hosts = NULL;
  PRBool more;
  record->HasMore(&more);
  if (!more) {
    failed_cb(query_data, "Not found");
#ifdef DEBUG_PURPLE_INIT
    printf("notfound");
#endif
    return true;
  }
  union PRNetAddr addr;
  while (more)
  {
    record->GetNextAddr(purple_dnsquery_get_port(query_data), &addr);
#ifdef DEBUG_PURPLE_INIT
    printf("One adresse found");
#endif
    hosts = g_slist_append(hosts, GINT_TO_POINTER(sizeof(addr.inet)));
    hosts = g_slist_append(hosts, g_memdup(&addr.inet, sizeof(addr.inet)));
    record->HasMore(&more);
  }
#ifdef DEBUG_PURPLE_INIT
  printf("dns resolution done");
#endif
  resolved_cb(query_data, hosts);
  return true;
}

static PurpleDnsQueryUiOps dnsUiOps = {
  resolve,
  NULL,
  NULL,NULL,NULL,NULL
};
#endif /* PURPLE_MOZ_DNS */

nsresult
init_libpurple()
{
  /* Set the instantbird profile folder as libpurple custom user
     directory */

  nsCOMPtr<nsIFile> folder;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
				       getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString path;
  rv = folder->GetNativePath(path);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_INIT
  printf("setting custom user directory to:  %s\n", path.get());
#endif
  purple_util_set_user_dir(path.get());

#ifdef DEBUG_PURPLE_STDOUT
  purple_debug_set_enabled(TRUE);
#else
  purple_debug_set_enabled(FALSE);
#endif

	/* Set the core-uiops, which is used to
	 * 	- initialize the ui specific preferences.
	 * 	- initialize the debug ui.
	 * 	- initialize the ui components for all the modules.
	 * 	- uninitialize the ui components for all the modules when the core terminates.
	 */
	purple_core_set_ui_ops(&null_core_uiops);

#ifdef DEBUG_PURPLE_CONSOLE
  purple_debug_set_ui_ops(&debugconsoleuiops);
#endif

#ifdef PURPLE_MOZ_DNS
  purple_dnsquery_set_ui_ops(&dnsUiOps);
#endif

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
  rv = folder->GetNativePath(path);
  NS_ENSURE_SUCCESS(rv, rv);
  path.Append("/");
  path.Append(CUSTOM_PLUGIN_PATH);
  purple_plugins_add_search_path(path.get());

  /* Here "instantbird" stands for the UI_ID. libpurple
   * sometimes need this id (for example for some plugins and
   * some preferences)
   */
  if (!purple_core_init("instantbird")) {
#ifdef DEBUG_PURPLE_INIT
    printf("purple_core_init failed!!!\n");
#endif
    return NS_ERROR_FAILURE;
  }

 	/* Create and load the buddylist. */
 	purple_set_blist(purple_blist_new());
 	purple_blist_load();

// 	/* Load the preferences. */
// 	purple_prefs_load();
	purple_prefs_set_bool("/purple/logging/log_ims", TRUE);
	purple_prefs_set_bool("/purple/logging/log_chats", TRUE);
	purple_prefs_set_bool("/purple/logging/log_system", TRUE);

// 	/* Load the pounces. */
// 	purple_pounces_load();


 	/* Now, to connect the account(s), create a status and activate it. */
  	PurpleSavedStatus *status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
  	purple_savedstatus_activate(status);

	connect_to_connections_signals();
	connect_to_accounts_signals();
	connect_to_blist_signals();
 	connect_to_signals_for_demonstration_purposes_only();

	return NS_OK;
}
