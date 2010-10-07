/* vim: set textwidth=80 tabstop=4: */

/* Copyright (c) 2010 Michael Rasmussen (mir@datanom.net)
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

#ifndef __RESPONSE_PARSER_H__
#define __RESPONSE_PARSER_H__

#include <glib.h>
G_BEGIN_DECLS

#include "caldav-utils.h"
#include "caldav.h"

/*
 * Supported CalDAV methods
 * PUT, REPORT, DELETE, PROPFIND, LOCK, UNLOCK, OPTIONS, GET
 * 
 */
typedef enum {
	CALDAV_DELETE,
	CALDAV_GET,
	CALDAV_LOCK,
	CALDAV_OPTIONS,
	CALDAV_PROPFIND,
	CALDAV_PUT,
	CALDAV_REPORT,
	CALDAV_UNLOCK
} CALDAV_METHOD;

gboolean
parse_response(CALDAV_METHOD method, int response_code, gchar* response_body);

G_END_DECLS

#endif
