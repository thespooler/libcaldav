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

#include "add-caldav-object.h"
#include "response-parser.h"
#include <glib.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Function for adding a new event.
 * @param settings A pointer to caldav_settings. @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return TRUE in case of error, FALSE otherwise.
 */
gboolean caldav_add(caldav_settings* settings, caldav_error* error) {
	CURL* curl;
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE];
	struct config_data data;
	struct MemoryStruct chunk;
	struct MemoryStruct headers;
	struct curl_slist *http_header = NULL;
	gboolean result = FALSE;
	gchar* url;

	chunk.memory = NULL; /* we expect realloc(NULL, size) to work */
	chunk.size = 0;    /* no data at this point */
	headers.memory = NULL;
	headers.size = 0;

	curl = get_curl(settings);
	if (!curl) {
		error->code = -1;
		error->str = g_strdup("Could not initialize libcurl");
		g_free(settings->file);
		settings->file = NULL;
		return TRUE;
	}

	http_header = curl_slist_append(http_header,
			"Content-Type: text/calendar; charset=\"utf-8\"");
	http_header = curl_slist_append(http_header, "If-None-Match: *");
	http_header = curl_slist_append(http_header, "Expect:");
	http_header = curl_slist_append(http_header, "Transfer-Encoding:");
	data.trace_ascii = settings->trace_ascii;

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,	WriteHeaderCallback);
	/* we pass our 'headers' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&headers);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, (char *) &error_buf);
	if (settings->debug) {
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
		curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &data);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	gchar* tmp = random_file_name(settings->file);
	gchar* s = rebuild_url(settings, NULL);
	if (g_str_has_suffix(s, "/")) {
		url = g_strdup_printf("%slibcaldav-%s.ics", s, tmp);
	}
	else {
		url = g_strdup_printf("%s/libcaldav-%s.ics", s, tmp);
	}
	g_free(s);
	g_free(tmp);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	tmp = g_strdup(settings->file);
	g_free(settings->file);
	settings->file = verify_uid(tmp);
	g_free(tmp);
	/* enable uploading */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, settings->file);
	curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(settings->file));
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1);
	curl_easy_setopt(curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
	res = curl_easy_perform(curl);
	if (res != 0) {
		error->code = -1;
		error->str = g_strdup_printf("%s", error_buf);
		g_free(settings->file);
		settings->file = NULL;
		result = TRUE;
	}
	else {
		long code;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (! parse_response(CALDAV_PUT, code, chunk.memory)) {
			error->str = g_strdup(chunk.memory);
			error->code = code;
			result = TRUE;
		}
	}
	if (! settings->id)
		settings->id = caldav_get_caldav_id();
	gchar* etag = get_response_header("ETAG", headers.memory, FALSE);
	if (etag) {
		settings->id->Type = CALDAV_ETAG_TYPE;
		gchar* tmp = sanitize(etag);
		g_free(etag);
		settings->id->Ident.Etag.etag = g_strdup(tmp);
		g_free(tmp);
		settings->id->Ident.Etag.uri = g_strdup(url);
	}
	else {
		gchar* location = get_response_header("Location", headers.memory, FALSE);
		if (location) {
			settings->id->Type = CALDAV_LOCATION_TYPE;
			settings->id->Ident.Location.location = g_strdup(location);
			g_free(location);
		}
	}
	g_free(settings->url);
	settings->url = NULL;
	if (chunk.memory)
		free(chunk.memory);
	if (headers.memory)
		free(headers.memory);
	curl_slist_free_all(http_header);
	curl_easy_cleanup(curl);
	return result;
}

