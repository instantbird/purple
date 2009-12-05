/**
 * @file debug.c Debug API
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
#include "debug.h"
#include "prefs.h"
#include "util.h"

static PurpleDebugUiOps *debug_ui_ops = NULL;

/*
 * These determine whether verbose or unsafe debugging are desired.  I
 * don't want to make these purple preferences because their values should
 * not be remembered across instances of the UI.
 */
static gboolean debug_verbose = FALSE;
static gboolean debug_unsafe = FALSE;

void
purple_debug_with_location(PurpleDebugLevel level, const char *category,
			   const char *file, int line, const char *function,
			   const char *format, ...)
{
	va_list args;
	PurpleDebugUiOps *ops;
	char *arg_s = NULL;

	g_return_if_fail(level != PURPLE_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	ops = purple_debug_get_ui_ops();
	if ((ops == NULL) ||
	    (ops->print_with_location == NULL && ops->print == NULL) ||
	    (ops->is_enabled && !ops->is_enabled(level, category)))
		return;

	va_start(args, format);

	arg_s = g_strdup_vprintf(format, args);
	if (ops->print_with_location != NULL)
		ops->print_with_location(level, category, file, line, function, arg_s);
	else
		ops->print(level, category, arg_s);
	g_free(arg_s);

	va_end(args);
}

/* This is kept for ABI compatibility only. Should be removed once we
 * change the version number and thus can safely assume that all old
 * plugins will be disabled.
 * New code uses purple_debug_with_location. */
#undef purple_debug
#undef purple_debug_misc
#undef purple_debug_info
#undef purple_debug_warning
#undef purple_debug_error
#undef purple_debug_fatal

static void
purple_debug_vargs(PurpleDebugLevel level, const char *category,
				 const char *format, va_list args)
{
	PurpleDebugUiOps *ops;
	char *arg_s = NULL;

	g_return_if_fail(level != PURPLE_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	ops = purple_debug_get_ui_ops();

	if ((ops == NULL) || (ops->print == NULL) ||
	    (ops->is_enabled && !ops->is_enabled(level, category)))
		return;

	arg_s = g_strdup_vprintf(format, args);
	ops->print(level, category, arg_s);
	g_free(arg_s);
}

void
purple_debug(PurpleDebugLevel level, const char *category,
		   const char *format, ...)
{
	va_list args;

	g_return_if_fail(level != PURPLE_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	va_start(args, format);
	purple_debug_vargs(level, category, format, args);
	va_end(args);
}

#define PURPLE_IMPL_DEBUG_HELPER(name, NAME)				  \
 void									  \
 purple_debug_##name(const char *category, const char *format, ...)	  \
 {									  \
	 va_list args;							  \
									  \
	 g_return_if_fail(format != NULL);				  \
									  \
	 va_start(args, format);					  \
	 purple_debug_vargs(PURPLE_DEBUG_##NAME, category, format, args); \
	 va_end(args);							  \
 }

PURPLE_IMPL_DEBUG_HELPER(misc, MISC)
PURPLE_IMPL_DEBUG_HELPER(info, INFO)
PURPLE_IMPL_DEBUG_HELPER(warning, WARNING)
PURPLE_IMPL_DEBUG_HELPER(error, ERROR)
PURPLE_IMPL_DEBUG_HELPER(fatal, FATAL)
/* End of the code kept for ABI compatibility */

void
purple_debug_set_ui_ops(PurpleDebugUiOps *ops)
{
	debug_ui_ops = ops;
}

gboolean
purple_debug_is_verbose()
{
	return debug_verbose;
}

void
purple_debug_set_verbose(gboolean verbose)
{
	debug_verbose = verbose;
}

gboolean
purple_debug_is_unsafe()
{
	return debug_unsafe;
}

void
purple_debug_set_unsafe(gboolean unsafe)
{
	debug_unsafe = unsafe;
}

PurpleDebugUiOps *
purple_debug_get_ui_ops(void)
{
	return debug_ui_ops;
}
