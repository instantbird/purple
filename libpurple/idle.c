/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"

#include "connection.h"
#include "debug.h"
#include "idle.h"
#include "signals.h"

/**
 * This is needed for the I'dle Mak'er plugin to work correctly.  We
 * use it to determine if we're the ones who set our accounts idle
 * or if someone else did it (the I'dle Mak'er plugin, for example).
 * Basically we just keep track of which accounts were set idle by us,
 * and then we'll only set these specific accounts unidle when the
 * user returns.
 */
static GList *idled_accts = NULL;

static void
set_account_idle(PurpleAccount *account, int time_idle)
{
	PurplePresence *presence;

	presence = purple_account_get_presence(account);

	if (purple_presence_is_idle(presence))
		/* This account is already idle! */
		return;

	purple_debug_info("idle", "Setting %s idle %d seconds\n",
			   purple_account_get_username(account), time_idle);
	purple_presence_set_idle(presence, TRUE, time(NULL) - time_idle);
	idled_accts = g_list_prepend(idled_accts, account);
}

static void
set_account_unidle(PurpleAccount *account)
{
	PurplePresence *presence;

	presence = purple_account_get_presence(account);

	idled_accts = g_list_remove(idled_accts, account);

	if (!purple_presence_is_idle(presence))
		/* This account is already unidle! */
		return;

	purple_debug_info("idle", "Setting %s unidle\n",
			   purple_account_get_username(account));
	purple_presence_set_idle(presence, FALSE, 0);
}

static time_t global_time_idle = 0;
void
purple_idle_set(time_t time_idle)
{
	if (time_idle)
	{
		GList *l;
		for (l = purple_connections_get_all(); l != NULL; l = l->next)
		{
			PurpleConnection *gc = l->data;
			set_account_idle(purple_connection_get_account(gc), time_idle);
		}
	}
	else
	{
		while (idled_accts != NULL)
			set_account_unidle(idled_accts->data);
	}
	global_time_idle = time_idle;
}


static void
signing_on_cb(PurpleConnection *gc, void *data)
{
	/* When signing on a new account, check if the account should be idle */
	if (global_time_idle)
		set_account_idle(purple_connection_get_account(gc), global_time_idle);
}

static void
signing_off_cb(PurpleConnection *gc, void *data)
{
	set_account_unidle(purple_connection_get_account(gc));
}

static void *
purple_idle_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_idle_init()
{
	purple_signal_connect(purple_connections_get_handle(), "signing-on",
						purple_idle_get_handle(),
						PURPLE_CALLBACK(signing_on_cb), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signing-off",
						purple_idle_get_handle(),
						PURPLE_CALLBACK(signing_off_cb), NULL);
}

void
purple_idle_uninit()
{
	purple_signals_disconnect_by_handle(purple_idle_get_handle());
}
