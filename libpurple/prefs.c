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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include "internal.h"
#include "prefs.h"
#include "debug.h"
#include "util.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

static PurplePrefsUiOps *prefs_ui_ops = NULL;

struct pref_cb {
	PurplePrefCallback func;
	gpointer data;
	guint id;
	void *handle;
	void *observer;
	char *name;
};

static guint       save_timer = 0;
static gboolean    prefs_loaded = FALSE;
static GSList     *callbacks = NULL;

static gboolean
save_cb(gpointer data)
{
	PurplePrefsUiOps *uiop = purple_prefs_get_ui_ops();
	g_return_val_if_fail(uiop, FALSE);
	uiop->save();

	save_timer = 0;
	return FALSE;
}

/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

gboolean
purple_prefs_load()
{
	prefs_loaded = TRUE;

	return TRUE;
}

static void
prefs_save_cb(const char *name, PurplePrefType type, gconstpointer val,
			  gpointer user_data)
{
	if (!prefs_loaded)
		return;

	purple_debug_misc("prefs", "%s changed, scheduling save.\n", name);

	if (save_timer == 0)
		save_timer = purple_timeout_add_seconds(6, save_cb, NULL);
}

#define UIOP(aCall)						    \
	{							    \
		PurplePrefsUiOps *uiop = purple_prefs_get_ui_ops(); \
		g_return_if_fail(uiop);				    \
								    \
		uiop->aCall;					    \
	}

#define UIOP_return(aCall, aDefault)				    \
	{							    \
		PurplePrefsUiOps *uiop = purple_prefs_get_ui_ops(); \
		g_return_val_if_fail(uiop, aDefault);		    \
								    \
		return uiop->aCall;				    \
	}

void
purple_prefs_add_none(const char *name)
	UIOP(add_none(name))

void
purple_prefs_add_bool(const char *name, gboolean value)
	UIOP(add_bool(name, value))

void
purple_prefs_add_int(const char *name, int value)
	UIOP(add_int(name, value))

void
purple_prefs_add_string(const char *name, const char *value)
	UIOP(add_string(name, value))

void
purple_prefs_remove(const char *name)
	UIOP(remove(name))

void
purple_prefs_trigger_callback(const char *name)
{
	GSList *cbs;

	purple_debug_misc("prefs", "trigger callback %s\n", name);

	for (cbs = callbacks; cbs; cbs = cbs->next) {
		const char *cb_name = ((struct pref_cb *)cbs->data)->name;
		size_t len = strlen(cb_name);
		if (!strncmp(cb_name, name, len) &&
		    (name[len] == 0 || name[len] == '/' ||
		     (len && name[len - 1] == '/'))) {
			/* This test should behave like this:
			 * name    = /toto/tata
			 * cb_name = /toto/tata --> true
			 * cb_name = /toto/tatatiti --> false
			 * cb_name = / --> true
			 * cb_name = /toto --> true
			 * cb_name = /toto/ --> true
			 */
			purple_prefs_observe(cbs->data);
		}
	}
}

void
purple_prefs_set_bool(const char *name, gboolean value)
	UIOP(set_bool(name, value))

void
purple_prefs_set_int(const char *name, int value)
	UIOP(set_int(name, value))

void
purple_prefs_set_string(const char *name, const char *value)
	UIOP(set_string(name, value))

gboolean
purple_prefs_exists(const char *name)
	UIOP_return(exists(name), FALSE)

PurplePrefType
purple_prefs_get_type(const char *name)
	UIOP_return(get_type(name), PURPLE_PREF_NONE)

gboolean
purple_prefs_get_bool(const char *name)
	UIOP_return(get_bool(name), FALSE)

int
purple_prefs_get_int(const char *name)
	UIOP_return(get_int(name), 0)

const char *
purple_prefs_get_string(const char *name)
	UIOP_return(get_string(name), NULL)

