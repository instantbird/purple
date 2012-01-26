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

#pragma GCC visibility push(default)
#include <libpurple/blist.h>
#pragma GCC visibility pop

#include "purpleAccountBuddy.h"
#include <imIContactsService.h>
#include <nsServiceManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsStringAPI.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleInit:5
//
extern PRLogModuleInfo *gPurpleInitLog;
#endif
#define LOG(args) PR_LOG(gPurpleInitLog, PR_LOG_DEBUG, args)

/*
 * Used as handle to connect and then mass-disconnect the buddy list signals.
 * The value is 1 when the signals are connected, 0 when they are not.
 */
int blist_signals_handle = 0;

static void buddy_signals(PurpleBuddy *aBuddy, const char *aSignal)
{
  // FIXME when this was handled by purpleCoreService, signals sent
  // while mQuitting was true were ignored.

  LOG(("Attempting to send %s signal, group = %s, buddy = %s", aSignal,
       purple_group_get_name(purple_buddy_get_group(aBuddy)), aBuddy->name));

  nsCOMPtr<purpleAccountBuddy> pab =
    purpleAccountBuddy::fromPurpleBuddy(aBuddy);
  NS_ENSURE_TRUE(pab, );

  nsresult rv = pab->NotifyObservers(aSignal);
  NS_ENSURE_SUCCESS(rv, );
}

void buddy_availability_changed(PurpleBuddy *aBuddy)
{
  buddy_signals(aBuddy, "account-buddy-availability-changed");
  buddy_signals(aBuddy, "account-buddy-status-changed");
}

static void buddy_signed_on(PurpleBuddy *aBuddy, const char *aSignal)
{
  if (!blist_signals_handle)
    return;

  buddy_availability_changed(aBuddy);
  buddy_signals(aBuddy, "account-buddy-signed-on");
}

static void buddy_signed_off(PurpleBuddy *aBuddy, const char *aSignal)
{
  if (!blist_signals_handle)
    return;

  buddy_availability_changed(aBuddy);
  buddy_signals(aBuddy, "account-buddy-signed-off");
}

static void buddy_added(PurpleBuddy *aBuddy, void *null)
{
  if (!blist_signals_handle)
    return;

  // This is the buddy-added purple signal. It is fired when a buddy
  // is added to the list or to a group.

  // FIXME what should we do if the buddy is moved to a new group. Is
  // ui_data already set in this case?

  nsCOMPtr<imIContactsService> contacts =
    do_GetService("@instantbird.org/purple/contacts-service;1");
  NS_ENSURE_TRUE(contacts, );

  nsCOMPtr<imIAccountBuddy> accountBuddy =
    new purpleAccountBuddy(aBuddy);
  contacts->AccountBuddyAdded(accountBuddy);
}

// This is still called even after signals have been removed.
static void buddy_removed(PurpleBuddy *aBuddy, void *null)
{
  // This is the buddy-removed purple signal. It is fired when a buddy
  // is removed permanenty (ie from the server list or if the user
  // request the deletion of a buddy in her list).
  // It is also fired during shutdown when we uninitialize the
  // accounts; in this case, even though the buddy isn't really
  // removed, we still want to uninitialize its purpleAccountBuddy
  // instance.

  purpleAccountBuddy *accountBuddy =
    purpleAccountBuddy::fromPurpleBuddy(aBuddy);

  if (blist_signals_handle) {
    NS_ENSURE_TRUE(accountBuddy, );

    nsCOMPtr<imIContactsService> contacts =
      do_GetService("@instantbird.org/purple/contacts-service;1");
    if (contacts)
      contacts->AccountBuddyRemoved(accountBuddy);
  }

  if (accountBuddy)
    accountBuddy->UnInit();
}

static void buddy_away(PurpleBuddy *aBuddy,
                       PurpleStatus *old_status, PurpleStatus *status)
{
  if (!blist_signals_handle)
    return;

  buddy_availability_changed(aBuddy);
}

static void buddy_idle(PurpleBuddy *aBuddy, gboolean old_idle, gboolean idle)
{
  if (!blist_signals_handle)
    return;

  buddy_availability_changed(aBuddy);
}

static void buddy_icon(PurpleBuddy *aBuddy)
{
  if (!blist_signals_handle)
    return;

  buddy_signals(aBuddy, "account-buddy-icon-changed");
}

static void buddy_alias(PurpleBlistNode *aNode, const char *old_alias)
{
  if (!blist_signals_handle)
    return;

  // We are only interested by buddy aliases
  if (!PURPLE_BLIST_NODE_IS_BUDDY(aNode))
    return;

  purpleAccountBuddy *pab =
    purpleAccountBuddy::fromPurpleBuddy((PurpleBuddy *)aNode);
  if (!pab)
    return; // it's fine to ignore these notifications for unknown buddies

  nsString oldAlias;
  NS_CStringToUTF16(nsDependentCString(old_alias), NS_CSTRING_ENCODING_UTF8,
                    oldAlias);

  pab->NotifyObservers("account-buddy-display-name-changed", oldAlias.get());
}


#define PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER(aSignal, aHandler)          \
  purple_signal_connect(instance, aSignal, &blist_signals_handle,       \
                        PURPLE_CALLBACK(aHandler), (void *)aSignal)

void
connect_to_blist_signals()
{
  // Remove the existing buddy-removed signal if this is a
  // re-initialization of libpurple.
  purple_signals_disconnect_by_handle(&blist_signals_handle);

  void *instance = purple_blist_get_handle();
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-signed-on", buddy_signed_on);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-signed-off", buddy_signed_off);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-added", buddy_added);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-removed", buddy_removed);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-status-changed", buddy_away);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-idle-changed", buddy_idle);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-icon-changed", buddy_icon);
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("blist-node-aliased", buddy_alias);
  blist_signals_handle = 1;
  LOG(("Connected to blist signals"));
}

void
disconnect_blist_signals()
{
  purple_signals_disconnect_by_handle(&blist_signals_handle);
  blist_signals_handle = 0;

  // Reconnect the buddy-removed handler so that we can still properly
  // unInit the purpleAccountBuddy instances.
  void *instance = purple_blist_get_handle();
  PURPLE_CONNECT_BUDDY_SIGNAL_HANDLER("buddy-removed", buddy_removed);
}

static void remove_blist_node(PurpleBuddyList *aList, PurpleBlistNode *aNode)
{
  // Ignore this ui ops if buddy list signals are not connected:
  if (!blist_signals_handle)
    return;

  // This is the removed blist uiop. It is fired when a possibly
  // visible element of the buddy list is removed (because the account
  // got disconnected)

  // For now, we are only interested by buddy removal
  if (!PURPLE_BLIST_NODE_IS_BUDDY(aNode))
    return;

  PurpleBuddy *buddy = (PurpleBuddy *) aNode;
  LOG(("purple uiop : remove_blist_node, name = %s", buddy->name));
  buddy_availability_changed(buddy);
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
  NULL, /* save_node */
  NULL, /* remove_node */
  NULL, /* save_account */

  NULL
};

void init_libpurple_blist()
{
  /* The only effect is to set blist_loaded to TRUE */
  purple_blist_load();

  purple_blist_set_ui_ops(&blist_uiops);
  connect_to_blist_signals();
}
