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

#include "get-caldav-report.h"
#include <glib.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * A static literal string containing the calendar query for fetching
 * all events from collection.
 */
static const char* getall_request =
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
"<C:calendar-query xmlns:D=\"DAV:\""
"                 xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
" <D:prop>"
"   <D:getetag/>"
"   <C:calendar-data/>"
" </D:prop>"
" <C:filter>"
"   <C:comp-filter name=\"VCALENDAR\">"
"     <C:comp-filter name=\"VEVENT\"/>"
"   </C:comp-filter>"
" </C:filter>"
"</C:calendar-query>\r\n";

/**
 * A static literal string containing the first part of the calendar query.
 * The actual VEVENT to search for is added at runtime.
 */
static const char* getrange_request_head =
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
/*"<C:calendar-query xmlns:D=\"DAV:\""
"                 xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
" <D:prop>"*/
"<C:calendar-query xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
" <D:prop xmlns:D=\"DAV:\">"
/*"   <D:getetag/>"*/
"   <C:calendar-data/>"
" </D:prop>"
" <C:filter>"
"   <C:comp-filter name=\"VCALENDAR\">"
"     <C:comp-filter name=\"VEVENT\">";

/**
 * A static literal string containing the last part of the calendar query
 */
static const char* getrange_request_foot =
"     </C:comp-filter>"
"   </C:comp-filter>"
" </C:filter>"
"</C:calendar-query>\r\n";

/**
 * Function for getting all events from collection.
 * @param settings A pointer to caldav_settings. @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return TRUE in case of error, FALSE otherwise.
 */
gboolean caldav_getall(caldav_settings* settings, caldav_error* error) {
	CURL* curl;
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE];
	struct config_data data;
	struct MemoryStruct chunk;
	struct MemoryStruct headers;
	struct curl_slist *http_header = NULL;
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
	if (settings->verify_ssl_certificate)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
	else
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	if (settings->custom_cacert)
		curl_easy_setopt(curl, CURLOPT_CAINFO, settings->custom_cacert);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, WriteHeaderCallback);
	/* we pass our 'headers' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&headers);
	/* enable uploading */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, getall_request);
	curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(getall_request));
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);
	/* some servers don't like requests that are made without a user-agent
	 * field, so we provide one */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, __CALDAV_USERAGENT);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, (char *) &error_buf);
	if (settings->debug) {
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
		curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &data);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "REPORT");
	curl_easy_setopt(curl, CURLOPT_URL, rebuild_url(settings, NULL));
	res = curl_easy_perform(curl);
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
			error->str = g_strdup(headers.memory);
			result = TRUE;
		}
		else {
			gchar* report;
			report = parse_caldav_report(
						chunk.memory, "calendar-data", "VEVENT");
			settings->file = g_strdup(report);
		}
	}
	if (chunk.memory)
		free(chunk.memory);
	if (headers.memory)
		free(headers.memory);
	curl_slist_free_all(http_header);
	curl_easy_cleanup(curl);
	return result;
}

/**
 * Function for getting all events within a time range from collection.
 * @param settings A pointer to caldav_settings. @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return TRUE in case of error, FALSE otherwise.
 */
gboolean caldav_getrange(caldav_settings* settings, caldav_error* error) {
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
	if (settings->verify_ssl_certificate)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
	else
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	if (settings->custom_cacert)
		curl_easy_setopt(curl, CURLOPT_CAINFO, settings->custom_cacert);
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
	curl_easy_setopt(curl, CURLOPT_USERAGENT, __CALDAV_USERAGENT);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, (char *) &error_buf);
	if (settings->debug) {
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
		curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &data);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	curl_easy_setopt(curl, CURLOPT_URL, rebuild_url(settings, NULL));
	request = g_strdup_printf(
		"%s\r\n<C:time-range start=\"%s\"\r\n end=\"%s\"/>\r\n%s",
			getrange_request_head, get_caldav_datetime(&settings->start),
			get_caldav_datetime(&settings->end), getrange_request_foot);
	/* enable uploading */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
	curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(request));
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "REPORT");
	res = curl_easy_perform(curl);
	if (res != 0) {
		error->code = -1;
		error->str = g_strdup_printf("%s", error_buf);
		settings->file = NULL;
		result = TRUE;
	}
	else {
		gchar* report;
		report = parse_caldav_report(chunk.memory, "calendar-data", "VEVENT");
		settings->file = g_strdup(report);
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

