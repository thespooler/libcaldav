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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "options-caldav-server.h"
#include <glib.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
		caldav_error* error, gboolean test) {
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE + 1];
	struct MemoryStruct chunk;
	struct MemoryStruct headers;
	gboolean enabled = FALSE;

	if (!error) {
		error = (caldav_error *) malloc(sizeof(struct _caldav_error));
		memset(error, '\0', sizeof(struct _caldav_error));
	}
	chunk.memory = NULL; /* we expect realloc(NULL, size) to work */
	chunk.size = 0;    /* no data at this point */
	headers.memory = NULL;
	headers.size = 0;
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, WriteHeaderCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&headers);
	/* some servers don't like requests that are made without a user-agent
	 * field, so we provide one */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/0.1");
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_buf);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	if (res == 0) {
		gchar* head;
		/*gchar* headerlist = g_strdup(headers.memory);*/
		head = get_response_header("DAV", headers.memory, TRUE);
		if (head && strstr(head, "calendar-access") != NULL) {
			enabled = TRUE;
			g_free(error);
			if (! test) {
				result->msg = g_strdup(
						get_response_header("Allow", headers.memory, FALSE));
			}
		}
		else {
			long code;
			res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
			error->code = code;
			error->str = g_strdup(headers.memory);
		}
	}
	else {
		error->code = -1;
		error->str = g_strdup("URL is not a CalDAV resource");
	}
	if (chunk.memory)
		free(chunk.memory);
	if (headers.memory)
		free(headers.memory);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
	return enabled;
}
