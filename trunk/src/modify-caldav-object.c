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

#include "modify-caldav-object.h"
#include <glib.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * A static literal string containing the first part of the calendar query.
 * The actual UID to use for the query is added at runtime.
 */
static char* search_head =
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
"<C:calendar-query xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
"  <D:prop xmlns:D=\"DAV:\">"
"    <D:getetag/>"
"    <C:calendar-data/>"
"  </D:prop>"
"  <C:filter>"
"    <C:comp-filter name=\"VCALENDAR\">"
"      <C:comp-filter name=\"VEVENT\">"
"        <C:prop-filter name=\"UID\">";

/**
 * A static literal string containing the last part of the calendar query
 */
static char* search_tail =
"</C:prop-filter>"
"      </C:comp-filter>"
"    </C:comp-filter>"
"  </C:filter>"
"</C:calendar-query>";

/**
 * Function for modifying an event.
 * @param settings A pointer to caldav_settings. @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return TRUE in case of error, FALSE otherwise.
 */
gboolean caldav_modify(caldav_settings* settings, caldav_error* error) {
	CURL* curl;
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE + 1];
	struct config_data data;
	struct MemoryStruct chunk;
	struct MemoryStruct headers;
	struct curl_slist *http_header = NULL;
	gchar* search;
	gchar* uid;
	gboolean result = FALSE;

	chunk.memory = NULL; /* we expect realloc(NULL, size) to work */
	chunk.size = 0;    /* no data at this point */
	headers.memory = NULL;
	headers.size = 0;
	http_header = curl_slist_append(http_header,
			"Content-Type: application/xml; charset=\"utf-8\"");
	http_header = curl_slist_append(http_header, "Depth: 1");
	http_header = curl_slist_append(http_header, "Expect:");
	http_header = curl_slist_append(http_header, "Transfer-Encoding:");
	data.trace_ascii = settings->trace_ascii;
	curl = curl_easy_init();
	if (!curl) {
		error->code = -1;
		error->str = g_strdup("Could not initialize libcurl");
		settings->file = NULL;
		return TRUE;
	}
	if (settings->username) {
		gchar* userpwd = NULL;
		if (settings->password)
			userpwd = g_strdup_printf("%s:%s",
				settings->username, settings->password);
		else
			userpwd = g_strdup_printf("%s",	settings->username);
		curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
		g_free(userpwd);
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,	WriteHeaderCallback);
	/* we pass our 'headers' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&headers);
	/* some servers don't like requests that are made without a user-agent
	 * field, so we provide one */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/0.1");
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_buf);
	if (settings->debug) {
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
		curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &data);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	curl_easy_setopt(curl, CURLOPT_URL, settings->url);
	gchar* file = g_strdup(settings->file);
	if ((uid = get_response_header("uid", file, TRUE)) == NULL) {
		g_free(file);
		error->code = 1;
		error->str = g_strdup("Error: Missing required UID for object");
		return TRUE;
	}
	g_free(file);
	search = g_strdup_printf(
		"%s\r\n<C:text-match collation=\"i;ascii-casemap\">%s</C:text-match>\r\n%s",
		search_head, uid, search_tail);
	g_free(uid);
	/* enable uploading */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, search);
	curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(search));
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "REPORT");
	res = curl_easy_perform(curl);
	curl_slist_free_all(http_header);
	http_header = NULL;
	if (res != 0) {
		error->code = -1;
		error->str = g_strdup_printf("%s", error_buf);
		settings->file = NULL;
		result = TRUE;
	}
	else {
		long code;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (code != 207) {
			error->code = code;
			error->str = g_strdup(chunk.memory);
			result = TRUE;
		}
		else {
			/* enable uploading */
			gchar* url = NULL;
			gchar* etag = NULL;
			url = get_url(chunk.memory);
			if (url) {
				etag = get_etag(chunk.memory);
				if (etag) {
					gchar* host = get_host(settings->url);
					if (host) {
						url = g_strdup_printf("%s%s", host, url);
						g_free(host);
					}
					else {
						g_free(etag);
						g_free(url);
					}
				}
				else {
					g_free(url);
				}
			}
			if (url) {
				etag = g_strdup_printf("If-Match: %s", etag);
				http_header = curl_slist_append(http_header, etag);
				g_free(etag);
				http_header = curl_slist_append(http_header,
					"Content-Type: text/calendar; charset=\"utf-8\"");
				http_header = curl_slist_append(http_header, "Expect:");
				http_header = curl_slist_append(
								http_header, "Transfer-Encoding:");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);
				curl_easy_setopt(curl, CURLOPT_URL, url);
				g_free(url);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, settings->file);
				curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(settings->file));
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
				res = curl_easy_perform(curl);
				if (res != 0) {
					error->code = code;
					error->str = g_strdup(chunk.memory);
					result = TRUE;
					settings->file = NULL;
				}
				else {
					long code;
					res = curl_easy_getinfo(
								curl, CURLINFO_RESPONSE_CODE, &code);
					if (code != 204) {
						error->code = code;
						error->str = g_strdup(chunk.memory);
						result = TRUE;
					}
				}
				curl_slist_free_all(http_header);
			}
			else {
				error->code = code;
				if (chunk.memory)
					error->str = g_strdup(chunk.memory);
				else
					error->str = g_strdup("No object found");
				result = TRUE;
			}
		}
	}
	if (chunk.memory)
		free(chunk.memory);
	if (headers.memory)
		free(headers.memory);
	curl_easy_cleanup(curl);
	return result;
}

