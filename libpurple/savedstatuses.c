/**
 * @file savedstatuses.c Saved Status API
 * @ingroup core
 */

/* purple
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
 */
#include "internal.h"

#include "idle.h"
#include "savedstatuses.h"

/**
 * The default message to use when the user becomes auto-away.
 */
#define DEFAULT_AUTOAWAY_MESSAGE _("I'm not here right now")

static gboolean is_idle = FALSE;
static gboolean is_idleaway = FALSE;
static gboolean is_offline = TRUE;

static PurpleSavedStatus *default_status = NULL;
static PurpleSavedStatus *offline_status = NULL;
static PurpleSavedStatus *idleaway_status = NULL;

struct _PurpleSavedStatus
{
	PurpleStatusPrimitive type;
	char *message;
};

static void
free_saved_status(PurpleSavedStatus *status)
{
	g_return_if_fail(status != NULL);

	g_free(status->message);
	g_free(status);
}

static PurpleSavedStatus *
purple_savedstatus_new(PurpleStatusPrimitive type)
{
	PurpleSavedStatus *status = g_new0(PurpleSavedStatus, 1);
	status->type = type;
	return status;
}

/**************************************************************************
* Saved status API
**************************************************************************/
void
purple_savedstatus_set_type(PurpleSavedStatus *status, PurpleStatusPrimitive type)
{
	g_return_if_fail(status != NULL);

	status->type = type;
}

void
purple_savedstatus_set_message(PurpleSavedStatus *status, const char *message)
{
	g_return_if_fail(status != NULL);

	g_free(status->message);
	if ((message == NULL) || (*message == '\0'))
		status->message = NULL;
	else
		status->message = g_strdup(message);
}

PurpleSavedStatus *
purple_savedstatus_get_current(void)
{
	PurpleSavedStatus *default_status;

	if (purple_savedstatus_is_offline())
		return purple_savedstatus_get_offline();

	/* Return the idle_away status only if the current status is not offline. */
	default_status = purple_savedstatus_get_default();
	if (purple_savedstatus_is_idleaway() &&
	    purple_savedstatus_get_type(default_status) != PURPLE_STATUS_OFFLINE)
		return purple_savedstatus_get_idleaway();

	return default_status;
}

PurpleSavedStatus *
purple_savedstatus_get_default()
{
	return default_status;
}

PurpleSavedStatus *
purple_savedstatus_get_offline()
{
	return offline_status;
}

PurpleSavedStatus *
purple_savedstatus_get_idleaway()
{
	return idleaway_status;
}

gboolean
purple_savedstatus_is_idle()
{
	return is_idle;
}

gboolean
purple_savedstatus_is_idleaway()
{
	return is_idleaway;
}

void
purple_savedstatus_set_idleaway(gboolean idleaway, const char *message)
{
	GList *accounts, *node;
	PurpleSavedStatus *old, *saved_status;

	if (purple_savedstatus_is_idleaway() == idleaway)
		/* Don't need to do anything */
		return;

	old = purple_savedstatus_get_current();
	is_idleaway = idleaway;

	if (idleaway && (purple_savedstatus_get_type(old) == PURPLE_STATUS_AWAY))
		/* Our global status is already "away," so don't change anything */
		return;

	if (idleaway) {
		const char *new_message;

		saved_status = purple_savedstatus_get_idleaway();
		/* Use the message provided as parameter only if the
		   current status has no status message. */
		new_message = purple_savedstatus_get_message(old);
		if (new_message == NULL)
			new_message = message;
		purple_savedstatus_set_message(saved_status, new_message);
	}
	else
		saved_status = purple_savedstatus_get_default();

	accounts = purple_accounts_get_all_active();
	for (node = accounts; node != NULL; node = node->next)
	{
		PurpleAccount *account;
		PurplePresence *presence;
		PurpleStatus *status;

		account = node->data;
		presence = purple_account_get_presence(account);
		status = purple_presence_get_active_status(presence);

		if (!idleaway || purple_status_is_available(status))
			purple_savedstatus_activate_for_account(saved_status, account);
	}

	g_list_free(accounts);
}

gboolean
purple_savedstatus_is_offline()
{
	return is_offline;
}

void
purple_savedstatus_set_offline(gboolean offline)
{
	is_offline = offline;
	purple_savedstatus_activate(purple_savedstatus_get_current());
}

PurpleStatusPrimitive
purple_savedstatus_get_type(const PurpleSavedStatus *saved_status)
{
	g_return_val_if_fail(saved_status != NULL, PURPLE_STATUS_OFFLINE);

	return saved_status->type;
}

const char *
purple_savedstatus_get_message(const PurpleSavedStatus *saved_status)
{
	g_return_val_if_fail(saved_status != NULL, NULL);

	return saved_status->message;
}

void
purple_savedstatus_activate(PurpleSavedStatus *saved_status)
{
	GList *accounts, *node;

	g_return_if_fail(saved_status != NULL);

	accounts = purple_accounts_get_all_active();
	for (node = accounts; node != NULL; node = node->next)
	{
		PurpleAccount *account;

		account = node->data;

		purple_savedstatus_activate_for_account(saved_status, account);
	}

	g_list_free(accounts);

	if (purple_savedstatus_is_idleaway()) {
		purple_savedstatus_set_idleaway(FALSE, NULL);
	}
}

void
purple_savedstatus_activate_for_account(const PurpleSavedStatus *saved_status,
									  PurpleAccount *account)
{
	const PurpleStatusType *status_type;
	const char *message = NULL;

	g_return_if_fail(saved_status != NULL);
	g_return_if_fail(account != NULL);

	status_type = purple_account_get_status_type_with_primitive(account, saved_status->type);
	if (status_type == NULL) {
		if (saved_status->type == PURPLE_STATUS_UNAVAILABLE)
			status_type = purple_account_get_status_type_with_primitive(account,
										    PURPLE_STATUS_AWAY);
		if (status_type == NULL)
			return;
	}
	message = saved_status->message;

	if ((message != NULL) &&
		(purple_status_type_get_attr(status_type, "message")))
	{
		purple_account_set_status(account, purple_status_type_get_id(status_type),
								TRUE, "message", message, NULL);
	}
	else
	{
		purple_account_set_status(account, purple_status_type_get_id(status_type),
								TRUE, NULL);
	}
}

void
purple_savedstatuses_init(void)
{
	idleaway_status = purple_savedstatus_new(PURPLE_STATUS_AWAY);
	default_status = purple_savedstatus_new(PURPLE_STATUS_AVAILABLE);
	offline_status = purple_savedstatus_new(PURPLE_STATUS_OFFLINE);
}

#define free_and_null_saved_status(status) free_saved_status(status); status = NULL

void
purple_savedstatuses_uninit(void)
{
	free_and_null_saved_status(idleaway_status);
	free_and_null_saved_status(offline_status);
	free_and_null_saved_status(default_status);
}
