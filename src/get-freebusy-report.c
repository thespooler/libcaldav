/* vim: set textwidth=80 tabstop=4 smarttab: */

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

#include "get-freebusy-report.h"
#include "response-parser.h"
#include <glib.h>
#include <curl/curl.h>
#include <string.h>

/**
 * A static literal string containing the first part of the calendar query.
 * The actual VEVENT to search for is added at runtime.
 */
static const char* getrange_request_head =
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
"<C:free-busy-query xmlns:D=\"DAV:\""
"                 xmlns:C=\"urn:ietf:params:xml:ns:caldav\">";

/**
 * A static literal string containing the last part of the calendar query
 */
static const char* getrange_request_foot =
"</C:free-busy-query>\r\n";

/**
 * Function for getting freebusy within a time range from collection.
 * @param settings A pointer to caldav_settings. @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return TRUE in case of error, FALSE otherwise.
 */
gboolean caldav_freebusy(caldav_settings* settings, caldav_error* error) {
	CURL* curl;
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE + 1];
	struct config_data data;
	struct MemoryStruct chunk;
	struct MemoryStruct headers;
	struct curl_slist *http_header = NULL;
	gboolean result = FALSE;
	gchar* request = NULL;

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
			"Content-Type: application/xml; charset=\"utf-8\"");
	http_header = curl_slist_append(http_header, "Depth: 1");
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
	request = g_strdup_printf(
		"%s\r\n<C:time-range start=\"%s\"\r\n end=\"%s\"/>\r\n%s",
			getrange_request_head, get_caldav_datetime(&settings->start),
			get_caldav_datetime(&settings->end), getrange_request_foot);
	/* enable uploading */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
	curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(request));
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "REPORT");
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
		if (! parse_response(CALDAV_REPORT, code, chunk.memory)) {
			/*
			 * Handle partly wrong server behavior.
			 * result must be raw Ical
			 */
			if (code == 200) {
				gchar* upcase;
				upcase = g_ascii_strup(chunk.memory, chunk.size);
				if (g_str_has_prefix(upcase, "BEGIN:VCALENDAR")) {
					settings->file = g_strdup(chunk.memory);
				}
				else {
					error->code = code;
					error->str = g_strdup(headers.memory);
					result = TRUE;
				}
				g_free(upcase);
			}
			else {
				error->code = code;
				error->str = g_strdup(headers.memory);
				result = TRUE;
			}
			
		}
		else {
			gchar* report;
			report = parse_caldav_report(chunk.memory, "calendar-data", "VFREEBUSY");
			settings->file = g_strdup(report);
			g_free(report);
		}
	}
	g_free(request);
	if (chunk.memory)
		free(chunk.memory);
	if (headers.memory)
		free(headers.memory);
	curl_slist_free_all(http_header);
	curl_easy_cleanup(curl);
	return result;
}
