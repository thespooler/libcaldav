/* vim: set textwidth=80 tabstop=4: */

/**
 * @file caldav.h
 * @brief interface to the caldav library.
 * The library conforms to RFC4791. For further information follow this
 * link http://www.ietf.org/rfc/rfc4791.txt
 */

/** 
 * @mainpage
 * This document is the documentation for the public interface to libcaldav.
 * If you want to study the implementation look for the developers API.
 *
 * The libray and documentation is Copyright (c) 2008 Michael Rasmussen
 * (mir@datanom.net)
 *
 * License for the source code.
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
 *
 * License for the documentation.
 *
 * Permission is granted to copy, distribute and/or modify this document
 * under the terms of the GNU Free Documentation License, Version 1.2
 * or any later version published by the Free Software Foundation;
 * with no Invariant Sections, no Front-Cover Texts, and no Back-Cover
 * Texts.
 */

#ifndef __CALDAV_H__
#define __CALDAV_H__

#include <time.h>
#include <glib.h>

/* For debug purposes */
/**
 * @struct debug_curl
 * A struct used to set internal options in the library
 */
struct debug_curl {
  char		trace_ascii; /** @var char trace_ascii
					 	  * 0 or 1
					 	  */
  char		debug;       /** @var char debug
					 	  * 0 or 1
					 	  */
  gboolean	verify_ssl_certificate;
  gchar*	custom_cacert; 
};

/**
 * @typedef struct _caldav_error caldav_error
 * Pointer to a caldav_error structure
 */
typedef struct _caldav_error caldav_error;

/**
 * @struct _caldav_error
 * A struct for storing error codes and messages
 */
struct _caldav_error {
	long code; /**
				* @var long code
				* if < 0 internal error > 0 CalDAV protocol error.
				*/
	char* str; /** @var char* str
				* For storing human readable error message
				*/
};

/* CalDAV is defined in RFC4791 */

/* Buffer to hold response */
/**
 * @typedef struct _response response
 * Pointer to a _response structure
 */
typedef struct _response response;

/**
 * @struct _response
 * A struct used for returning messages from the library to users
 */
struct _response {
	char* msg; /** @var char* msg
				* String for storing response
				*/
};

/**
 * @enum CALDAV_ACTION specifies supported CalDAV actions.
 * UNKNOWN. An unknown action.
 * ADD. Add a CalDAV calendar object.
 * DELETE. Delete a CalDAV calendar object.
 * MODIFY. Modify a CalDAV calendar object.
 * GET. Get one or more CalDAV calendar object(s).
 * GETALL. Get all CalDAV calendar objects.
 */
typedef enum {
	UNKNOWN,
	ADD,
	DELETE,
	MODIFY,
	GET,
	GETALL,
	GETCALNAME,
	ISCALDAV,
	OPTIONS
} CALDAV_ACTION;

/**
 * @enum CALDAV_RESPONSE specifies CalDAV error states.
 * OK (HTTP 200). Request was satisfied.
 * FORBIDDEN (HTTP 403). Access not allowed. Dont repeat request.
 * CONFLICT (HTTP 409). Conflict between current state of CalDAV collection
 * and request. Client must solve the conflict and then resend request.
 * LOCKED (HTTP 423). Locking failed.
 */
typedef enum {
	OK,
	FORBIDDEN,
	CONFLICT,
	LOCKED
} CALDAV_RESPONSE;


#ifndef __CALDAV_USERAGENT
#define __CALDAV_USERAGENT "libcurl-agent/0.1"
#endif


/**
 * Function for adding a new event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_add_object(const char* object, const char* URL);

/**
 * Function for deleting an event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_delete_object(const char* object, const char* URL);

/**
 * Function for modifying an event.
 * @param object Appointment following ICal format (RFC2445). Receiver is
 * responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_modify_object(const char* object, const char* URL);

/**
 * Function for getting a collection of events determined by time range.
 * @param result A pointer to struct _response where the result is to stored.
 * @see response. Caller is responsible for freeing the memory.
 * @param start time_t variable specifying start for range. Included in search.
 * @param end time_t variable specifying end for range. Included in search.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_get_object(
	response* result, time_t start, time_t end, const char* URL);

/**
 * Function for getting all events from the collection.
 * @param result A pointer to struct _response where the result is to stored.
 * @see response. Caller is responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_getall_object(response* result, const char* URL);

/**
 * Function for getting the stored display name for the collection.
 * @param result A pointer to struct _response where the result is to stored.
 * @see response. Caller is responsible for freeing the memory.
 * @param URL Defines CalDAV resource. Receiver is responsible for freeing
 * the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @return Ok, FORBIDDEN, or CONFLICT. @see CALDAV_RESPONSE
 */
CALDAV_RESPONSE caldav_get_displayname(response* result, const char* URL);

/**
 * Function to test wether a calendar resource is CalDAV enabled or not.
 * @param URL Defines CalDAV resource. Receiver is responsible for
 * freeing the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @result 0 (zero) means no CalDAV support, otherwise CalDAV support
 * detechted.
 */
int caldav_enabled_resource(const char* URL);

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
caldav_error* caldav_get_error(caldav_error* lib_error);

/** 
 * Function for freeing memory for a previous initialization of a
 * caldav_error. @see caldav_get_error()
 * Caller provides a pointer to a local caldav_error structure.
 * @param lib_error A pointer to a struct _caldav_error. @see _caldav_error
 */
void caldav_free_error(caldav_error* lib_error);

/* Setting various options in library */

/**
 * Function which supports sending various options inside the library.
 * @param curl_options A struct debug_curl. See debug_curl.
 */
void caldav_set_options(struct debug_curl curl_options);

/**
 * Function to call to get a list of supported CalDAV options for a server
 * @param URL Defines CalDAV resource. Receiver is responsible for
 * freeing the memory. [http://][username[:password]@]host[:port]/url-path.
 * See (RFC1738).
 * @result A list of available options or NULL in case of any error.
 */
char** caldav_get_server_options(const char* URL);

#endif
