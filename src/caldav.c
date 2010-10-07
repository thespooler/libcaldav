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
#include "options-caldav-server.h"
#include "get-freebusy-report.h"
#include <curl/curl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnutls/gnutls.h>
#include <gcrypt.h>
#include <pthread.h>
#include <errno.h>

GCRY_THREAD_OPTION_PTHREAD_IMPL;

static void init_runtime(runtime_info* info) {
	gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
    gnutls_global_init();
    if (! info)
		return;
    if (! info->error)
		info->error = g_new0(caldav_error, 1);
    if (! info->options) {
		info->options = g_new0(debug_curl, 1);
		info->options->trace_ascii = 1;
		info->options->debug = 0;
		info->options->verify_ssl_certificate = 1;
		info->options->use_locking = 1;
		info->options->custom_cacert = NULL;
    }
}

/**
 * @param curl An instance of libcurl.
 * @param settings Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. URL is part of the structure. [http://][username:password@]host[:port]/url-path.
 * See (RFC1738).
 * @return FALSE (zero) mens URL does not reference a CalDAV calendar
 * resource. TRUE if the URL does reference a CalDAV calendar resource.
 */
static gboolean test_caldav_enabled(CURL* curl,
				    caldav_settings* settings,
				    caldav_error* error) {
	return caldav_getoptions(curl, settings, NULL, error, TRUE);
}

/* 
 * @param settings An instance of caldav_settings. @see caldav_settings
 * @return TRUE if there was an error. Error can be in libcurl, in libcaldav,
 * or an error related to the CalDAV protocol.
 */
static gboolean make_caldav_call(caldav_settings* settings,
				 runtime_info* info) {
	CURL* curl;
	gboolean result = FALSE;

	g_return_val_if_fail(info != NULL, TRUE);

	curl = get_curl(settings);
	if (!curl) {
		info->error->str = g_strdup("Could not initialize libcurl");
		g_free(settings->file);
		settings->file = NULL;
		return TRUE;
	}
	if (!test_caldav_enabled(curl, settings, info->error)) {
		g_free(settings->file);
		settings->file = NULL;
		curl_easy_cleanup(curl);
		return TRUE;
	}
	curl_easy_cleanup(curl);
	switch (settings->ACTION) {
		case GETALL: result = caldav_getall(settings, info->error); break;
		case GET: result = caldav_getrange(settings, info->error); break;
		case ID_ADD:
		case ADD: result = caldav_add(settings, info->error); break;
		case ID_DELETE:
		case DELETE: result = caldav_delete(settings, info->error); break;
		case ID_MODIFY:
		case MODIFY: result = caldav_modify(settings, info->error); break;
		case GETCALNAME: result = caldav_getname(settings, info->error); break;
		case FREEBUSY: result = caldav_freebusy(settings, info->error); break;
		default: break;
	}
	return result;
}

