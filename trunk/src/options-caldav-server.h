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

#ifndef __OPTIONS_CALDAV_SERVER_H__
#define __OPTIONS_CALDAV_SERVER_H__

#include "caldav-utils.h"
#include "caldav.h"

/**
 * Function for getting supported options from a server.
 * @param curl A pointer to an initialized CURL instance
 * @param url string containing the URL to the server. If authentication
 * is required prior to making the call the credentials must be available
 * via CURLOPT_USERPWD before calling.
 * @param result A pointer to a struct _response. If test is true
 * this variable can be NULL. Caller is responsible for freeing associated
 * memory.
 * @param error A pointer to caldav_error. @see caldav_error
 * @param test if this is true response will be whether the server
 * represented by the URL is a CalDAV collection or not.
 * @return FALSE in case of error, TRUE otherwise.
 */
gboolean caldav_getoptions(CURL* curl, gchar* url, response* result,
				caldav_error* error, gboolean test);

#endif
