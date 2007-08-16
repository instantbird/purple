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
#include <unistd.h>

#include <xpcom-config.h>

#include<nsServiceManagerUtils.h>
#include<nsIObserverService.h>
#include<nsCOMPtr.h>
// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include<nsStringAPI.h>
#include<nsEmbedString.h>
#include"purpleConversation.h"
#include"purpleCoreService.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#define NS_APP_USER_PROFILE_50_DIR              "ProfD"

//#include "defines.h"
//#define DEBUG_PURPLE_INIT
//#define DEBUG_PURPLE

/**
 * The following eventloop functions are used in both pidgin and purple-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _PurpleGLibIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;
} PurpleGLibIOClosure;

static void purple_glib_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
  PurpleGLibIOClosure *closure = (PurpleGLibIOClosure *)data;
  PurpleInputCondition purple_cond = (PurpleInputCondition) 0;

  if (condition & PURPLE_GLIB_READ_COND)
    purple_cond = (PurpleInputCondition) (purple_cond | PURPLE_INPUT_READ);
  if (condition & PURPLE_GLIB_WRITE_COND)
    purple_cond = (PurpleInputCondition) (purple_cond | PURPLE_INPUT_WRITE);

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  purple_cond);

	return TRUE;
}

static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
							   gpointer data)
{
	PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = (GIOCondition) 0;

	closure->function = function;
	closure->data = data;

	if (condition & PURPLE_INPUT_READ)
		cond = (GIOCondition) (cond | PURPLE_GLIB_READ_COND);
	if (condition & PURPLE_INPUT_WRITE)
		cond = (GIOCondition) (cond | PURPLE_GLIB_WRITE_COND);

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      purple_glib_io_invoke, closure, purple_glib_io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

static guint timeout_add(guint interval,
			 GSourceFunc function, gpointer data)
{
  /* FIXME This is an horrible hack to prevent libpurple from writting
     prefs.xml, blist.xml, status.xml and pounces.xml

     Those files are all written with a callback with a timeout of 5
     secondes.

     The only other 5 seconds callback in libpurple is in
     oscar/peers.c and fortunately, it passes a non NULL data pointer.
  */
  if (interval == 5000 && data == NULL) {
    /* assume this is a buddy list, accounts or prefs saving */
#ifdef DEBUG_PURPLE_INIT
    printf("TIMEOUT REFUSED\n");
#endif
    return 0;
  }
#ifdef DEBUG_PURPLE_INIT
  printf("TIMEOUTADD     %i\n", interval);
#endif
  return g_timeout_add(interval, function, data);
}


static PurpleEventLoopUiOps glib_eventloops =
{
	timeout_add, /* hack to prevent from writting .xml files to disk */
	g_source_remove,
	glib_input_add,
	g_source_remove,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
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
  const char *name;
  if (alias && *alias)
    name = alias;
  else if (who && *who)
    name = who;
  else
    name = NULL;


  printf("(%s) %s %s: %s\n", purple_conversation_get_name(conv),
	 purple_utf8_strftime("(%H:%M:%S)", localtime(&mtime)),
	 name, message);

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

  purpleIConversation *prplConv = nsnull;
  rv = pcs->FindConversationById((PRInt32) conv, &prplConv);
  os->NotifyObservers(prplConv, "new text", msg.get());
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
  rv = pcs->FindConversationById((PRInt32) conv, &prplConv);
#ifdef DEBUG_PURPLE_INIT
  if (NS_FAILED(rv))
    printf("FindConversationById failed \n");
  else
    printf("FindConversationById OK \n");
  printf("id : %i\n", (int)conv);
#endif
  os->NotifyObservers(prplConv, "new message", msg.get());
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

nsresult
init_libpurple()
{
  /* Set the instantbird profile folder as libpurple custom user
     directory */

  nsCOMPtr<nsIFile> profileFolder;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
				       getter_AddRefs(profileFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString path;
  rv = profileFolder->GetNativePath(path);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_INIT
  printf("setting custom user directory to:  %s\n", path.get());
#endif
  purple_util_set_user_dir(path.get());

	/* We do not want any debugging for now to keep the noise to a minimum. */
#ifdef DEBUG_PURPLE
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

	/* Set the uiops for the eventloop. If your client is glib-based, you can safely
	 * copy this verbatim. */
	purple_eventloop_set_ui_ops(&glib_eventloops);

	/* Set path to search for plugins. The core (libpurple) takes care of loading the
	 * core-plugins, which includes the protocol-plugins. So it is not essential to add
	 * any path here, but it might be desired, especially for ui-specific plugins. */
	purple_plugins_add_search_path(CUSTOM_PLUGIN_PATH);

#ifdef DEBUG_PURPLE_INIT
	printf("before purple_core_init\n");
#endif
	/* Now that all the essential stuff has been set, let's try to init the core. It's
	 * necessary to provide a non-NULL name for the current ui to the core. This name
	 * is used by stuff that depends on this ui, for example the ui-specific plugins. */
	if (!purple_core_init(UI_ID)) {
		/* Initializing the core failed. Terminate. */
		fprintf(stderr,
				"libpurple initialization failed. Dumping core.\n"
				"Please report this!\n");
		return NS_ERROR_FAILURE;
	}
#ifdef DEBUG_PURPLE_INIT
	printf("after purple_core_init\n");
#endif

 	/* Create and load the buddylist. */
 	purple_set_blist(purple_blist_new());
 	purple_blist_load();

// 	/* Load the preferences. */
// 	purple_prefs_load();

// 	/* Load the desired plugins. The client should save the list of loaded plugins in
// 	 * the preferences using purple_plugins_save_loaded(PLUGIN_SAVE_PREF) */
// 	purple_plugins_load_saved(PLUGIN_SAVE_PREF);

// 	/* Load the pounces. */
// 	purple_pounces_load();


 	/* Now, to connect the account(s), create a status and activate it. */
  	PurpleSavedStatus *status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
  	purple_savedstatus_activate(status);

	connect_to_connections_signals();
	connect_to_accounts_signals();
 	connect_to_signals_for_demonstration_purposes_only();

	return NS_OK;
}
