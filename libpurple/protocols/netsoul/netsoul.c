/*
 * @file netsoul.c
 *
 * gaim-netsoul Protocol Plugin
 *
 * Copyright (C) 2004, 2007, Edward Hervey <bilboed@gmail.com>
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

#include "netsoul.h"

static PurplePlugin *_netsoul_plugin = NULL;

/*
  netsoul_list_icon
  Returns the name of the icon to use for netsoul
*/

static const char *netsoul_list_icon (PurpleAccount *account, PurpleBuddy *buddy)
{
  return "netsoul";
}

/*
  netsoul_status_text
  Returns the text to display next to the icon in the contact list
*/

static char *netsoul_status_text(PurpleBuddy *gb)
{
  NetsoulBuddy	*nb = (NetsoulBuddy *) gb->proto_data;

  if (!nb)
    return NULL;
  purple_debug_info("netsoul", "status_text %s\n", nb->login);
  if ((nb->state == NS_STATE_AWAY) || (nb->state == NS_STATE_IDLE))
    return g_strdup("Away");
  if (nb->state == NS_STATE_SERVER)
    return g_strdup("Server");
  if (nb->state == NS_STATE_LOCK)
    return g_strdup("Lock");
  if (nb->state == NS_STATE_ACTIF_MORE)
    return g_strdup("Actif+");
  if (nb->state == NS_STATE_SEVERAL_ACTIF)
    return g_strdup("Several Active");
  if (nb->state == NS_STATE_SEVERAL_INACTIF)
    return g_strdup("Away+");
  if (nb->state == NS_STATE_CONNECTION)
    return g_strdup("Connection");
  return NULL;
}

/*
  Returns the description of the given netsoul connection
  used in netsoul_tooltip_text
*/

void netsoul_conn_tooltip(NetsoulConn *nc, PurpleNotifyUserInfo *user_info)
{
  char	*loggedin;
  char	*statetime;
  char	*state;
  char	*state2;

  time_t now = time(NULL);

  loggedin = purple_str_seconds_to_string(now - nc->logintime);
  state = ns_state_to_text(nc->state);
  if (nc->statetime) {
    statetime = purple_str_seconds_to_string(now - nc->statetime);
    state2 = g_strdup_printf("%s (%s)", state, statetime);
    g_free(statetime);
    g_free(state);
  }
  else
    state2 = state;

  purple_notify_user_info_add_pair(user_info, "Location", nc->location);
  purple_notify_user_info_add_pair(user_info, "IP", nc->ip);
  purple_notify_user_info_add_pair(user_info, "Comment", nc->comment);
  purple_notify_user_info_add_pair(user_info, "Logged in", loggedin);
  purple_notify_user_info_add_pair(user_info, "State", state2);

  g_free(loggedin);
  g_free(state2);
}

char *netsoul_conn_text_html(NetsoulConn *nc)
{
  char	*loggedin;
  char	*statetime;
  char	*state;
  char	*state2;
  char	*resp;

  time_t now = time(NULL);

  loggedin = purple_str_seconds_to_string(now - nc->logintime);
  state = ns_state_to_text(nc->state);
  if (nc->statetime) {
    statetime = purple_str_seconds_to_string(now - nc->statetime);
    state2 = g_strdup_printf("%s (%s)", state, statetime);
    g_free(statetime);
    g_free(state);
  }
  else
    state2 = state;

  resp = g_strdup_printf("<b>Location:</b> %s<br><b>IP:</b> %s<br><b>Comment:</b> %s<br><b>Logged in:</b> %s<br><b>State:</b> %s",
			 nc->location, nc->ip, nc->comment, loggedin, state2);
  g_free(loggedin);
  g_free(state2);
  return resp;
}


/*
  netsoul_tooltip_text
  Returns the content of the tooltip for the given buddy
*/

