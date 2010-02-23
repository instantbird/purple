/**
 * @file idle.h Idle API
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
#ifndef _PURPLE_IDLE_H_
#define _PURPLE_IDLE_H_

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Idle API                                                        */
/**************************************************************************/
/*@{*/

/**
 * Set as idle (if the timestamp is provided) or as unidle if the
 * timestamp is 0.
 */
void purple_idle_set(time_t time);

/*@}*/

/**************************************************************************/
/** @name Idle Subsystem                                                  */
/**************************************************************************/
/*@{*/

/**
 * Initializes the idle system.
 */
void purple_idle_init(void);

/**
 * Uninitializes the idle system.
 */
void purple_idle_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_IDLE_H_ */
