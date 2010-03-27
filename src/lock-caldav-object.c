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

#include "lock-caldav-object.h"
#include "options-caldav-server.h"
#include <glib.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * A static literal string containing the lock query.
 */
static char* lock_query =
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
"<D:lockinfo xmlns:D=\"DAV:\">"
"  <D:lockscope><D:exclusive/></D:lockscope>"
"  <D:locktype><D:write/></D:locktype>"
"</D:lockinfo>";

/**
 * Function which requests a lock on a calendar resource
 * @param URI The resource to request lock on.
 * @param settings @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return The Lock-Token or NULL in case of error
 */
gchar* caldav_lock_object(
		gchar* URI, caldav_settings* settings, caldav_error* error) {
	CURL* curl;
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE];
	struct config_data data;
	struct MemoryStruct chunk;
	struct MemoryStruct headers;
	struct curl_slist *http_header = NULL;
	gchar* lock_token = NULL;

	if (! caldav_lock_support(settings, error))
		return lock_token;
	chunk.memory = NULL; /* we expect realloc(NULL, size) to work */
	chunk.size = 0;    /* no data at this point */
	headers.memory = NULL;
	headers.size = 0;
	http_header = curl_slist_append(http_header,
			"Content-Type: application/xml; charset=\"utf-8\"");
	http_header = curl_slist_append(http_header, "Timeout: Second-300");
	http_header = curl_slist_append(http_header, "Expect:");
	http_header = curl_slist_append(http_header, "Transfer-Encoding:");
	data.trace_ascii = settings->trace_ascii;
	curl = curl_easy_init();
	if (!curl) {
		error->code = -1;
		error->str = g_strdup("Could not initialize libcurl");
		settings->file = NULL;
		return lock_token;
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
	curl_easy_setopt(curl, CURLOPT_URL, URI);
	/* enable uploading */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, lock_query);
	curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(lock_query));
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "LOCK");
	res = curl_easy_perform(curl);
	curl_slist_free_all(http_header);
	if (res != 0) {
		error->code = -1;
		error->str = g_strdup_printf("%s", error_buf);
		settings->file = NULL;
	}
	else {
		long code;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (code != 200) {
			error->code = code;
			error->str = g_strdup(chunk.memory);
		}
		else {
			lock_token = get_response_header(
						"Lock-Token", headers.memory, FALSE);
		}
	}
	if (chunk.memory)
		free(chunk.memory);
	if (headers.memory)
		free(headers.memory);
	curl_easy_cleanup(curl);
	return lock_token;
}

/**
 * Function which requests to have a lock removed from a calendar resource
 * @param lock_token A privious aquired Lock-Token
 * @param URI The resource to request unlock on.
 * @param settings @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return False in case the lock could not be removed. True otherwise.
 */
gboolean caldav_unlock_object(gchar* lock_token, gchar* URI, 
			caldav_settings* settings, caldav_error* error) {
	CURL* curl;
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE];
	struct config_data data;
	struct MemoryStruct chunk;
	struct MemoryStruct headers;
	struct curl_slist *http_header = NULL;
	gboolean result = FALSE;

	if (! caldav_lock_support(settings, error))
		return result;
	chunk.memory = NULL; /* we expect realloc(NULL, size) to work */
	chunk.size = 0;    /* no data at this point */
	headers.memory = NULL;
	headers.size = 0;
	http_header = curl_slist_append(http_header, 
			g_strdup_printf("Lock-Token: %s", lock_token));
	http_header = curl_slist_append(http_header, "Expect:");
	http_header = curl_slist_append(http_header, "Transfer-Encoding:");
	data.trace_ascii = settings->trace_ascii;
	curl = curl_easy_init();
	if (!curl) {
		error->code = -1;
		error->str = g_strdup("Could not initialize libcurl");
		settings->file = NULL;
		return result;
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
	curl_easy_setopt(curl, CURLOPT_URL, URI);
	/* enable uploading */
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "UNLOCK");
	res = curl_easy_perform(curl);
	curl_slist_free_all(http_header);
	if (res != 0) {
		error->code = -1;
		error->str = g_strdup_printf("%s", error_buf);
		settings->file = NULL;
	}
	else {
		long code;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (code != 204) {
			error->code = code;
			error->str = g_strdup(chunk.memory);
		}
		else {
			result = TRUE;
		}
	}
	if (chunk.memory)
		free(chunk.memory);
	if (headers.memory)
		free(headers.memory);
	curl_easy_cleanup(curl);
	return result;
}

/**
 * Function to test whether the server supports locking or not. Searching
 * for PROP LOCK. If LOCK is present then according to RFC4791 PROP UNLOCK
 * must also be present.
 * @param settings @see caldav_settings
 * @param error A pointer to caldav_error. @see caldav_error
 * @return True if locking is supported by the server. False otherwise
 */
gboolean caldav_lock_support(caldav_settings* settings, caldav_error* error) {
	gboolean found = FALSE;
	gchar* url = NULL;
    gchar* mystr = NULL;
	if (settings->usehttps) {
		mystr =  g_strdup("https://");
	} else {
		mystr =  g_strdup("http://");
	}
	

	if (settings->username && settings->password) {
		url = g_strdup_printf("%s%s:%s@%s",
				mystr, settings->username, settings->password, settings->url);
	}
	else if (settings->username) {
		url = g_strdup_printf("%s%s@%s", 
				mystr, settings->username, settings->url);
	}
	else {
		url = g_strdup_printf("%s%s", mystr, settings->url);
	}
	gchar** options = caldav_get_server_options(url);
	while (*options) {
		if (strcmp(*options++, "LOCK") == 0) {
			found = TRUE;
			break;
		}
	}			
	g_free(mystr);
	return found;
}


