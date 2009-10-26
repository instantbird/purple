#include "internal.h"

#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "signals.h"
#include "version.h"

#include "plugin.h"
#include "pluginpref.h"
#include "prefs.h"

#define STATENOTIFY_PLUGIN_ID "core-statenotify"

static void
write_status(PurpleBuddy *buddy, const char *message, PurpleConvUpdateType type)
{
	PurpleAccount *account = NULL;
	PurpleConversation *conv;
	const char *who;
	char buf[256];
	char *escaped;
	const gchar *buddy_name = NULL;

	account = purple_buddy_get_account(buddy);
	buddy_name = purple_buddy_get_name(buddy);

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
												 buddy_name, account);

	if (conv == NULL)
		return;
	g_return_if_fail(conv->type == PURPLE_CONV_TYPE_IM);

	/* Prevent duplicate notifications for buddies in multiple groups */
	if (buddy != purple_find_buddy(account, buddy_name))
		return;

	who = purple_buddy_get_alias(buddy);
	escaped = g_markup_escape_text(who, -1);

	g_snprintf(buf, sizeof(buf), message, escaped);
	g_free(escaped);

	purple_conv_im_write(conv->u.im, NULL, buf, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_ACTIVE_ONLY | PURPLE_MESSAGE_NO_LINKIFY, time(NULL));
	purple_conversation_update(conv, type);
}

static void
buddy_status_changed_cb(PurpleBuddy *buddy, PurpleStatus *old_status,
                        PurpleStatus *status, void *data)
{
	gboolean available, old_available;

	if (!purple_status_is_exclusive(status) ||
			!purple_status_is_exclusive(old_status))
		return;

	available = purple_status_is_available(status);
	old_available = purple_status_is_available(old_status);

	if (purple_prefs_get_bool("/plugins/core/statenotify/notify_away")) {
		if (available && !old_available)
			write_status(buddy, _("%s is no longer away."), PURPLE_CONV_UPDATE_AWAY);
		else if (!available && old_available)
			write_status(buddy, _("%s has gone away."), PURPLE_CONV_UPDATE_AWAY);
	}
}

static void
buddy_idle_changed_cb(PurpleBuddy *buddy, gboolean old_idle, gboolean idle,
                      void *data)
{
	if (purple_prefs_get_bool("/plugins/core/statenotify/notify_idle")) {
		if (idle && !old_idle) {
			write_status(buddy, _("%s has become idle."), PURPLE_CONV_UPDATE_AWAY);
		} else if (!idle && old_idle) {
			write_status(buddy, _("%s is no longer idle."), PURPLE_CONV_UPDATE_AWAY);
		}
	}
}

static void
buddy_signon_cb(PurpleBuddy *buddy, void *data)
{
	if (purple_prefs_get_bool("/plugins/core/statenotify/notify_signon"))
		write_status(buddy, _("%s has signed on."), PURPLE_CONV_ACCOUNT_ONLINE);
}

static void
buddy_signoff_cb(PurpleBuddy *buddy, void *data)
{
	if (purple_prefs_get_bool("/plugins/core/statenotify/notify_signon"))
		write_status(buddy, _("%s has signed off."), PURPLE_CONV_ACCOUNT_OFFLINE);
}

static void
account_signed_off_cb(PurpleConnection *gc, gpointer event)
{
	GList *iter;

	if (!gc || !gc->account)
		return;

	for (iter = purple_get_conversations(); iter; iter = iter->next)
	{
		PurpleConversation *conv = iter->data;
		if (purple_conversation_get_account(conv) == gc->account)
		{
			purple_conversation_update(conv, PURPLE_CONV_ACCOUNT_OFFLINE);
			purple_conversation_write(conv, NULL, _("Your account is disconnected."), PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_ACTIVE_ONLY | PURPLE_MESSAGE_NO_LINKIFY, time(NULL));
		}
	}
}

#if 0
static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_label(_("Notify When"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label("/plugins/core/statenotify/notify_away", _("Buddy Goes _Away"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label("/plugins/core/statenotify/notify_idle", _("Buddy Goes _Idle"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label("/plugins/core/statenotify/notify_signon", _("Buddy _Signs On/Off"));
	purple_plugin_pref_frame_add(frame, ppref);

	return frame;
}
#endif

static gboolean
plugin_load(PurplePlugin *plugin)
{
	void *blist_handle = purple_blist_get_handle();

	purple_signal_connect(blist_handle, "buddy-status-changed", plugin,
	                    PURPLE_CALLBACK(buddy_status_changed_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-idle-changed", plugin,
	                    PURPLE_CALLBACK(buddy_idle_changed_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-signed-on", plugin,
	                    PURPLE_CALLBACK(buddy_signon_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-signed-off", plugin,
	                    PURPLE_CALLBACK(buddy_signoff_cb), NULL);

	purple_signal_connect(purple_connections_get_handle(), "signed-off", plugin,
	                      G_CALLBACK(account_signed_off_cb), NULL);

	return TRUE;
}

#if 0
static PurplePluginUiInfo prefs_info =
{
	get_plugin_pref_frame,
	0,   /* page_num (Reserved) */
	NULL, /* frame (Reserved) */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	STATENOTIFY_PLUGIN_ID,                            /**< id             */
	N_("Buddy State Notification"),                   /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Notifies in a conversation window when a buddy goes or returns from "
	   "away or idle."),
	                                                  /**  description    */
	N_("Notifies in a conversation window when a buddy goes or returns from "
	   "away or idle."),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};
#endif 

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/core/statenotify");
	purple_prefs_add_bool("/plugins/core/statenotify/notify_away", TRUE);
	purple_prefs_add_bool("/plugins/core/statenotify/notify_idle", TRUE);
	purple_prefs_add_bool("/plugins/core/statenotify/notify_signon", TRUE);
}

#if 0
PURPLE_INIT_PLUGIN(statenotify, init_plugin, info)
#endif

void purple_statenotify_init(void)
{
	int handle;

	init_plugin(NULL);
	plugin_load((PurplePlugin *)&handle);
}
