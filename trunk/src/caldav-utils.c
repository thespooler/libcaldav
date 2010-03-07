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

#include "caldav-utils.h"
#include "md5.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/**
 * This function is burrowed from the libcurl documentation
 * @param text
 * @param stream
 * @param ptr
 * @param size
 * @param nohex
 */
void dump(const char* text, FILE* stream, char* ptr, size_t size, char nohex) {
	size_t i;
	size_t c;

	unsigned int width=0x10;

	if(nohex)
		/* without the hex output, we can fit more on screen */
		width = 0x40;
	fprintf(stream, "%s, %zd bytes (0x%zx)\n", text, size, size);
	for(i=0; i<size; i+= width) {
		fprintf(stream, "%04zx: ", i);
		if(!nohex) {
			/* hex not disabled, show it */
			for(c = 0; c < width; c++) {
				if(i+c < size)
					fprintf(stream, "%02x ", ptr[i+c]);
				else
					fputs("   ", stream);
			}
		}
		for(c = 0; (c < width) && (i+c < size); c++) {
		/* check for 0D0A; if found, skip past and start a new line of output */
			if (nohex && (i+c+1 < size) && ptr[i+c]==0x0D && ptr[i+c+1]==0x0A) {
				i+=(c+2-width);
				break;
			}
			fprintf(stream, "%c",(ptr[i+c]>=0x20) && (ptr[i+c]<0x80)?ptr[i+c]:'.');
			/* check again for 0D0A, to avoid an extra \n if it's at width */
			if (nohex && (i+c+2 < size) && ptr[i+c+1]==0x0D && ptr[i+c+2]==0x0A) {
				i+=(c+3-width);
				break;
			}
		}
		fputc('\n', stream); /* newline */
	}
	fflush(stream);
}

/**
 * This function is burrowed from the libcurl documentation
 * @param handle
 * @param type
 * @param data
 * @param size
 * @param userp
 * @return
 */
int my_trace(CURL* handle, curl_infotype type, char* data, size_t size, void* userp) {
	struct config_data* config = (struct config_data *)userp;
	const char* text;
	(void)handle; /* prevent compiler warning */

	switch (type) {
		case CURLINFO_TEXT:
			fprintf(stderr, "== Info: %s", data);
		default: /* in case a new one is introduced to shock us */
			return 0;
		case CURLINFO_HEADER_OUT:
			text = "=> Send header";
			break;
		case CURLINFO_DATA_OUT:
			text = "=> Send data";
			break;
		case CURLINFO_SSL_DATA_OUT:
			text = "=> Send SSL data";
			break;
		case CURLINFO_HEADER_IN:
			text = "<= Recv header";
			break;
		case CURLINFO_DATA_IN:
			text = "<= Recv data";
			break;
		case CURLINFO_SSL_DATA_IN:
			text = "<= Recv SSL data";
			break;
	}
	dump(text, stderr, data, size, config->trace_ascii);
	return 0;
}

/**
 * This function is burrowed from the libcurl documentation
 * @param ptr
 * @param size
 * @return void* to memory region
 */
static void* myrealloc(void* ptr, size_t size) {
/* There might be a realloc() out there that doesn't like reallocing
 * NULL pointers, so we take care of it here
 * */
	if(ptr)
		return realloc(ptr, size);
	else
		return malloc(size);
}

/**
 * This function is burrowed from the libcurl documentation
 * @param ptr
 * @param size
 * @param nmemb
 * @param data
 * @return number of written bytes
 */
