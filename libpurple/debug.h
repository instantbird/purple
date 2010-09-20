/**
 * @file debug.h Debug API
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
#ifndef _PURPLE_DEBUG_H_
#define _PURPLE_DEBUG_H_

#include <glib.h>
#include <stdarg.h>

/**
 * Debug levels.
 */
typedef enum
{
	PURPLE_DEBUG_ALL = 0,  /**< All debug levels.              */
	PURPLE_DEBUG_MISC,     /**< General chatter.               */
	PURPLE_DEBUG_INFO,     /**< General operation Information. */
	PURPLE_DEBUG_WARNING,  /**< Warnings.                      */
	PURPLE_DEBUG_ERROR,    /**< Errors.                        */
	PURPLE_DEBUG_FATAL     /**< Fatal errors.                  */

} PurpleDebugLevel;

/**
 * Debug UI operations.
 */
typedef struct
{
	void (*print)(PurpleDebugLevel level, const char *category,
				  const char *arg_s);
	void (*print_with_location)(PurpleDebugLevel level, const char *category,
				    const char *file, int line,
				    const char *function, const char *arg_s);
	gboolean (*is_enabled)(PurpleDebugLevel level,
			const char *category);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
} PurpleDebugUiOps;

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Debug API                                                       */
/**************************************************************************/

/**
 * Outputs debug information.
 *
 * @param level    The debug level.
 * @param category The category (or @c NULL).
 * @param file     The file containing the calling code.
 * @param line     The line number.
 * @param function The calling function.
 * @param format   The format string.
 */
void purple_debug_with_location(PurpleDebugLevel level, const char *category,
				const char *file, int line,
				const char *function, const char *format, ...);

#ifndef _MSC_VER
/* The use of __VA_ARGS__ will cause gcc to annoyingly output this warning:
 *  warning: anonymous variadic macros were introduced in C99
 * Tell it to shut up...
 */
#pragma GCC system_header
#endif

/**
 * Outputs debug information.
 *
 * @param level    The debug level.
 * @param category The category (or @c NULL).
 * @param format   The format string.
 */
/*
void purple_debug(PurpleDebugLevel level, const char *category,
				const char *format, ...) G_GNUC_PRINTF(3, 4);
*/
#define purple_debug(level, category, ...)				\
	purple_debug_with_location(level, category, __FILE__, __LINE__, \
	                           __FUNCTION__, __VA_ARGS__)

/**
 * Outputs misc. level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_MISC as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see purple_debug()
 */
/*
void purple_debug_misc(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);
*/
#define purple_debug_misc(category, ...) \
	purple_debug(PURPLE_DEBUG_MISC, category, __VA_ARGS__)

/**
 * Outputs info level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_INFO as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see purple_debug()
 */
/*
void purple_debug_info(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);
*/
#define purple_debug_info(category, ...) \
	purple_debug(PURPLE_DEBUG_INFO, category, __VA_ARGS__)

/**
 * Outputs warning level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_WARNING as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see purple_debug()
 */
/*
void purple_debug_warning(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);
*/
#define purple_debug_warning(category, ...) \
	purple_debug(PURPLE_DEBUG_WARNING, category, __VA_ARGS__)

/**
 * Outputs error level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_ERROR as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see purple_debug()
 */
/*
void purple_debug_error(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);
*/
#define purple_debug_error(category, ...) \
	purple_debug(PURPLE_DEBUG_ERROR, category, __VA_ARGS__)

/**
 * Outputs fatal error level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_FATAL as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see purple_debug()
 */
/*
void purple_debug_fatal(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);
*/
#define purple_debug_fatal(category, ...) \
	purple_debug(PURPLE_DEBUG_FATAL, category, __VA_ARGS__)

/**
 * Enable or disable verbose debugging.  This ordinarily should only be called
 * by #purple_debug_init, but there are cases where this can be useful for
 * plugins.
 *
 * @param verbose TRUE to enable verbose debugging or FALSE to disable it.
 *
 * @since 2.6.0
 */
void purple_debug_set_verbose(gboolean verbose);

/**
 * Check if verbose logging is enabled.
 *
 * @return TRUE if verbose debugging is enabled, FALSE if it is not.
 *
 * @since 2.6.0
 */
gboolean purple_debug_is_verbose(void);

/**
 * Enable or disable unsafe debugging.  This ordinarily should only be called
 * by #purple_debug_init, but there are cases where this can be useful for
 * plugins.
 *
 * @param unsafe TRUE to enable debug logging of messages that could
 *        potentially contain passwords and other sensitive information.
 *        FALSE to disable it.
 *
 * @since 2.6.0
 */
void purple_debug_set_unsafe(gboolean unsafe);

/**
 * Check if unsafe debugging is enabled.  Defaults to FALSE.
 *
 * @return TRUE if the debug logging of all messages is enabled, FALSE
 *         if messages that could potentially contain passwords and other
 *         sensitive information are not logged.
 *
 * @since 2.6.0
 */
gboolean purple_debug_is_unsafe(void);

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used when outputting debug
 * information.
 *
 * @param ops The UI operations structure.
 */
void purple_debug_set_ui_ops(PurpleDebugUiOps *ops);

/**
 * Returns the UI operations structure used when outputting debug
 * information.
 *
 * @return The UI operations structure in use.
 */
PurpleDebugUiOps *purple_debug_get_ui_ops(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_DEBUG_H_ */
