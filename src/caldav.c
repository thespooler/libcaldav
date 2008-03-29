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

#include "caldav.h"
#include "caldav-utils.h"
#include "get-caldav-report.h"
#include "add-caldav-object.h"
#include "delete-caldav-object.h"
#include "modify-caldav-object.h"
#include "get-display-name.h"
#include <curl/curl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct debug_curl debug_options;
static debug_options options = {1,0};
static caldav_error* error;

/**
 * @param curl An instance of libcurl.
 * @param url Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username:password@]host[:port]/url-path.
 * See (RFC1738).
 * @return FALSE (zero) mens URL does not reference a CalDAV calendar
 * resource. TRUE if the URL does reference a CalDAV calendar resource.
 */
static gboolean test_caldav_enabled(CURL* curl, gchar* url) {
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
		head = get_response_header("DAV", headers.memory);
		if (head && strstr(head, "calendar-access") != NULL) {
			enabled = TRUE;
			g_free(error);
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

/* 
 * @param settings An instance of caldav_settings. @see caldav_settings
 * @return TRUE if there was an error. Error can be in libcurl, in libcaldav,
 * or an error related to the CalDAV protocol.
 */
static gboolean make_caldav_call(caldav_settings* settings) {
	CURL* curl;
	gboolean result = FALSE;
	
	curl = curl_easy_init();
	if (!curl) {
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
	if (!test_caldav_enabled(curl, settings->url)) {
		settings->file = NULL;
		curl_easy_cleanup(curl);
		return TRUE;
	}
	error = (caldav_error *) malloc(sizeof(struct _caldav_error));
	memset(error, '\0', sizeof(struct _caldav_error));
	curl_easy_cleanup(curl);
	switch (settings->ACTION) {
		case GETALL: result = caldav_getall(settings, error); break;
		case GET: result = caldav_getrange(settings, error); break;
		case ADD: result = caldav_add(settings, error); break;
		case DELETE: result = caldav_delete(settings, error); break;
		case MODIFY: result = caldav_modify(settings, error); break;
		case GETCALNAME: result = caldav_getname(settings, error); break;
		default: break;
	}
	if (!result)
		g_free(error);
	return result;
}

/**
 * Function for adding a new event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_add_object(const char* object, const char* URL) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	init_caldav_settings(&settings);
	settings.file = g_strdup(object);
	settings.ACTION = ADD;
	if (options.debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (options.trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings);
	if (res) {
		caldav_response = CONFLICT;
	}
	else {
		caldav_response = OK;
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * Function for deleting an event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_delete_object(const char* object, const char* URL) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	init_caldav_settings(&settings);
	settings.file = g_strdup(object);
	settings.ACTION = DELETE;
	if (options.debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (options.trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings);
	if (res) {
		caldav_response = CONFLICT;
	}
	else {
		caldav_response = OK;
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * Function for modifying an event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_modify_object(const char* object, const char* URL) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	init_caldav_settings(&settings);
	settings.file = g_strdup(object);
	settings.ACTION = MODIFY;
	if (options.debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (options.trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings);
	if (res) {
		caldav_response = CONFLICT;
	}
	else {
		caldav_response = OK;
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * Function for getting a collection of events determined by time range.
 * @param result A pointer to struct _response where the result is to stored.
 * @see response. Caller is responsible for freeing the memory.
 * @param start time_t variable specifying start and end for range. Both
 * are included in range.
 * @param end time_t variable specifying start and end for range. Both
 * are included in range.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_get_object(
		response *result, time_t start, time_t end, const char* URL) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	if (!result) {
		result = malloc(sizeof(response *));
		memset(result, '\0', sizeof(response *));
	}
	init_caldav_settings(&settings);
	settings.ACTION = GET;
	settings.start = start;
	settings.end = end;
	if (options.debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (options.trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings);
	if (res) {
		result->msg = NULL;
		caldav_response = FORBIDDEN;
	}
	else {
		result->msg = g_strdup(settings.file);
		caldav_response = OK;
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * Function for getting all events from the collection.
 * @param result A pointer to struct _response where the result is to stored.
 * @see response. Caller is responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_getall_object(response* result, const char* URL) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	if (!result) {
		result = malloc(sizeof(response *));
		memset(result, '\0', sizeof(response *));
	}
	init_caldav_settings(&settings);
	settings.ACTION = GETALL;
	if (options.debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (options.trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings);
	if (res) {
		result->msg = NULL;
		caldav_response = FORBIDDEN;
	}
	else {
		result->msg = g_strdup(settings.file);
		caldav_response = OK;
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * Function for getting the stored display name for the collection.
 * @param result A pointer to struct _response where the result is to stored.
 * @see response. Caller is responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_get_displayname(response* result, const char* URL) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	if (!result) {
		result = malloc(sizeof(response *));
		memset(result, '\0', sizeof(response *));
	}
	init_caldav_settings(&settings);
	settings.ACTION = GETCALNAME;
	if (options.debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (options.trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings);
	if (res) {
		result->msg = NULL;
		caldav_response = FORBIDDEN;
	}
	else {
		result->msg = g_strdup(settings.file);
		caldav_response = OK;
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * Function to test wether a calendar resource is CalDAV enabled or not.
 * @param URL Defines CalDAV resource. Receiver is responsible for
 * freeing the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @result 0 (zero) means no CalDAV support, otherwise CalDAV support
 * detechted.
 */
int caldav_enabled_resource(const char* URL) {
	CURL* curl;
	caldav_settings settings;
	struct config_data data;

	init_caldav_settings(&settings);
	if (options.trace_ascii)
		data.trace_ascii = 1;
	else
		data.trace_ascii = 0;
	parse_url(&settings, URL);
	curl = curl_easy_init();
	if (!curl) {
		settings.file = NULL;
		return 0;
	}
	if (options.debug) {
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
		curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &data);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	if (settings.username) {
		gchar* userpwd = NULL;
		if (settings.password)
			userpwd = g_strdup_printf("%s:%s",
				settings.username, settings.password);
		else
			userpwd = g_strdup_printf("%s",	settings.username);
		curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
		g_free(userpwd);
	}
	gboolean res = test_caldav_enabled(curl, settings.url);
	free_caldav_settings(&settings);
	curl_easy_cleanup(curl);
	return (res) ? 1 : 0;
}

/**
 * Function which supports sending various options inside the library.
 * @param curl_options A struct debug_curl. See debug_curl.
 */
void caldav_set_options(struct debug_curl curl_options) {
	options = curl_options;
}

/** 
 * Function to call in case of errors.
 * Caller provides a pointer to a local caldav_error structure.
 * Caldav_get_error will initialize pointer if NULL.
 * Caller is responsible for freeing returned memory.
 * After the first call the internal error buffer is reset.
 * @param lib_error A pointer to a struct _caldav_error. @see _caldav_error
 * @return An initialized caldav_error pointer to memory where error
 * messages can be found from the last call to the library.
 */
caldav_error* caldav_get_error(caldav_error* lib_error) {
	if (!lib_error) {
		lib_error = (caldav_error *) malloc(sizeof(struct _caldav_error));
		memset(lib_error, '\0', sizeof(struct _caldav_error));
	}
	lib_error->code = error->code;
	if (error->str) {
		lib_error->str = (char *) malloc(strlen(error->str) + 1);
		memcpy(lib_error->str, error->str, strlen(error->str));
	}
	/*free_static_caldav_error();*/
	return lib_error;
}

/** 
 * Function for freeing memory for a previous initialization of a
 * caldav_error. @see caldav_get_error()
 * Caller provides a pointer to a local caldav_error structure.
 * @param lib_error A pointer to a struct _caldav_error. @see _caldav_error
 */
void caldav_free_error(caldav_error* lib_error) {
	if (lib_error->str)
		free(lib_error->str);
	free(lib_error);
	lib_error = NULL;
}