/**
 * @deprecated since this function can cause lost updates.
 * Function for adding a new event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_add_object(const char* object,
				  const char* URL,
				  runtime_info* info) {
	return caldav_id_add_object(NULL, object, URL, info);
}

/**
 * Function for adding an event.
 * id will contain unique identification for object. Either ETAG or Location.
 * @param id @see CALDAV_ID
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @param info Pointer to a runtime_info structure. @see runtime_info
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_id_add_object(CALDAV_ID** id,
					 const char* object,
				     const char* URL,
				     runtime_info* info) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	g_return_val_if_fail(info != NULL, TRUE);

	init_runtime(info);
	init_caldav_settings(&settings);
	settings.file = g_strdup(object);
	settings.ACTION = ADD;
	if (info->options->debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (info->options->trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings, info);
	if (res) {
		if (info->error->code > 0) {
			switch (info->error->code) {
				case 403: caldav_response = FORBIDDEN; break;
				case 409: caldav_response = CONFLICT; break;
				case 423: caldav_response = LOCKED; break;
				case 501: caldav_response = NOTIMPLEMENTED; break;
				default: caldav_response = CONFLICT; break;
			}
		}
		else {
			/* fall-back to conflicting state */
			caldav_response = CONFLICT;
		}
	}
	else {
		if (settings.id && settings.id->Type == CALDAV_LOCATION_TYPE) {
			gchar* start = get_element_value(settings.file, "DTSTART");
			if (start) {
				settings.start = get_time_t(start);
				g_free(start);
				//start = get_caldav_datetime(&s);
				gchar* end = get_element_value(settings.file, "DTEND");
				if (end) {
					settings.end = get_time_t(end);
					g_free(end);
					//end = get_caldav_datetime(&e);
					g_free(settings.username);
					g_free(settings.password);
					g_free(settings.url);
					parse_url(&settings, URL);
					if (caldav_request_etag(&settings, info->error) == OK) {
						settings.id->Ident.Location.etag = g_strdup(settings.etag);
					}
				}
			}
		}
		caldav_response = OK;
	}
	if (id) {
		if (*id)
			caldav_free_caldav_id(id);
		*id = caldav_copy_caldav_id(settings.id);
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * Function for deleting an event.
 * @param id @see CALDAV_ID
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @param info Pointer to a runtime_info structure. @see runtime_info
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_id_delete_object(CALDAV_ID* id,
					 const char* object,
				     const char* URL,
				     runtime_info* info) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	g_return_val_if_fail(info != NULL, TRUE);

	init_runtime(info);
	init_caldav_settings(&settings);
	settings.file = g_strdup(object);
	if (id) {
		settings.ACTION = ID_DELETE;
		settings.id = caldav_copy_caldav_id(id);
	}
	else
		settings.ACTION = DELETE;
	if (info->options->debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (info->options->trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings, info);
	if (res) {
		if (info->error->code > 0) {
			switch (info->error->code) {
				case 403: caldav_response = FORBIDDEN; break;
				case 409: caldav_response = CONFLICT; break;
				case 423: caldav_response = LOCKED; break;
				case 501: caldav_response = NOTIMPLEMENTED; break;
				default: caldav_response = CONFLICT; break;
			}
		}
		else {
			/* fall-back to conflicting state */
			caldav_response = CONFLICT;
		}
	}
	else {
		caldav_response = OK;
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * @deprecated since this function can cause lost updates.
 * Function for deleting an event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_delete_object(const char* object,
				     const char* URL,
				     runtime_info* info) {
	return caldav_id_delete_object(NULL, object, URL, info);
}

/**
 * @deprecated since this function can cause lost updates.
 * Function for modifying an event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_modify_object(const char* object,
				     const char* URL,
				     runtime_info* info) {
	return caldav_id_modify_object(NULL, object, URL, info);
}

/**
 * Function for modifying an event.
 * id will contain unique identification for object. Either ETAG or Location.
 * @param id @see CALDAV_ID
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @param info Pointer to a runtime_info structure. @see runtime_info
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_id_modify_object(CALDAV_ID** id,
					 const char* object,
				     const char* URL,
				     runtime_info* info) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	g_return_val_if_fail(info != NULL, TRUE);

	init_runtime(info);
	init_caldav_settings(&settings);
	settings.file = g_strdup(object);
	if (id) {
		settings.ACTION = ID_MODIFY;
		if (*id) 
			settings.id = caldav_copy_caldav_id(*id);
		else
			settings.id = caldav_get_caldav_id();
	}
	else
		settings.ACTION = MODIFY;
	if (info->options->debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (info->options->trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings, info);
	if (res) {
		if (info->error->code > 0) {
			switch (info->error->code) {
				case 403: caldav_response = FORBIDDEN; break;
				case 409: caldav_response = CONFLICT; break;
				case 423: caldav_response = LOCKED; break;
				case 501: caldav_response = NOTIMPLEMENTED; break;
				default: caldav_response = CONFLICT; break;
			}
		}
		else {
			/* fall-back to conflicting state */
			caldav_response = CONFLICT;
		}
	}
	else {
		if (settings.id && settings.id->Type == CALDAV_LOCATION_TYPE) {
			gchar* start = get_element_value(settings.file, "DTSTART");
			if (start) {
				settings.start = get_time_t(start);
				g_free(start);
				//start = get_caldav_datetime(&s);
				gchar* end = get_element_value(settings.file, "DTEND");
				if (end) {
					settings.end = get_time_t(end);
					g_free(end);
					//end = get_caldav_datetime(&e);
					g_free(settings.username);
					g_free(settings.password);
					g_free(settings.url);
					parse_url(&settings, URL);
					if (caldav_request_etag(&settings, info->error) == OK) {
						settings.id->Ident.Location.etag = g_strdup(settings.etag);
					}
				}
			}
		}
		caldav_response = OK;
	}
	if (id) {
		if (*id)
			caldav_free_caldav_id(id);
		*id = caldav_copy_caldav_id(settings.id);
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
CALDAV_RESPONSE caldav_get_object(response *result,
				  time_t start,
				  time_t end,
				  const char* URL,
				  runtime_info* info) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	g_return_val_if_fail(info != NULL, TRUE);

	init_runtime(info);
	if (!result) {
		result = malloc(sizeof(response *));
		memset(result, '\0', sizeof(response *));
	}
	init_caldav_settings(&settings);
	settings.ACTION = GET;
	settings.start = start;
	settings.end = end;
	if (info->options->debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (info->options->trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings, info);
	if (res) {
		result->msg = NULL;
		if (info->error->code > 0) {
			switch (info->error->code) {
				case 403: caldav_response = FORBIDDEN; break;
				case 409: caldav_response = CONFLICT; break;
				case 423: caldav_response = LOCKED; break;
				case 501: caldav_response = NOTIMPLEMENTED; break;
				default: caldav_response = CONFLICT; break;
			}
		}
		else {
			/* fall-back to conflicting state */
			caldav_response = CONFLICT;
		}
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
CALDAV_RESPONSE caldav_getall_object(response* result,
				     const char* URL,
				     runtime_info* info) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	g_return_val_if_fail(info != NULL, TRUE);

	init_runtime(info);
	if (!result) {
		result = malloc(sizeof(response *));
		memset(result, '\0', sizeof(response *));
	}
	init_caldav_settings(&settings);
	settings.ACTION = GETALL;
	if (info->options->debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (info->options->trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings, info);
	if (res) {
		result->msg = NULL;
		if (info->error->code > 0) {
			switch (info->error->code) {
				case 403: caldav_response = FORBIDDEN; break;
				case 409: caldav_response = CONFLICT; break;
				case 423: caldav_response = LOCKED; break;
				case 501: caldav_response = NOTIMPLEMENTED; break;
				default: caldav_response = CONFLICT; break;
			}
		}
		else {
			/* fall-back to conflicting state */
			caldav_response = CONFLICT;
		}
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
CALDAV_RESPONSE caldav_get_displayname(response* result,
				       const char* URL,
				       runtime_info* info) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	g_return_val_if_fail(info != NULL, TRUE);

	init_runtime(info);
	if (!result) {
		result = malloc(sizeof(response *));
		memset(result, '\0', sizeof(response *));
	}
	init_caldav_settings(&settings);
	settings.ACTION = GETCALNAME;
	if (info->options->debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (info->options->trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings, info);
	if (res) {
		result->msg = NULL;
		if (info->error->code > 0) {
			switch (info->error->code) {
				case 403: caldav_response = FORBIDDEN; break;
				case 409: caldav_response = CONFLICT; break;
				case 423: caldav_response = LOCKED; break;
				case 501: caldav_response = NOTIMPLEMENTED; break;
				default: caldav_response = CONFLICT; break;
			}
		}
		else {
			/* fall-back to conflicting state */
			caldav_response = CONFLICT;
		}
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
int caldav_enabled_resource(const char* URL, runtime_info* info) {
	CURL* curl;
	caldav_settings settings;
	struct config_data data;

	g_return_val_if_fail(info != NULL, TRUE);

	init_runtime(info);
	init_caldav_settings(&settings);

	parse_url(&settings, URL);
	curl = get_curl(&settings);
	if (!curl) {
		info->error->code = -1;
		info->error->str = g_strdup("Could not initialize libcurl");
		settings.file = NULL;
		return TRUE;
	}

	if (info->options->trace_ascii)
		data.trace_ascii = 1;
	else
		data.trace_ascii = 0;
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;

	if (info->options->debug) {
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
		curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &data);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	gboolean res = test_caldav_enabled(curl, &settings, info->error);
	free_caldav_settings(&settings);
	curl_easy_cleanup(curl);
	return (res && (info->error->code == 0 || info->error->code == 200)) ? 1 : 0;
}

/**
 * Function for getting free/busy information.
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
CALDAV_RESPONSE caldav_get_freebusy(response *result,
				  time_t start,
				  time_t end,
				  const char* URL,
				  runtime_info* info) {
	caldav_settings settings;
	CALDAV_RESPONSE caldav_response;

	g_return_val_if_fail(info != NULL, TRUE);

	init_runtime(info);
	if (!result) {
		result = malloc(sizeof(response *));
		memset(result, '\0', sizeof(response *));
	}
	init_caldav_settings(&settings);
	settings.ACTION = FREEBUSY;
	settings.start = start;
	settings.end = end;
	if (info->options->debug)
		settings.debug = TRUE;
	else
		settings.debug = FALSE;
	if (info->options->trace_ascii)
		settings.trace_ascii = 1;
	else
		settings.trace_ascii = 0;
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;
	parse_url(&settings, URL);
	gboolean res = make_caldav_call(&settings, info);
	if (res) {
		result->msg = NULL;
		if (info->error->code > 0) {
			switch (info->error->code) {
				case 403: caldav_response = FORBIDDEN; break;
				case 409: caldav_response = CONFLICT; break;
				case 423: caldav_response = LOCKED; break;
				case 501: caldav_response = NOTIMPLEMENTED; break;
				default: caldav_response = CONFLICT; break;
			}
		}
		else {
			/* fall-back to conflicting state */
			caldav_response = CONFLICT;
		}
	}
	else {
		result->msg = g_strdup(settings.file);
		caldav_response = OK;
	}
	free_caldav_settings(&settings);
	return caldav_response;
}

/**
 * Function which supports sending various options inside the library.
 * @param curl_options A struct debug_curl. See debug_curl.
 */
void caldav_set_options(debug_curl curl_options) {
}

/** 
 * @deprecated Function to call in case of errors.
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
		lib_error = g_new0(caldav_error, 1);
	}
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
		g_free(lib_error->str);
	g_free(lib_error);
	lib_error = NULL;
}

/**
 * Function to call to get a list of supported CalDAV options for a server
 * @param URL Defines CalDAV resource. Receiver is responsible for
 * freeing the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @result A list of available options or NULL in case of any error.
 */
char** caldav_get_server_options(const char* URL, runtime_info* info) {
	CURL* curl;
	caldav_settings settings;
	response server_options;
	gchar** option_list = NULL;
	gchar** tmp;
	gboolean res = FALSE;

	g_return_val_if_fail(info != NULL, NULL);

	init_runtime(info);
	tmp = option_list = NULL;
	init_caldav_settings(&settings);

	parse_url(&settings, URL);
	curl = get_curl(&settings);
	if (!curl) {
		info->error->code = -1;
		info->error->str = g_strdup("Could not initialize libcurl");
		settings.file = NULL;
		return NULL;
	}
	if (info->options->use_locking)
		settings.use_locking = 1;
	else
		settings.use_locking = 0;

	res = caldav_getoptions(curl, &settings, &server_options, info->error, FALSE);
	if (res) {
		if (server_options.msg) {
			option_list = g_strsplit(server_options.msg, ", ", 0);
			tmp = &(*(option_list));
			while (*tmp) {
				g_strstrip(*tmp++);
			}
		}
	}
	free_caldav_settings(&settings);
	curl_easy_cleanup(curl);
	return (option_list) ? option_list : NULL;
}

/**
 * Function for getting an initialized runtime_info structure
 * @return runtime_info. @see runtime_info
 */
runtime_info* caldav_get_runtime_info() {
	runtime_info* rt_info;
	
	rt_info = g_new0(runtime_info, 1);
	rt_info->error = g_new0(caldav_error, 1);
	rt_info->options = g_new0(debug_curl, 1);
	rt_info->options->trace_ascii = 1;
  	rt_info->options->debug = 0;
  	rt_info->options->verify_ssl_certificate = 1;
  	rt_info->options->use_locking = 1;
  	rt_info->options->custom_cacert = NULL; 
	
	return rt_info;
}

/**
 * Function for freeing memory for a previous initialization of an info
 * structure
 * @param info Address to a pointer to a runtime_info structure. @see 
 * runtime_info
 */
void caldav_free_runtime_info(runtime_info** info) {
    runtime_info* ri;

    if (*info) {
		ri = *info;
		if (ri->error) {
		    if (ri->error->str)
			g_free(ri->error->str);
		    g_free(ri->error);
		    ri->error = NULL;
		}
		if (ri->options) {
		    if (ri->options->custom_cacert)
			g_free(ri->options->custom_cacert);
		    g_free(ri->options);
		    ri->options = NULL;
		}
		g_free(ri);
		*info = ri = NULL;
    }
}

/**
 * Function for getting an initialized response structure
 * @return response. @see _response
 */
response* caldav_get_response() {
	response* r;
	
	r = g_new0(response, 1);
	
	return r;
}

/**
 * Function for freeing memory for a previous initialization of an response
 * structure
 * @param info Address to a pointer to a response structure. @see 
 * _response
 */
void caldav_free_response(response** resp) {
	response* r;

	if (*resp) {
		r = *resp;
		if (r->msg)
			g_free(r->msg);
		g_free(r);
		*resp = r = NULL;
	}
}

/**
 * Function returning Object's ETAG
 * @param xml CalDAV envelope containing a CalDAV object
 * @return etag The CalDAV object's ETAG
 */
gchar* caldav_get_etag(const gchar* xml) {
	gchar* etag = NULL;
	
	if (xml) {
		gchar* tmp = g_strdup(xml);
		etag = get_etag(tmp);
		g_free(tmp);
	}
	return etag;
}

CALDAV_ID* caldav_get_caldav_id() {
	return g_new0(CALDAV_ID, 1);
}

void caldav_free_caldav_id(CALDAV_ID** id) {
	CALDAV_ID* tmp;
	
	if (*id) {
		tmp = *id;
		if (tmp->Type == CALDAV_ETAG_TYPE) {
			g_free(tmp->Ident.Etag.uri);
			g_free(tmp->Ident.Etag.etag);
		}
		else {
			g_free(tmp->Ident.Location.location);
			g_free(tmp->Ident.Location.etag);
		}
		g_free(tmp);
		*id = tmp = NULL;
	}
}

CALDAV_ID* caldav_copy_caldav_id(CALDAV_ID* src) {
	CALDAV_ID* dst = NULL;
	
	if (src) {
		dst = g_new0(CALDAV_ID, 1);
		if (src->Type == CALDAV_ETAG_TYPE) {
			dst->Type = CALDAV_ETAG_TYPE;
			if (src->Ident.Etag.etag)
				dst->Ident.Etag.etag = g_strdup(src->Ident.Etag.etag);
			if (src->Ident.Etag.uri) 
				dst->Ident.Etag.uri = g_strdup(src->Ident.Etag.uri);
		}
		else {
			dst->Type = CALDAV_LOCATION_TYPE;
			if (src->Ident.Location.location)
				dst->Ident.Location.location = g_strdup(src->Ident.Location.location);
			if (src->Ident.Location.etag)
				dst->Ident.Location.etag = g_strdup(src->Ident.Location.etag);
		}
	}
	return dst;
}