size_t WriteMemoryCallback(void* ptr, size_t size, size_t nmemb, void* data) {
	size_t realsize = size * nmemb;
	struct MemoryStruct* mem = (struct MemoryStruct *)data;
	mem->memory = (char *)myrealloc(mem->memory, mem->size + realsize + 1);
	
	if (mem->memory) {
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	return realsize;
}

/**
 * This function is burrowed from the libcurl documentation
 * @param ptr
 * @param size
 * @param nmemb
 * @param data
 * @return number of written bytes
 */
size_t WriteHeaderCallback(void* ptr, size_t size, size_t nmemb, void* data) {
	size_t realsize = size * nmemb;
	struct MemoryStruct* mem = (struct MemoryStruct *)data;
	mem->memory = (char *)myrealloc(mem->memory, mem->size + realsize + 1);
	
	if (mem->memory) {
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	return realsize;
}

/*
size_t ReadMemoryCallback(void* ptr, size_t size, size_t nmemb, void* data){
	struct MemoryStruct* mem = (struct MemoryStruct *)data;

	memcpy(ptr, mem->memory, mem->size);
	return mem->size;
}
*/

/**
 * Initialize caldav settings structure.
 * @param settings @see caldav_settings
 */
void init_caldav_settings(caldav_settings* settings) {
	settings->username = NULL;
	settings->password = NULL;
	settings->url = NULL;
	settings->file = NULL;
	settings->usehttps = FALSE;
	settings->custom_cacert = NULL;
	settings->verify_ssl_certificate = TRUE;
	settings->debug = FALSE;
	settings->trace_ascii = 1;
	settings->ACTION = UNKNOWN;
	settings->start = 0;
	settings->end = 0;
}

/**
 * Free momory assigned to caldav settings structure.
 * @param settings @see caldav_settings
 */
void free_caldav_settings(caldav_settings* settings) {
	if (settings->username) {
		g_free(settings->username);
		settings->username = NULL;
	}
	if (settings->password) {
		g_free(settings->password);
		settings->password = NULL;
	}
	if (settings->url) {
		g_free(settings->url);
		settings->url = NULL;
	}
	if (settings->file) {
		g_free(settings->file);
		settings->file = NULL;
	}
	if (settings->custom_cacert) {
		g_free(settings->custom_cacert);
		settings->custom_cacert = NULL;
	}
	settings->verify_ssl_certificate = TRUE;
	settings->usehttps = FALSE;
	settings->debug = FALSE;
	settings->trace_ascii = 1;
	settings->ACTION = UNKNOWN;
	settings->start = 0;
	settings->end = 0;
}

/**
 * Parse URL
 * @param settings @see caldav_settings
 * @param url String containing URL to collection
 */
void parse_url(caldav_settings* settings, const char* url) {
	char* start;
	char* end;
	char* pos;
	char* scheme;

	scheme = pos = end = start = NULL;
	if (!url)
		return;
	if ((pos = strstr(url, "//")) != NULL) {
		/* Does the URL use https ?*/
		if (!g_ascii_strncasecmp(url,"https",5)) {
				settings->usehttps=TRUE;
		}
		start = g_strdup(&(*(pos + 2)));
		if ((pos = strchr(start, '@')) != NULL) {
			/* username and/or password present */
			end = g_strdup(pos);
			scheme = g_strndup(start, strlen(start) - strlen(pos));
			if (start)
				g_free(start);
			if ((pos = strchr(scheme, ':')) != NULL) {
				/* username and password present */
				settings->username = 
					g_strndup(scheme, strlen(scheme) - 
							strlen(pos));
				pos = &(*(pos + 1));
				settings->password = g_strdup(pos);
			}
			else {
				/* only username present */
				settings->username = g_strdup(scheme);
				settings->password = NULL;
			}
			if (scheme)
				g_free(scheme);
			pos = end;
			end = &(*(end + 1));
			settings->url = g_strdup(end);
			if (pos)
				g_free(pos);
		}
		else {
			/* no username or password present */
			settings->url = g_strdup(start);
			if (start)
				g_free(start);
			settings->username = NULL;
			settings->password = NULL;
		}
	}
}

/**
 * Find a specific HTTP header from last request
 * @param header HTTP header to search for
 * @param headers String of HTTP headers from last request
 * @param lowcase Should string be returned in all lower case.
 * @return The header found or NULL
 */
#define MAX_TOKENS 2
gchar* get_response_header(
		const char* header, gchar* headers, gboolean lowcase) {
	gchar* line;
	gchar* head = NULL;
	gchar** buf;
	gchar* header_list;

	header_list = g_strdup(headers);
	line = strtok(header_list, "\r\n");
	if (line != NULL) {
		do  {
			buf = g_strsplit(line, ":", MAX_TOKENS);
			if (buf[1] != NULL) {
				if (g_ascii_strcasecmp(buf[0], header) == 0) {
					head = g_strdup(buf[1]);
					if (head)
						g_strstrip(head);
					g_strfreev(buf);
					break;
				}
			}
			g_strfreev(buf);
		} while ((line = strtok(NULL, "\r\n")) != NULL);
	}
	if (head)
		return (lowcase) ? g_ascii_strdown(head, -1) : head;
	else
		return NULL;
}

static const char* VCAL_HEAD =
"BEGIN:VCALENDAR\r\n"
"PRODID:-//CalDAV Calendar//NONSGML libcaldav//EN\r\n"
"VERSION:2.0\r\n";
static const char* VCAL_FOOT = "END:VCALENDAR";

/**
 * Parse response from CalDAV server. Internal function.
 * @param report Response from server
 * @param element XML element to find
 * @param type VCalendar element to find
 * @param wrap Is this the final parsing or just a part
 * @param recursive Stop after first match or not
 * @return the parsed result
 */
static gchar* parse_caldav_report_wrap(
		char* report, const char* element, const char* type,
			gboolean wrap, gboolean recursive) {
	char* pos;
	char* start;
	char* object;
	gchar* response;
	gchar* begin_type;
	gchar* end_type;
	gboolean keep_going = TRUE;

	begin_type = g_strdup_printf("BEGIN:%s", type);
	end_type = g_strdup_printf("END:%s", type);
	pos = start = object = response = NULL;
	while ((pos = strstr(report, element)) != NULL && keep_going) {
		pos = strchr(pos, '>');
		if (!pos) {
			break;
		}
		pos = &(*(pos + 1));
		pos = strstr(pos, begin_type);
		if (!pos) {
			break;
		}
		object = &(*(pos + strlen(begin_type)));
		object = g_strchug(object);
		start = g_strdup(object);
		if ((pos = strstr(start, end_type)) == NULL) {
			g_free(start);
			break;
		}
		object = g_strndup(start, strlen(start) - strlen(pos));
		if (response)
			response = g_strdup_printf("%s%s\r\n%s%s\r\n",
				response, begin_type, object, end_type);
		else {
			if (wrap)
				response = g_strdup_printf("%s%s\r\n%s%s\r\n",
					VCAL_HEAD, begin_type, object, end_type);
			else
				response = g_strdup_printf("%s\r\n%s%s\r\n",
					begin_type, object, end_type);
		}
		pos = strchr(pos, '>');
		report = g_strdup(&(*(pos + 1)));
		g_free(start);
		g_free(object);
		if (!recursive)
			keep_going = FALSE;
	}
	g_free(begin_type);
	g_free(end_type);
	if (wrap)
		if (response)
			response = g_strdup_printf("%s%s", response, VCAL_FOOT);
	return response;
}

/**
 * Parse response from CalDAV server
 * @param report Response from server
 * @param element XML element to find
 * @param type VCalendar element to find
 * @return the parsed result
 */
gchar* parse_caldav_report(char* report, const char* element, const char* type) {
	gchar* response = NULL;
	gchar* timezone = NULL;
	gchar* temp = NULL;

	if (!report || !element || !type)
		return NULL;
	/* test for VTIMEZONE.
	 * Only the first found will be used and this will then
	 * be the time zone for the entire report
	 */
	timezone = parse_caldav_report_wrap(
			report, element, "VTIMEZONE", FALSE, FALSE);
	if (timezone) {
			response = g_strdup_printf("%s%s", VCAL_HEAD, timezone);
			g_free(timezone);
			temp = parse_caldav_report_wrap(report, element, type, FALSE, TRUE);
			if (temp) {
					response = g_strdup_printf(
							"%s%s%s", response, temp, VCAL_FOOT);
			}
			else {
				g_free(response);
				return NULL;
			}
	}
	else
		response = parse_caldav_report_wrap(report, element, type, TRUE, TRUE);
	return response;
}

/**
 * Convert a time_t variable to CalDAV DateTime
 * @param time a specific date and time
 * @return the CalDAV DateTime
 */
gchar* get_caldav_datetime(time_t* time) {
	struct tm *current;
	gchar* datetime;

	current = localtime(time);
	datetime = g_strdup_printf("%d%.2d%.2dT%.2d%.2d%.2dZ",
			current->tm_year + 1900, current->tm_mon + 1, current->tm_mday,
			current->tm_hour, current->tm_min, current->tm_sec);
	return datetime;
}

/**
 * Create a random text string, using MD5. @see caldav_md5_hex_digest()
 * @param text some text to randomize
 * @return MD5 hash of text
 */
gchar* random_file_name(gchar* text) {
	unsigned char* name;
	gchar md5sum[33];

	name = (unsigned char *) g_strdup(text);
	caldav_md5_hex_digest(md5sum, name);
	return g_strdup(md5sum);
}

/**
 * Does the event contain a UID element or not. If not add it.
 * @param object A specific event
 * @return event, eventually added UID
 */
gchar* verify_uid(gchar* object) {
	gchar* uid;
	gchar* newobj;
	gchar* pos;

	newobj = g_strdup(object);
	uid = get_response_header("uid", object, TRUE);
	if (!uid) {
		object = g_strdup(newobj);
		g_free(newobj);
		pos = strstr(object, "END:VEVENT");
		newobj = g_strndup(object, strlen(object) - strlen(pos));
		newobj = g_strchomp(newobj);
		uid = random_file_name(object);
		newobj = g_strdup_printf("%s\r\nUID:libcaldav-%s@tempuri.org\r\n%s",
					newobj, uid, pos);
		g_free(object);
	}
	if (uid)
		g_free(uid);
	newobj = g_strchomp(newobj);
	return newobj;
}

/**
 * Fetch a URL from a XML element
 * @param text String
 * @return URL
 */
#define ELEM_HREF "href>"
gchar* get_url(gchar* text) {
	gchar* pos;
	gchar* url = NULL;

	if ((pos = strstr(text, ELEM_HREF)) == NULL)
		return url;
	pos = &(*(pos + strlen(ELEM_HREF)));
	url = g_strndup(pos, strlen(pos) - strlen(strchr(pos, '<')));
	return url;
}

/**
 * Fetch any element from XML
 * @param text String
 * @param tag The element to look for
 * @return element
 */
gchar* get_tag(const gchar* tag, gchar* text) {
	gchar* pos;
	gchar* res = NULL;
	gchar* the_tag = NULL;

	the_tag = g_strdup_printf("<%s>", tag);
	if ((pos = strstr(text, the_tag)) == NULL)
		return res;
	pos = &(*(pos + strlen(the_tag)));
	res = g_strndup(pos, strlen(pos) - strlen(strchr(pos, '<')));
	return res;	
}

/**
 * Fetch the etag element from XML
 * @param text String
 * @return etag
 */
#define ELEM_ETAG "getetag"
gchar* get_etag(gchar* text) {
	return get_tag(ELEM_ETAG, text);	
}

/**
 * Fetch host from URL
 * @param url URL
 * @return host
 */
gchar* get_host(gchar* url) {
	gchar** buf;
	gchar* result = NULL;

	buf = g_strsplit(url, "/", 2);
	if (buf[0]) {
		result = g_strdup(buf[0]);
	}
	g_strfreev(buf);
	return result;
}

/**
 * rebuild a ral URL with https if needed from the settings
 * @param settings caldav_settings
 * @return URL
 */

gchar* rebuild_url(caldav_settings* settings){
    gchar* url = NULL;
    gchar* mystr = NULL;
	if (settings->usehttps) {
		mystr = "https://";
	} else {
		mystr = "http://";
	}
	/* XXX We probably don't need to pass username and password in the URL as
	 * they have to be set with the curl-way */
/*
    if (settings->username && settings->password) {
        url = g_strdup_printf("%s%s:%s@%s",
                mystr,settings->username, settings->password, settings->url);
    }
    else if (settings->username) {
        url = g_strdup_printf("%s%s@%s",
                mystr,settings->username, settings->url);
    }
    else {*/
        url = g_strdup_printf("%s%s", mystr,settings->url);
/*    }*/
	return url;
}