static void netsoul_tooltip_text(PurpleBuddy *gb, PurpleNotifyUserInfo *user_info, gboolean full)
{
  NetsoulBuddy	*nb = (NetsoulBuddy *)gb->proto_data;
  GList	*tmp;
  PurpleConnection *gc = purple_account_get_connection (purple_buddy_get_account(gb));
  NetsoulData *ns = (NetsoulData *)gc->proto_data;
  PurpleBuddyIcon *icon = purple_buddy_get_icon (gb);
  purple_debug_info("netsoul", "netsoul_tooltip_text");

  if (icon != NULL)
    purple_debug_info("netsoul", "netsoul_tooltip_text %s icon_type: %s\n",
		      gb->name, purple_buddy_icon_get_extension(icon));
  if (nb == NULL)
  {
    nb = g_new0(NetsoulBuddy, 1);
    gb->proto_data = nb;
    nb->login = g_strdup(gb->name);
    ns_watch_buddy(gc, gb);
    /* watch_log_user */
    ns_watch_log_user(gc);
    ns_list_users(gc, ns->watchlist);
  }
  if (nb->nblocations == 0)
    return;

  purple_debug_info("netsoul", "netsoul_tooltip_text nblocation != 0\n");
  purple_notify_user_info_add_pair(user_info, "Status", ns_state_to_text(nb->state));
  purple_notify_user_info_add_pair(user_info, "Group", nb->group);

  if (full) {
    purple_notify_user_info_add_section_break(user_info);
    purple_notify_user_info_add_section_header(user_info, "Connections");

    for (tmp = nb->locationlist; tmp; tmp = tmp->next) {
      purple_notify_user_info_add_section_break(user_info);
      netsoul_conn_tooltip((NetsoulConn *)tmp->data, user_info);
    }
  }
}

/*
  netsoul_away_states
  Returns the list of possible away states
*/

static GList * netsoul_away_states (PurpleAccount* account)
{
  GList	*types;
  PurpleStatusType* status;

  types = NULL;
  status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE,
				     NULL, NULL, TRUE, TRUE, FALSE);
  types = g_list_append(types, status);
  status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE,
				     "actif", NULL, FALSE, TRUE, FALSE);
  types = g_list_append(types, status);

  status = purple_status_type_new_full(PURPLE_STATUS_AWAY,
				     "away", NULL, FALSE, TRUE, FALSE);
  types = g_list_append(types, status);

  status = purple_status_type_new_full(PURPLE_STATUS_AWAY,
				     "lock", NULL, FALSE, TRUE, FALSE);
  types = g_list_append(types, status);

  status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE,
				     NULL, NULL, TRUE, TRUE, FALSE);
  types = g_list_append(types, status);


  return (types);
}

/*
  netsoul_set_away
  Sets the account in away mode
*/

static void netsoul_set_away(PurpleAccount *account, PurpleStatus* status)
{
  int	ns_state;
  PurplePresence* state = purple_status_get_presence (status);

  if (purple_presence_is_available (state))
    ns_state = NS_STATE_ACTIF;
  else if (purple_presence_is_idle (state))
    ns_state = NS_STATE_IDLE;
  else
    ns_state = NS_STATE_AWAY;
  ns_send_state(purple_account_get_connection (account), ns_state, time(NULL));
}

/*
  netsoul_set_idle
  Set the account to idle
*/

static void netsoul_set_idle(PurpleConnection *gc, int idletime)
{
  purple_debug_info("netsoul", "netsoul_set_idle. idletime:%d\n", idletime);
  if (idletime)
    ns_send_state(gc, NS_STATE_IDLE, idletime);
  else {
    int ns_state = NS_STATE_ACTIF;
    PurpleAccount *account = NULL;
    PurpleStatus *status = NULL;
    PurplePresence* state = NULL;
    if ((account = purple_connection_get_account(gc)) &&
        (status = purple_account_get_active_status(account)) &&
        (state = purple_status_get_presence (status)) &&
        !purple_presence_is_available(state))
      ns_state = NS_STATE_AWAY;
    ns_send_state(gc, ns_state, 0);
  }
}

/*
  netsoul_close
  Closes the account
*/

