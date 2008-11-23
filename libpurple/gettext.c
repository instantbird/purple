/**
 * @file gettext.c GetText API
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

#include "gettext.h"
#ifdef ENABLE_NLS
#  include <libintl.h>
#endif
#include <glib.h>

static PurpleGetTextUiOps *gettext_ui_ops = NULL;

const char *purple_get_text(const char *package,
			    const char *string)
{
	PurpleGetTextUiOps *ui_ops;

	g_return_val_if_fail(package  != NULL, NULL);
	g_return_val_if_fail(string   != NULL, NULL);

	ui_ops = purple_gettext_get_ui_ops();

	if (ui_ops != NULL && ui_ops->get_text != NULL)
		return ui_ops->get_text(package, string);

#ifdef ENABLE_NLS
	return (const char *)dgettext(package, string);
#else
	return string;  
#endif
}

const char *purple_get_plural_text(const char *package,
				   const char *singular,
				   const char *plural,
				   unsigned long int number)
{
	PurpleGetTextUiOps *ui_ops;

	g_return_val_if_fail(package  != NULL, NULL);
	g_return_val_if_fail(singular != NULL, NULL);
	g_return_val_if_fail(plural   != NULL, NULL);

	ui_ops = purple_gettext_get_ui_ops();

	if (ui_ops != NULL && ui_ops->get_plural_text != NULL)
		return ui_ops->get_plural_text(package, singular, plural, number);

#ifdef ENABLE_NLS
	return dngettext(package, singular, plural, number);
#else
	return (const char *)(number == 1 ? singular : plural);
#endif
}

void
purple_gettext_set_ui_ops(PurpleGetTextUiOps *ops)
{
	gettext_ui_ops = ops;
}

PurpleGetTextUiOps *
purple_gettext_get_ui_ops(void)
{
	return gettext_ui_ops;
}