guint
purple_prefs_connect_callback(void *handle, const char *name, PurplePrefCallback func, gpointer data)
{
	struct pref_cb *cb;
	static guint cb_id = 0;
	PurplePrefsUiOps *uiop = NULL;

	g_return_val_if_fail(name != NULL, 0);
	g_return_val_if_fail(func != NULL, 0);

	uiop = purple_prefs_get_ui_ops();
	g_return_val_if_fail(uiop, 0);

	cb = g_new0(struct pref_cb, 1);

	cb->func = func;
	cb->data = data;
	cb->id = ++cb_id;
	cb->handle = handle;
	cb->name = g_strdup(name);

	cb->observer = uiop->add_observer(name, cb);

	if (cb->observer == NULL) {
		purple_debug_error("prefs", "purple_prefs_connect_callback: add observer failed for %s\n", name);
		g_free(cb->name);
		g_free(cb);
		return 0;
	}

	callbacks = g_slist_append(callbacks, cb);
	return cb->id;
}

void purple_prefs_observe(gpointer data)
{
	struct pref_cb *cb = data;
	PurplePrefsUiOps *uiop = purple_prefs_get_ui_ops();
	gconstpointer value = NULL;
	PurplePrefType type = PURPLE_PREF_NONE;
	type = uiop->get_type(cb->name);

	purple_debug_misc("prefs", "observe name = %s\n", cb->name);

	switch (type) {
		case PURPLE_PREF_INT:
			value = GINT_TO_POINTER(uiop->get_int(cb->name));
			break;
		case PURPLE_PREF_BOOLEAN:
			value = GINT_TO_POINTER(uiop->get_bool(cb->name));
			break;
		case PURPLE_PREF_STRING:
			value = uiop->get_string(cb->name);
			break;
		case PURPLE_PREF_NONE:
			break;
		default:
			purple_debug_error("prefs", "Unexpected type = %i\n", type);
	}
	cb->func(cb->name, type, value, cb->data);
}

void
purple_prefs_disconnect_callback(guint callback_id)
{
	GSList *cbs;

	for (cbs = callbacks; cbs; cbs = cbs->next) {
		struct pref_cb *cb = cbs->data;
		if (cb->id == callback_id) {
			PurplePrefsUiOps *uiop = purple_prefs_get_ui_ops();
			uiop->remove_observer(cb->name, cb->observer);

			callbacks = g_slist_delete_link(callbacks, cbs);
			g_free(cb->name);
			g_free(cb);
			return;
		}
	}
}

void
purple_prefs_disconnect_by_handle(void *handle)
{
	GSList *cbs;

	g_return_if_fail(handle != NULL);

	cbs = callbacks;
	while (cbs != NULL) {
		PurplePrefsUiOps *uiop;
		struct pref_cb *cb = cbs->data;
		if (cb->handle != handle) {
			cbs = cbs->next;
			continue;
		}

		uiop = purple_prefs_get_ui_ops();
		uiop->remove_observer(cb->name, cb->observer);

		callbacks = g_slist_delete_link(callbacks, cbs);
		g_free(cb->name);
		g_free(cb);
		cbs = callbacks;
	}
}

GList *
purple_prefs_get_children_names(const char *name)
	UIOP_return(get_children_names(name), NULL)

void *
purple_prefs_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_prefs_init(void)
{
	void *handle = purple_prefs_get_handle();

	purple_prefs_connect_callback(handle, "/", prefs_save_cb, NULL);

	purple_prefs_add_none("/purple");
	purple_prefs_add_none("/plugins");
	purple_prefs_add_none("/plugins/core");
	purple_prefs_add_none("/plugins/lopl");
	purple_prefs_add_none("/plugins/prpl");

	/* Away */
	purple_prefs_add_none("/purple/away");
	purple_prefs_add_string("/purple/away/auto_reply", "awayidle");

	/* Buddies */
	purple_prefs_add_none("/purple/buddies");

	/* Contact Priority Settings */
	purple_prefs_add_none("/purple/contact");
	purple_prefs_add_bool("/purple/contact/last_match", FALSE);
	purple_prefs_remove("/purple/contact/offline_score");
	purple_prefs_remove("/purple/contact/away_score");
	purple_prefs_remove("/purple/contact/idle_score");

	purple_prefs_load();
}

void
purple_prefs_uninit()
{
	if (save_timer != 0)
	{
		purple_timeout_remove(save_timer);
		save_cb(NULL);
	}

	purple_prefs_disconnect_by_handle(purple_prefs_get_handle());

	prefs_loaded = FALSE;
}

void
purple_prefs_set_ui_ops(PurplePrefsUiOps *ops)
{
	prefs_ui_ops = ops;
}

PurplePrefsUiOps *
purple_prefs_get_ui_ops(void)
{
	return prefs_ui_ops;
}
