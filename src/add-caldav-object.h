/* vim: set textwidth=80 tabstop=4: */

/* Copyright (c) 2008 Michael Rasmussen (mir@datanom.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __ADD_CALDAV_OBJECT_H__
#define __ADD_CALDAV_OBJECT_H__

#include <glib.h>
G_BEGIN_DECLS

#include "caldav-utils.h"
#include "caldav.h"

/**
 * Function for adding a new event.
 * If CalDAV server returns the Location header tag, like Google Calendar,
 * then this could imply, as with Google Calendar, that Location should be
 * used when referencing this particular event on the server MUST be used.
 * This option should therefore be kept for later reference.
 * The Location will be stored in settings->url otherwise settings->url
 * will be NULL.
 * @param settings A pointer to caldav_settings. @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return TRUE in case of error, FALSE otherwise.
 */
gboolean caldav_add(caldav_settings* settings, caldav_error* error);

G_END_DECLS

#endif

