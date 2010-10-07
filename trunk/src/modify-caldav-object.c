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
#include "lock-caldav-object.h"
#include "response-parser.h"
#include <glib.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Function for modifying an event.
 * @param settings A pointer to caldav_settings. @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return TRUE in case of error, FALSE otherwise.
 */
gboolean caldav_modify(caldav_settings* settings, caldav_error* error) {
	CURL* curl;
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE];
	struct config_data data;
	struct MemoryStruct chunk;
	struct MemoryStruct headers;
	struct curl_slist *http_header = NULL;
	gchar* etag;
	gchar* url = NULL;
	gchar* file;
	gboolean result = FALSE;
	gboolean LOCKSUPPORT = FALSE;
	gchar* lock_token = NULL;
	long code;

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
	if (settings->ACTION == ID_MODIFY) {
		if (! settings->id) {
			error->code = -1;
			error->str = g_strdup("missing etag and/or path");
			return TRUE;
		}
		else {
			if (settings->id->Type == CALDAV_ETAG_TYPE) {
				if (strcmp(settings->id->Ident.Etag.etag, "") == 0) {
					g_free(settings->id->Ident.Etag.etag);
					settings->id->Ident.Etag.etag = g_strdup("*");
				}
				etag = g_strconcat("\"", settings->id->Ident.Etag.etag, "\"", NULL);
				/*fprintf(stdout, "etag: %s\n", etag);*/
				url = g_strdup(remove_protocol(settings->id->Ident.Etag.uri));
			}
			else {
				etag = g_strconcat("\"", settings->id->Ident.Location.etag, "\"", NULL);
				/*fprintf(stdout, "lok: %s\n", etag);*/
				url = g_strdup(remove_protocol(settings->id->Ident.Location.location));
			}
			/**
			 * TODO Maybe check whether etag is the same
			 */
		}
	}
	if (settings->ACTION == MODIFY) {
		etag = find_etag(&chunk, settings, error);
		url = get_url(chunk.memory);
		if (etag) {
			gchar* host = get_host(settings->url);
			if (host) {
				file = g_strdup(url);
				g_free(url);
				url = g_strdup_printf("%s%s", host, file);
				g_free(file);
				g_free(host);
			}
			else {
				g_free(etag);
				g_free(url);
				url = NULL;
			}
		}
		else {
			g_free(url);
			url = NULL;
		}
	}

	if (url) {
		int lock = 0;
		caldav_error lock_error;

		file = g_strdup(etag);
		g_free(etag);
		etag = g_strdup_printf("If-Match: %s", file);
		g_free(file);
		http_header = curl_slist_append(http_header, etag);
		http_header = curl_slist_append(http_header,
			"Content-Type: text/calendar; charset=\"utf-8\"");
		http_header = curl_slist_append(http_header, "Expect:");
		http_header = curl_slist_append(
						http_header, "Transfer-Encoding:");
		if (settings->use_locking)
			LOCKSUPPORT = caldav_lock_support(settings, &lock_error);
		else
			LOCKSUPPORT = FALSE;
		if (LOCKSUPPORT) {
			lock_token = caldav_lock_object(url, settings, &lock_error);
			if (lock_token) {
				http_header = curl_slist_append(
					http_header, g_strdup_printf(
							"If: (%s)", lock_token));
			}
			/*
			 * If error code is 423 (Resource is LOCKED) bail out
			 */
			else if (lock_error.code == 423) {
				lock = -1;
			}
			/*
			 * If error code is 501 (Not implemented) we continue
			 * hoping for the best.
			 */
			else if (lock_error.code == 501) {
				lock_token = g_strdup("");
			}
			else {
				lock = -1;
			}
		}
		if (! LOCKSUPPORT || (LOCKSUPPORT && lock_token && lock_error.code != 423)) {
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,	WriteHeaderCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&headers);
			curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, (char *) &error_buf);
			data.trace_ascii = settings->trace_ascii;
			if (settings->debug) {
				curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
				curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &data);
				curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
			}
			curl_easy_setopt(curl, CURLOPT_URL, rebuild_url(settings, url));
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, settings->file);
			curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, 
						strlen(settings->file));
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
			curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1);
			curl_easy_setopt(curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
			res = curl_easy_perform(curl);
			if (LOCKSUPPORT && lock_token) {
				caldav_unlock_object(
						lock_token, url, settings, &lock_error);
			}
		}
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		g_free(lock_token);
		g_free(etag);
		if (res != 0 || lock < 0) {
			/* Is this a lock_error don't change error*/
			if (lock == 0 || lock_error.code == 423) {
				error->code = code;
				error->str = g_strdup(chunk.memory);
			}
			else {
				error->code = lock_error.code;
				error->str = g_strdup(lock_error.str);
			}
			result = TRUE;
			g_free(settings->file);
			settings->file = NULL;
		}
		else {
			if (! parse_response(CALDAV_PUT, code, chunk.memory)) {
				error->code = code;
				error->str = g_strdup(chunk.memory);
				result = TRUE;
			}
		}
		curl_slist_free_all(http_header);
	}
	else {
		/* error state */
		if (error->code == 0) {
			/* Error status not fetched yet */
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
			error->code = code;
			if (chunk.memory)
				error->str = g_strdup(chunk.memory);
			else
				error->str = g_strdup("No object found");
		}
		result = TRUE;
	}
	if (settings->id)
		caldav_free_caldav_id(&settings->id);
	settings->id = caldav_get_caldav_id();
	etag = get_response_header("ETAG", headers.memory, FALSE);
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
	curl_easy_cleanup(curl);
	return result;
}