static void netsoul_close (PurpleConnection *gc)
{
#if 0
  PurpleBlistNode *gnode, *cnode, *bnode;
#endif
  NetsoulData	*ns = (NetsoulData *)gc->proto_data;

  purple_debug_info("netsoul", "netsoul_close\n");

#if 0
  /* seems useless and produce valgrind errors */
  for(gnode = purple_get_blist()->root; gnode; gnode = gnode->next) {
    if(!PURPLE_BLIST_NODE_IS_GROUP(gnode)) continue;
    for(cnode = gnode->child; cnode; cnode = cnode->next) {
      if(!PURPLE_BLIST_NODE_IS_CONTACT(cnode)) continue;
      for(bnode = cnode->child; bnode; bnode = bnode->next) {
	if(!PURPLE_BLIST_NODE_IS_BUDDY(bnode)) continue;
	if(((PurpleBuddy*)bnode)->account == gc->account)
	{
	  PurpleBuddy *buddy = (PurpleBuddy*)bnode;
	  purple_buddy_icon_unref(purple_buddy_get_icon(buddy));
	}
      }
    }
  }
#endif

  g_free(ns->challenge);
  g_free(ns->host);
  closesocket(ns->fd);
  g_free(ns);
  if (gc->inpa)
    purple_input_remove(gc->inpa);
}


/*
 netsoul_got_photo
 */

static void netsoul_got_photo (PurpleUtilFetchUrlData *url, void *user_data,
			       const char *photo, size_t len, const char *error_msg)
{
  PurpleBuddy *gb = (PurpleBuddy *)user_data;
  PurpleAccount *account = purple_buddy_get_account (gb);

  /* Check if connection is still existing */
  PurpleConnection *gc = purple_account_get_connection (account);
  if (gc == NULL)
    return;

  purple_debug_info("netsoul", "netsoul_got_photo (size: %" G_GSIZE_FORMAT ") for %s\n",
		    len,
		    gb->name);

  /* Try to put the photo in , if there's one and is readable */
  if (user_data && photo && len != 0)
  {
    if (strstr(photo, "400 Bad Request")
	|| strstr(photo, "403 Forbidden")
	|| strstr(photo, "404 Not Found"))
      purple_debug_info("netsoul", "netsoul_got_photo: error: %s\n", photo);
    else
    {
      PurpleStoredImage *img = purple_imgstore_add(g_memdup(photo, len), len, NULL);
      PurpleBuddyIcon *icon = purple_buddy_icon_new(account, gb->name,
						    (void *)purple_imgstore_get_data(img),
						    purple_imgstore_get_size(img),
						    NULL);
      purple_buddy_set_icon(gb, icon);
    }
  }
}


/*
  netsoul_add_buddy
  Add the given buddy to the contact list
*/

static void netsoul_add_buddy (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
  NetsoulData *ns = (NetsoulData *)gc->proto_data;
  NetsoulBuddy	*nb;
  gchar		*photo = NULL;

  purple_debug_info("netsoul", "netsoul_add_buddy %s\n", buddy->name);
  nb = g_new0(NetsoulBuddy, 1);
  buddy->proto_data = nb;
  nb->login = g_strdup(buddy->name);
  /* Get photo */
  photo = g_strdup_printf("%s%s", NETSOUL_PHOTO_URL, buddy->name);

  purple_util_fetch_url(photo, TRUE, NULL, FALSE, netsoul_got_photo, buddy);

  /* if contact is not already is watch list, add it */
  ns_watch_buddy(gc, buddy);
  /* watch_log_user */
  ns_watch_log_user(gc);
  ns_list_users(gc, ns->watchlist);
}

/*
  netsoul_add_buddies
  Add the given buddies to the contact list
*/

static void netsoul_add_buddies(PurpleConnection *gc, GList *buddies, GList *groups)
{
  GList	*tmp;
  NetsoulData *ns = (NetsoulData *)gc->proto_data;
  NetsoulBuddy	*nb;
  PurpleBuddy	*gb;

  purple_debug_info("netsoul", "netsoul_add_buddies\n");
  /* for each contact */
  for (tmp = buddies; tmp; tmp = tmp->next) {
  /*   if contact is not already in watch list add it */
    gb = (PurpleBuddy *) tmp->data;
    nb = g_new0(NetsoulBuddy, 1);
    nb->login = g_strdup(gb->name);
    gb->proto_data = nb;
    ns_watch_buddy(gc, gb);
  }
  /* watch_log_user */
  ns_watch_log_user(gc);
  ns_list_users(gc, ns->watchlist);
}


