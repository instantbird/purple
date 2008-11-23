/**
 * @file gettext.h Gettext API
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
#ifndef _PURPLE_GETTEXT_H_
#define _PURPLE_GETTEXT_H_

/** @copydoc _PurpleGetTextUiOps */
typedef struct _PurpleGetTextUiOps PurpleGetTextUiOps;


/**  Gettext UI operations, used to obtain translated strings
 */
struct _PurpleGetTextUiOps
{
	/** The translation of a single string, located in the given package
	 */
	const char *(*get_text)(const char *package,
				const char *string);

	/** The translation of a single string, in either singular or
	 *  plural form, depending on the value of the number parameter
	 */
	const char *(*get_plural_text)(const char *package,
				       const char *singular,
				       const char *plural,
				       unsigned long int number);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/
/**
 * Sets the UI operations structure to be used for gettext.
 *
 * @param ops The UI operations structure.
 */
void purple_gettext_set_ui_ops(PurpleGetTextUiOps *ops);

/**
 * Returns the UI operations structure used for gettext.
 *
 * @return The UI operations structure in use.
 */
PurpleGetTextUiOps *purple_gettext_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name GetText API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Get the translation of a string, located in the given package
 *
 * @param package    The package.
 * @param string     The string that needs to be translated.
 *
 * @return The translated string.
 */
const char *purple_get_text(const char *package,
			    const char *string);

/**
 * Get the translation of a string, in either singular or plural form,
 * depending on the value of the number parameter
 *
 * @param package    The package.
 * @param singular   The string that needs to be translated, in singular form.
 * @param plural     The string that needs to be translated, in plural form.
 * @param number     The number used to decide if the singular or plural form
 *                   should be used.
 *
 * @return The translated string.
 */
const char *purple_get_plural_text(const char *package,
				   const char *singular,
				   const char *plural,
				   unsigned long int number);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_GETTEXT_H_ */
