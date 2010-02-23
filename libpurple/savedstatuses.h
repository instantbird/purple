/**
 * @file savedstatuses.h Saved Status API
 * @ingroup core
 * @see @ref savedstatus-signals
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
#ifndef _PURPLE_SAVEDSTATUSES_H_
#define _PURPLE_SAVEDSTATUSES_H_

/**
 * Saved statuses don't really interact much with the rest of Purple.  It
 * could really be a plugin.  It's just a list of away states.  When
 * a user chooses one of the saved states, their Purple accounts are set
 * to the settings of that state.
 */

typedef struct _PurpleSavedStatus     PurpleSavedStatus;

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Saved status subsystem                                          */
/**************************************************************************/
/*@{*/

/**
 * Set the type for the given saved status.
 *
 * @param status  The saved status.
 * @param type    The type of saved status.
 */
void purple_savedstatus_set_type(PurpleSavedStatus *status,
							   PurpleStatusPrimitive type);

/**
 * Set the message for the given saved status.
 *
 * @param status  The saved status.
 * @param message The message, or NULL if you want to unset the
 *                message for this status.
 */
void purple_savedstatus_set_message(PurpleSavedStatus *status,
								  const char *message);

/**
 * Returns the currently selected saved status.  If we are idle
 * then this returns purple_savedstatus_get_idleaway().  Otherwise
 * it returns purple_savedstatus_get_default().
 *
 * @return A pointer to the in-use PurpleSavedStatus.
 *         This function never returns NULL.
 */
PurpleSavedStatus *purple_savedstatus_get_current(void);

/**
 * Returns the default saved status that is used when our
 * accounts are not idle-away.
 *
 * @return A pointer to the in-use PurpleSavedStatus.
 *         This function never returns NULL.
 */
PurpleSavedStatus *purple_savedstatus_get_default(void);

/**
 * Returns the saved status that is used when the application is in
 * offline mode
 *
 * @return A pointer to the offline PurpleSavedStatus.
 *         This function never returns NULL.
 */
PurpleSavedStatus *purple_savedstatus_get_offline(void);

/**
 * Return TRUE if we are currently offline.  Otherwise
 * returns FALSE.
 *
 * @return TRUE if the application is in offline mode.
 */
gboolean purple_savedstatus_is_offline(void);

/**
 * Set whether the application is in offline mode.
 *
 * @param offline TRUE if the application should be switched to
 *                offline mode.  FALSE if the current status should be
 *                used.
 */
void purple_savedstatus_set_offline(gboolean offline);

/**
 * Returns the saved status that is used when your
 * accounts become idle-away.
 *
 * @return A pointer to the idle-away PurpleSavedStatus.
 *         This function never returns NULL.
 */
PurpleSavedStatus *purple_savedstatus_get_idleaway(void);

/**
 * Return TRUE if we are currently idle-away.  Otherwise
 * returns FALSE.
 *
 * @return TRUE if our accounts have been set to idle-away.
 */
gboolean purple_savedstatus_is_idleaway(void);

/**
 * Set whether accounts in Purple are idle-away or not.
 *
 * @param idleaway TRUE if accounts should be switched to use the
 *                 idle-away saved status.  FALSE if they should
 *                 be switched to use the default status.
 * @param message The away message to use for the idleaway status if
 *                the current status has no status message.
 */
void purple_savedstatus_set_idleaway(gboolean idleaway, const char *message);

/**
 * Return the type of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The name.
 */
PurpleStatusPrimitive purple_savedstatus_get_type(const PurpleSavedStatus *saved_status);

/**
 * Return the default message of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The message.  This will return NULL if the saved
 *         status does not have a message.  This will
 *         contain the normal markup that is created by
 *         Purple's IMHTML (basically HTML markup).
 */
const char *purple_savedstatus_get_message(const PurpleSavedStatus *saved_status);

/**
 * Sets the statuses for all your accounts to those specified
 * by the given saved_status.  This function calls
 * purple_savedstatus_activate_for_account() for all your accounts.
 *
 * @param saved_status The status you want to set your accounts to.
 */
void purple_savedstatus_activate(PurpleSavedStatus *saved_status);

/**
 * Sets the statuses for a given account to those specified
 * by the given saved_status.
 *
 * @param saved_status The status you want to set your accounts to.
 * @param account      The account whose statuses you want to change.
 */
void purple_savedstatus_activate_for_account(const PurpleSavedStatus *saved_status, PurpleAccount *account);

/**
 * Initializes the status subsystem.
 */
void purple_savedstatuses_init(void);

/**
 * Uninitializes the status subsystem.
 */
void purple_savedstatuses_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_SAVEDSTATUSES_H_ */