/*
  ns_get_buddies
  Add buddies to watchlist
*/
void netsoul_get_buddies (PurpleConnection* gc)
{
  PurpleBlistNode *gnode, *cnode, *bnode;
  NetsoulData *ns;
  purple_debug_info("netsoul", "ns_get_buddies\n");

  for(gnode = purple_get_blist()->root; gnode; gnode = gnode->next) {
    if(!PURPLE_BLIST_NODE_IS_GROUP(gnode)) continue;
    for(cnode = gnode->child; cnode; cnode = cnode->next) {
      if(!PURPLE_BLIST_NODE_IS_CONTACT(cnode)) continue;
      for(bnode = cnode->child; bnode; bnode = bnode->next) {
	if(!PURPLE_BLIST_NODE_IS_BUDDY(bnode)) continue;
	if(((PurpleBuddy*)bnode)->account == gc->account)
	{
	  PurpleBuddy *buddy = (PurpleBuddy*)bnode;
	  gchar *photo = NULL;
	  NetsoulBuddy *nb;
	  purple_debug_info("netsoul", "netsoul_add_buddy %s\n", buddy->name);

	  nb = g_new0(NetsoulBuddy, 1);
	  buddy->proto_data = nb;
	  nb->login = g_strdup(buddy->name);
	  /* Get photo */
	  photo = g_strdup_printf("%s%s", NETSOUL_PHOTO_URL, buddy->name);

	  purple_util_fetch_url(photo, TRUE, NULL, FALSE, netsoul_got_photo, buddy);

	  /* if contact is not already is watch list, add it */
	  ns_watch_buddy(gc, buddy);
	}
      }
    }
  }
  /* watch_log_user */
  ns = (NetsoulData *)gc->proto_data;
  ns_watch_log_user(gc);
  ns_list_users(gc, ns->watchlist);
}


/*
  netsoul_remove_buddy
  Remove the given buddy from the contact list
*/

static void netsoul_remove_buddy (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
  NetsoulBuddy	*nb;
  NetsoulConn	*nc;
  NetsoulData	*ns = (NetsoulData *)gc->proto_data;
  GList		*tmp;

  purple_debug_info("netsoul", "netsoul_remove_buddy\n");
  nb = (NetsoulBuddy *)buddy->proto_data;
  /* remove buddy from watchlist */
  if ((tmp = g_list_find_custom(ns->watchlist, nb->login, (GCompareFunc) g_ascii_strcasecmp)))
    ns->watchlist = g_list_delete_link(ns->watchlist, tmp);
  g_free(nb->login);
  if (nb->group)
    g_free(nb->group);
  for (tmp = nb->locationlist; tmp; tmp = tmp->next) {
    nc = (NetsoulConn *) tmp->data;
    g_free(nc->ip);
    g_free(nc->location);
    g_free(nc->comment);
  }
  g_list_free(nb->locationlist);
  g_free(nb);
}

/*
  netsoul_send_im
  Send a message to the given person
*/

static int netsoul_send_im (PurpleConnection *gc, const char *who, const char *what, PurpleMessageFlags flags)
{
  purple_debug_info("netsoul", "netsoul_send_im\n");
  ns_msg_user(gc, who, what);
  return 1;
}

static void netsoul_keepalive(PurpleConnection *gc)
{
  NetsoulData	*ns = (NetsoulData *) gc->proto_data;

  if (netsoul_write(ns, "ping\n") < 0) {
    purple_debug_warning("netsoul", "Error sending ping\n");
  }
}

static const char *netsoul_normalize(const PurpleAccount *account,
                                     const char *str)
{
  static char buf[42]; /* 9 should be enough */
  int i;

  g_return_val_if_fail(str != NULL, NULL);

  for (i = 0; str[i] && str[i] != '@' && i < 41; ++i)
    buf[i] = str[i];
  buf[i] = 0;

  return buf;
}

static unsigned netsoul_send_typing(PurpleConnection *gc, const char *name, PurpleTypingState typing)
{
  purple_debug_info("netsoul", "netsoul_send_typing\n");
  ns_send_typing(gc, name, typing);
  return 1;
}

static void netsoul_get_info(PurpleConnection *gc, const char *who)
{
  PurpleBuddy	*gb;
  NetsoulBuddy	*nb;
  char	*primary;
  char	*text;
  char	*title;
  char	**tab;
  int	i;
  GList	*tmp;
  NetsoulConn	*nc;
  PurpleNotifyUserInfo *user_info;

  purple_debug_info("netsoul", "netsoul_get_info %s\n", who);
  if (!(gb = get_good_stored_buddy(gc, (char *) who))) {
    PurpleNotifyUserInfo *user_info;
    user_info = purple_notify_user_info_new();
    purple_notify_user_info_add_pair(user_info,"Error", "No Info about this user!");
    purple_notify_userinfo(gc, who, user_info, NULL, NULL);
    purple_notify_user_info_destroy(user_info);
    return;
  }
  nb = (NetsoulBuddy *)gb->proto_data;

  tab = g_new0(char *, nb->nblocations + 1);
  for (i = 0, tmp = nb->locationlist; tmp; tmp = tmp->next, i++) {
    nc = (NetsoulConn *)tmp->data;
    tab[i] = netsoul_conn_text_html(nc);
  }
  text = g_strjoinv("<br>", tab);
  g_strfreev(tab);

  primary = g_strdup_printf("<b>Status:</b> %s<br><b>Groupe:</b> %s<hr>%s",
			    ns_state_to_text(nb->state), nb->group, text);
  title = g_strdup_printf("Info for %s", who);

  user_info = purple_notify_user_info_new();
  purple_notify_user_info_add_pair(user_info, title, primary);
  purple_notify_userinfo(gc, gb->name, user_info, NULL, NULL);
  purple_notify_user_info_destroy(user_info);
  g_free(primary);
  g_free(text);
}

/*
  netsoul_list_emblem
  Add little emblems on buddy icon
*/

static const char* netsoul_list_emblems(PurpleBuddy *buddy)
{
  NetsoulBuddy	*nb = (NetsoulBuddy *)buddy->proto_data;

  if (!nb)
    return "";
  purple_debug_info("netsoul", "list_emblems %s\n", nb->login);
  if ((nb->state == NS_STATE_AWAY) || (nb->state == NS_STATE_IDLE))
    return "away";
  if (nb->state == NS_STATE_SEVERAL_INACTIF)
    return "extendedaway";
  if ((nb->state == NS_STATE_SERVER) || (nb->state == NS_STATE_LOCK))
    return "secure";
  if ((nb->state == NS_STATE_SEVERAL_ACTIF) || (nb->state == NS_STATE_ACTIF_MORE))
    return "activebuddy";
  return "";
}

/*
** Si on a un clic droit sur un buddy dans la buddy liste,
** on va rajouter quelques champ dans le menu
*/
static GList *netsoul_blist_node_menu(PurpleBlistNode *node)
{
  if (PURPLE_BLIST_NODE_IS_BUDDY(node))
    {
      return ns_buddy_menu((PurpleBuddy *) node);
    }
  else
    return NULL;
}

static void netsoul_join_chat(PurpleConnection *gc, GHashTable *components)
{
   purple_debug_info("netsoul", "join_chat\n");
}

static void netsoul_reject_chat(PurpleConnection *gc, GHashTable *components)
{
  purple_debug_info("netsoul", "reject_chat\n");
}

static void netsoul_chat_invite(PurpleConnection *gc, int id, const char *who, const char *message)
{
  purple_debug_info("netsoul", "chat_invite\n");
}

static int netsoul_chat_send(PurpleConnection *gc, int id, const char *message)
{
  purple_debug_info("netsoul", "chat_send\n");
  return 0;
}

static PurplePluginProtocolInfo prpl_info =
{
    OPT_PROTO_MAIL_CHECK,    /* options          */
    NULL,                           /* user_splits      */
    NULL,                           /* protocol_options */
    {"jpeg", 48, 48, 96, 96, 0, PURPLE_ICON_SCALE_DISPLAY},                 /* icon_spec        */
    netsoul_list_icon,              /* list_icon        */
    netsoul_list_emblems,           /* list_emblems     */
    netsoul_status_text,            /* status_text      */
    netsoul_tooltip_text,           /* tooltip_text     */
    netsoul_away_states,            /* away_states      */
    netsoul_blist_node_menu,        /* blist_node_menu  */
    NULL,                           /* chat_info        */
    NULL,                           /* chat_info_defaults */
    netsoul_login,                  /* login            */
    netsoul_close,                  /* close            */
    netsoul_send_im,                /* send_im          */
    NULL,                           /* set_info         */
    netsoul_send_typing,            /* send_typing      */
    netsoul_get_info,               /* get_info         */
    netsoul_set_away,               /* set_away         */
    netsoul_set_idle,               /* set_idle         */
    NULL,                           /* change_password  */
    netsoul_add_buddy,              /* add_buddy        */
    netsoul_add_buddies,            /* add_buddies      */
    netsoul_remove_buddy,           /* remove_buddy     */
    NULL,                           /* remove_buddies   */
    NULL,                           /* add_permit       */
    NULL,                           /* add_deny         */
    NULL,                           /* rem_permit       */
    NULL,                           /* rem_deny         */
    NULL,                           /* set_permit_deny  */
    NULL/*netsoul_join_chat*/,              /* join_chat        */
    NULL/*netsoul_reject_chat*/,            /* reject_chat      */
    NULL,				    /* get_chat_name	*/
    NULL/*netsoul_chat_invite*/,            /* chat_invite      */
    NULL,                           /* chat_leave       */
    NULL,                           /* chat_whisper     */
    NULL /*netsoul_chat_send*/,              /* chat_send        */
    netsoul_keepalive,              /* keepalive        */
    NULL,                           /* register_user    */
    NULL,                           /* get_cb_info      */
    NULL,                           /* get_cb_away      */
    NULL,                           /* alias_buddy      */
    NULL,                           /* group_buddy      */
    NULL,                           /* rename_group     */
    NULL,                           /* buddy_free       */
    NULL,                           /* convo_closed     */
    netsoul_normalize,              /* normalize        */
    NULL,                           /* set_buddy_icon   */
    NULL,                           /* remove_group     */
    NULL,                           /* get_cb_real_name */
    NULL,                           /* set_chat_topic   */
    NULL,                           /* find_blist_chat  */
    NULL,                           /* roomlist_get_list*/
    NULL,                           /* roomlist_cancel  */
    NULL,                           /* roomlist_expand_catagory */
    NULL,                           /* can_receive_file */
    NULL,                           /* send_file        */
    NULL,		       	    /* new_xfer */
    NULL,			    /* offline_message */
    NULL,			    /* whiteboard_prpl_ops */
    NULL,			    /* send_raw */
    NULL,                           /* roomlist_room_serialize */
    NULL,		       	    /* unregister_user     */
    NULL,			    /* send_attention      */
    NULL,			    /* get_attention_types */
    sizeof(PurplePluginProtocolInfo),
    NULL,			    /* get_account_text_table */
    NULL,			    /* initiate_media */
    NULL			    /* can_do_media */
};

#define NETSOUL_VERSION "0.42"

static PurplePluginInfo info =
{
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_PROTOCOL,           /* type           */
    NULL,                           /* ui_requirement */
    0,                              /* flags          */
    NULL,                           /* dependencies   */
    PURPLE_PRIORITY_DEFAULT,          /* priority       */
    "prpl-bilboed-netsoul",                    /* id             */
    "Netsoul",                         /* name           */
    NETSOUL_VERSION,                   /* version        */
    N_("Netsoul Plugin"),              /* summary        */
    N_("Allows Purple to send messages over the Netsoul Protocol."),    /* description    */
    N_("Edward Hervey <bilboed@gmail.com>"),   /* author     */
    NETSOUL_WEBSITE,                   /* homepage       */
    NULL,                           /* load           */
    NULL,                           /* unload         */
    NULL,                           /* destroy        */
    NULL,                           /* ui_info        */
    &prpl_info,                     /* extra_info     */
    NULL,                           /* prefs_info     */
    NULL,                           /* actions        */
    NULL, NULL, NULL, NULL          /* purple_reserved */
};


static void init_plugin(PurplePlugin *plugin)
{
    PurpleAccountOption *option;
    option = purple_account_option_string_new(_("Server"), "server", NETSOUL_DEFAULT_SERVER);
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

    option = purple_account_option_int_new(_("Port"), "port", NETSOUL_DEFAULT_PORT);
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

    option = purple_account_option_string_new(_("Location"), "location", NETSOUL_DEFAULT_LOCATION);
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

    option = purple_account_option_string_new(_("Comment"), "comment", NETSOUL_DEFAULT_COMMENT);
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

    _netsoul_plugin = plugin;
}

PURPLE_INIT_PLUGIN(netsoul, init_plugin, info)
