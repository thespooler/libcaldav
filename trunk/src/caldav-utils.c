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
#include "response-parser.h"
#include "caldav.h"
#include "md5.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>
#include <ctype.h>

typedef struct {
	char* NS;
	char* prefix;
} Namespace;
static gchar DAV[] = "DAV:";
static gchar CALDAV[] = "urn:ietf:params:xml:ns:caldav";

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
	settings->trace_ascii = TRUE;
	settings->use_locking = TRUE;
	settings->ACTION = UNKNOWN;
	settings->start = 0;
	settings->end = 0;
	settings->etag = NULL;
	settings->id = NULL;
}

/**
 * Free memory assigned to caldav settings structure.
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
	settings->trace_ascii = TRUE;
	settings->use_locking = TRUE;
	settings->ACTION = UNKNOWN;
	settings->start = 0;
	settings->end = 0;
	if (settings->etag) {
		g_free(settings->etag);
		settings->etag = NULL;
	}
	if (settings->id)
		caldav_free_caldav_id(&settings->id);
}

static gchar* place_after_hostname(const gchar* start, const gchar* stop) {
	gchar* newpos = NULL;
	gchar* pos = (gchar *) stop;
	gboolean digit = TRUE;
	
	if (pos && stop && strcmp(start, pos) != 0) {
		while (*pos != ':' && strcmp(start, pos) != 0)
			--pos;
		if (pos > start) {
			gchar* tmp = (gchar *) pos + 1;
			/* is pos++ a port number */
			while (*tmp != '/' && digit) {
				if (isdigit(*tmp) != 0) {
					digit = TRUE;
					tmp++;
				}
				else
					digit = FALSE;
			}
			if (digit) {
				/* pos was a port number */
				while (*pos != '@' && strcmp(start, pos) != 0)
					--pos;
				if (strcmp(start, pos) != 0)
					newpos = pos;
			}
			else {
				while (*pos != '@' && pos != stop)
					pos++;
				if (pos != stop)
					newpos = pos;
			}
		}
		else {
			/* is a username present */
			gchar* tmp = NULL;
			while (*pos != '/' && pos != stop) {
				if (*pos == '@')
					tmp = pos;
				pos++;
			}
			if (tmp && pos != stop)
				newpos = tmp;
		}
	}
	return newpos;
}

/**
 * Parse URL
 * @param settings @see caldav_settings
 * @param url String containing URL to collection
 */
void parse_url(caldav_settings* settings, const char* url) {
	char* start;
	char* pos;
	char* end;
	char* login;

	login = pos = end = start = NULL;
	if (!url)
		return;
	if ((pos = strstr(url, "//")) != NULL) {
		/* Does the URL use https ?*/
		if (!g_ascii_strncasecmp(url,"https",5) && settings->usehttps == FALSE) {
				settings->usehttps=TRUE;
		}
		start = g_strdup(&(*(pos + 2)));
		if ((pos = place_after_hostname(start, strrchr(start, '\0') - 1)) != NULL) {
			/* username and/or password present */
			login = g_strndup(start, pos - start);
			end = pos;
			if ((pos = strrchr(login, ':')) != NULL) {
				/* both username and password is present */
				settings->username = g_strndup(login, pos - login);
				settings->password = g_strdup(++pos);
			}
			else {
				/* only username present */
				settings->username = g_strdup(login);
				settings->password = NULL;
			}
			g_free(login);
			settings->url = g_strdup(++end);
		}
		else {
			/* no username or password present */
			settings->url = g_strdup(start);
			settings->username = NULL;
			settings->password = NULL;
		}
		g_free(start);
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
	gchar* oldhead = NULL;
	gchar** buf;
	gchar* header_list;
	gchar* saveptr;

	header_list = g_strdup(headers);
	line = strtok_r(header_list, "\r\n", &saveptr);
	if (line != NULL) {
		do  {
			buf = g_strsplit(line, ":", MAX_TOKENS);
			if (buf[1] != NULL) {
				if (g_ascii_strcasecmp(buf[0], header) == 0) {
					if (head) {
						oldhead = head;
						head = g_strconcat(head, ", ", buf[1], NULL);
						g_free(oldhead);
					}
					else
						head = g_strdup(buf[1]);
					if (head)
						g_strstrip(head);
				}
			}
			g_strfreev(buf);
		} while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL);
	}
	g_free(header_list);
	if (head)
		return (lowcase) ? g_ascii_strdown(head, -1) : head;
	else
		return NULL;
}

/**
 * Find used namespace prefix in response
 * @param text String
 * @return gchar** list of struct Namespace
 */
static Namespace** getNamespace(gchar* text) {
	Namespace** list = NULL;
	Namespace* ns;
	gchar* prefix;
	gchar **nstoken, **head;
	int pos = 0;
	/* 
	 * haveNS == 1 => DAV
	 * haveNS == 2 => CALDAV
	 * haveNS == 3 => DAV, CALDAV
	 */
	int haveNS = 0;
	
	if (! text)
		return NULL;
	nstoken = g_strsplit(text, "xmlns", 0);
	if (! nstoken)
		return NULL;
	head = nstoken;
	while (*nstoken && haveNS < 3) {
		gchar* token = *nstoken;
		while (*token) {
			if (*token++ == ':')
				break;
		}
		if (token && strlen(token) > 0) {
			if ((prefix = g_strstr_len(token, -1, DAV)) != NULL && haveNS != 1) {
				ns = g_new0(Namespace, 1);
				ns->NS = g_strdup(DAV);
				ns->prefix = g_strndup(token, prefix - token - 2);
				list = g_realloc(list, sizeof(Namespace));
				list[pos++] = ns;
				haveNS += 1;
			}
			else if ((prefix = g_strstr_len(token, -1, CALDAV)) != NULL && haveNS != 2) {
				/*pos = (list) ? 1 : 0;*/
				ns = g_new0(Namespace, 1);
				ns->NS = g_strdup(CALDAV);
				ns->prefix = g_strndup(token, prefix - token - 2);
				list = g_realloc(list, sizeof(Namespace) * (pos + 1));
				list[pos++] = ns;
				haveNS += 2;
			}
		}
		nstoken += 1;
	}
	g_strfreev(head);
	if (list)
		list[pos] = NULL;

	return list;
}

static void freeNamespace(Namespace** ns) {
	Namespace** tmp = ns;
	
	if (! tmp)
		return;
	while (*tmp) {
		g_free((*tmp)->NS);
		g_free((*tmp)->prefix);
		g_free(*tmp);
		tmp += 1;
	}
	ns = tmp = NULL;
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
	char* tmp_report;
	char* tmp;
	gchar* response;
	gchar* begin_type;
	gchar* end_type;
	gboolean keep_going = TRUE;
	Namespace** ns;
	gchar* elem = NULL;
	int p;

	ns = getNamespace(report);
	if (ns) {
		for (p = 0; ns[p]; p++) {
			if (strcmp(ns[p]->NS, CALDAV) == 0) {
				elem = g_strconcat(ns[p]->prefix, ":", element, NULL);
				break;
			}
		}
		freeNamespace(ns);
	}
	if (! elem)
		elem = g_strdup(element);
	begin_type = g_strdup_printf("BEGIN:%s", type);
	end_type = g_strdup_printf("END:%s", type);
	pos = start = object = response = NULL;
	tmp_report = g_strdup(report);
	while ((pos = strstr(tmp_report, elem)) != NULL && keep_going) {
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
		if (response) {
			tmp = g_strdup(response);
			g_free(response);
			response = g_strdup_printf("%s%s\r\n%s%s\r\n",
				tmp, begin_type, object, end_type);
			g_free(tmp);
		}
		else {
			if (wrap)
				response = g_strdup_printf("%s%s\r\n%s%s\r\n",
					VCAL_HEAD, begin_type, object, end_type);
			else
				response = g_strdup_printf("%s\r\n%s%s\r\n",
					begin_type, object, end_type);
		}
		if (recursive) {
			pos = strchr(pos, '>');
			g_free(tmp_report);
			tmp_report = g_strdup(&(*(pos + 1)));
		}
		else {
			keep_going = FALSE;
		}
		g_free(start);
		g_free(object);
	}
	g_free(tmp_report);
	g_free(begin_type);
	g_free(end_type);
	g_free(elem);
	if (wrap)
		if (response) {
			object = g_strdup(response);
			g_free(response);
			response = g_strdup_printf("%s%s", object, VCAL_FOOT);
			g_free(object);
		}
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
				gchar* tmp = g_strdup(response);
				g_free(response);
				response = g_strdup_printf("%s%s%s", tmp, temp, VCAL_FOOT);
				g_free(tmp);
				g_free(temp);
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

	tzset();
	current = localtime(time);
	datetime = g_strdup_printf("%d%.2d%.2dT%.2d%.2d%.2dZ",
		current->tm_year/* + 1900*/, current->tm_mon/* + 1*/, current->tm_mday,
		current->tm_hour, current->tm_min, current->tm_sec);
	return datetime;
}

/**
 * Convert a CalDAV DateTime variable to time_t
 * @param time a specific date and time
 * @return time_t for the date and time
 */
time_t get_time_t(const gchar* date) {
	struct tm current;
	gchar* time;
	gchar* pos;

	if (! date)
		return (time_t) -1;
	
	if (!(strlen(date) == 8 || strlen(date) == 15 || strlen(date) == 16))
		return (time_t) -1;
	
	tzset();
	if ((pos = strchr(date, 'T')) != NULL) {
		pos += 1;
		time = g_strndup(pos, 2);
		current.tm_hour = atoi(time);
		g_free(time);
		time = g_strndup(pos+2, 2);
		current.tm_min = atoi(time);
		g_free(time);
		time = g_strndup(pos+4, 2);
		current.tm_sec = atoi(time);
		g_free(time);
		if (strlen(date) == 15) {
			/* localtime must be converted to UTF */
		}
	}
	time = g_strndup(date, 4);
	current.tm_year = atoi(time);
	g_free(time);
	time = g_strndup(date+4, 2);
	current.tm_mon = atoi(time);
	g_free(time);
	time = g_strndup(date+6, 2);
	current.tm_mday = atoi(time);
	g_free(time);
	return mktime(&current);
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
	g_free(name);
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
		gchar*tmp = g_strdup(newobj);
		g_free(newobj);
		newobj = g_strdup_printf("%s\r\nUID:libcaldav-%s@tempuri.org\r\n%s",
					tmp, uid, pos);
		g_free(uid);
		g_free(tmp);
		g_free(object);
	}
	else
		g_free(uid);
	g_strchomp(newobj);
	return newobj;
}

/**
 * Fetch a URL from a XML element
 * @param text String
 * @return URL
 */
#define ELEM_HREF "href"
gchar* get_url(gchar* text) {
	return get_tag_ns(DAV, ELEM_HREF, text);
}

/**
 * Fetch any element from XML. Namespace aware.
 * @param namespace
 * @param tag
 * @param text String
 * @return element
 */
gchar* get_tag_ns(const gchar* namespace, const gchar* tag, gchar* text) {
	gchar *pos;
	gchar* res = NULL;
	gchar* the_tag = NULL;
	gchar* end_tag = NULL;
	Namespace** ns;
	int p;
	
	/*printf("%s\n", text);*/
	if (namespace) {
		ns = getNamespace(text);
		if (ns) {
			for (p = 0; ns[p]; p++) {
				if (strcmp(ns[p]->NS, namespace) == 0) {
					the_tag = g_strconcat("<", ns[p]->prefix, ":", tag, ">", NULL);
					end_tag = g_strconcat("</", ns[p]->prefix, ":", tag, ">", NULL);
					break;
				}
			}
			freeNamespace(ns);
		}
	}
	if (! the_tag) {
		the_tag = g_strdup_printf("<%s>", tag);
		end_tag = g_strdup_printf("</%s>", tag);
	}
	if ((pos = strstr(text, the_tag)) == NULL) {
		g_free(the_tag);
		return res;
	}
	pos = &(*(pos + strlen(the_tag)));
	res = g_strndup(pos, strlen(pos) - strlen(strstr(pos, end_tag)));
	g_free(the_tag);
	g_free(end_tag);
	return res;	
}

gchar* sanitize(gchar* s) {
	gchar* s1;
	gchar *start, *end;
	
	if (! s)
		return NULL;
	
	if ((start = strchr(s, '"')) != NULL) {
		start += 1;
		if ((end = strchr(start, '"')) != NULL) {
			s1 = g_strndup(start, end - start);
		}
		else {
			s1 = g_strdup(start);
		}
	}
	else
		s1 = g_strdup(s);
	return s1; 
}

/**
 * Fetch a list of elements from XML. Namespace aware.
 * @param text String
 * @return list of elements. List is NULL terminated.
 */
#define NAMESPACE "DAV:"
#define THE_TAG "response"
GSList* get_tag_list(gchar* text) {
	gchar* elem;
	gchar* raw_text;
	gchar* token;
	Pair* pair = NULL;
	GSList* list = NULL;
	gchar* the_tag = NULL;
	gchar* etag = NULL;
	gchar* href = NULL;
	Namespace** ns;
	int p;
	
	ns = getNamespace(text);
	if (ns) {
		for (p = 0; ns[p]; p++) {
			if (strcmp(ns[p]->NS, NAMESPACE) == 0) {
				the_tag = g_strconcat(ns[p]->prefix, ":", THE_TAG, NULL);
				etag = g_strconcat(ns[p]->prefix, ":getetag", NULL);
				href = g_strconcat(ns[p]->prefix, ":href", NULL);
				break;
			}
		}
		freeNamespace(ns);
	}
	if (! the_tag) {
		the_tag = g_strdup(THE_TAG);
		etag = g_strdup("getetag");
		href = g_strdup("href");
	}
	token = g_strconcat("<", the_tag, ">", NULL);
	raw_text = g_strdup(text);
	elem = strstr(raw_text, token);
	if (elem) {
		do {
			gchar* element = get_tag_ns(NULL, the_tag, elem);
			pair = g_new0(Pair, 1);
			pair->href = g_strdup(get_tag_ns(NULL, href, element));
			gchar* tmp = sanitize(get_tag_ns(NULL, etag, element));
			pair->etag = g_strdup(tmp);
			g_free(tmp);
			list = g_slist_prepend(list, pair);
			elem  = elem + strlen(token);
			g_free(element);
		} while ((elem = strstr(elem, token)) != NULL);
	}
	g_free(token);
	g_free(the_tag);
	g_free(href);
	g_free(etag);
	g_free(raw_text);
	return list;
}

/**
 * Fetch any element from XML
 * @param text String
 * @return element
 * @deprecated Defaults to search for CalDAV elements
 */
gchar* get_tag(const gchar* tag, gchar* text) {
	return get_tag_ns(CALDAV, tag, text);
}

/**
 * Fetch the displayname element from XML
 * @param text String
 * @return displayname
 */
#define ELEM_DISPLAYNAME "displayname"
gchar* get_displayname(gchar* text) {
	return get_tag_ns(DAV, ELEM_DISPLAYNAME, text);
}

/**
 * Fetch the etag element from XML
 * @param text String
 * @return etag
 */
#define ELEM_ETAG "getetag"
gchar* get_etag(gchar* text) {
	return get_tag_ns(DAV, ELEM_ETAG, text);
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
 * rebuild a raw URL with https if needed from the settings
 * @param settings caldav_settings
 * @param uri URI to use instead of base
 * @return URL
 */
gchar* rebuild_url(caldav_settings* settings, gchar* uri){
    gchar* url = NULL;
    gchar* mystr = NULL;
	if (settings->usehttps) {
		mystr = "https://";
	} else {
		mystr = "http://";
	}
	if (uri)
		url = g_strdup_printf("%s%s", mystr, uri);
	else
    	url = g_strdup_printf("%s%s", mystr,settings->url);

	return url;
}

/**
 * Prepare a curl connection
 * @param settings caldav_settings
 * @return CURL
 */
CURL* get_curl(caldav_settings* setting) {
	CURL* curl;
	gchar* userpwd = NULL;
	gchar* url = NULL;

	curl = curl_easy_init();
	if (curl) {
		if (setting->username) {
			if (setting->password)
				userpwd = g_strdup_printf("%s:%s",
					setting->username, setting->password);
			else
				userpwd = g_strdup_printf("%s",	setting->username);
			curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
			/* 
			 * Use most secure way to authenticate
			 * This is a convenience macro that sets all bits and thus
			 * makes libcurl pick any it finds suitable. libcurl will
			 * automatically select the one it finds most secure.
			 */
			curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
			g_free(userpwd);
		}
		if (setting->verify_ssl_certificate)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
		else {
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		}
		if (setting->custom_cacert)
			curl_easy_setopt(curl, CURLOPT_CAINFO, setting->custom_cacert);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, __CALDAV_USERAGENT);
		url = rebuild_url(setting, NULL);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		g_free(url);
	}
	return (curl) ? curl : NULL;
}

/**
 * @param text text to search in
 * @param type VCalendar element to find
 * @return TRUE if text contains more than one resource FALSE otherwise
 */
gboolean single_resource(const gchar* text, const char* type) {
	const gchar* elem = "calendar-data";
	gchar* xml = g_strdup(text);
	gchar* token = g_strconcat("BEGIN:", type, NULL);
	int i = 0;
	
	gchar* objects = parse_caldav_report(xml, elem, type);
	gchar** parts = g_strsplit(objects, token, 0);
	g_free(objects);
	g_free(token);
	gchar** head = parts;
	while (parts && *parts++) {
		if (i++ > 1)
			break;
	}
	g_strfreev(head);
	g_free(xml);
	
	return (i < 3);
}

/**
 * @param text iCal to search in
 * @param elem VCalendar element to find
 * @return value or NULL if not found
 */
gchar* get_element_value(const gchar* text, const char* elem) {
	gchar* value = NULL;
	
	gchar* pos = strstr(text, elem);
	if (pos) {
		if ((pos = strchr(pos, ':')) != NULL) {
			pos = pos + 1;
			gchar* end = strchr(pos, '\n');
			if (end) {
				value = g_strndup(pos, end - pos);
			} 
		}
	}
	return value;
}

/**
 * A static literal string containing the first part of the calendar query.
 * The actual UID to use for the query is added at runtime.
 */
static char* search_head =
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
"<C:calendar-query xmlns:D=\"DAV:\""
"                  xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
"  <D:prop>"
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
 * Search CalDAV store for a specific object's ETAG
 * @param chunk struct MemoryStruct containing response from server
 * @param settings caldav_settings
 * @param error caldav_error
 * @return ETAG from the object or NULL
 */
gchar* find_etag(struct MemoryStruct* chunk,
  				 caldav_settings* settings,
				 caldav_error* error) {
	CURL* curl;
	CURLcode res = 0;
	char error_buf[CURL_ERROR_SIZE];
	struct config_data data;
	struct MemoryStruct headers;
	struct curl_slist *http_header = NULL;
	gchar* search;
	gchar* uid;
	gchar* etag = NULL;
	
	if (! chunk)
		return NULL;
	
	headers.memory = NULL;
	headers.size = 0;

	curl = get_curl(settings);
	if (!curl) {
		error->code = -1;
		error->str = g_strdup("Could not initialize libcurl");
		g_free(settings->file);
		settings->file = NULL;
		return NULL;
	}

	http_header = curl_slist_append(http_header,
			"Content-Type: application/xml; charset=\"utf-8\"");
	http_header = curl_slist_append(http_header, "Depth: infinity");
	http_header = curl_slist_append(http_header, "Expect:");
	http_header = curl_slist_append(http_header, "Transfer-Encoding:");
	data.trace_ascii = settings->trace_ascii;
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
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
	gchar* file = g_strdup(settings->file);
	if ((uid = get_response_header("uid", file, FALSE)) == NULL) {
		g_free(file);
		error->code = 1;
		error->str = g_strdup("Error: Missing required UID for object");
		return NULL;
	}
	g_free(file);
	/*
	 * ICalendar server does not support collation
	 * <C:text-match collation=\"i;ascii-casemap\">%s</C:text-match>
	 * <C:text-match>%s</C:text-match>
	 */
	search = g_strdup_printf(
		"%s\r\n<C:text-match collation=\"i;ascii-casemap\">%s</C:text-match>\r\n%s",
		search_head, uid, search_tail);
	g_free(uid);
	/* enable uploading */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, search);
	curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(search));
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "REPORT");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1);
	curl_easy_setopt(curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
	res = curl_easy_perform(curl);
	g_free(search);
	curl_slist_free_all(http_header);
	http_header = NULL;
	if (res != 0) {
		error->code = -1;
		error->str = g_strdup_printf("%s", error_buf);
		g_free(settings->file);
		settings->file = NULL;
	}
	else {
		long code;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (! parse_response(CALDAV_REPORT, code, chunk->memory)) {
			error->code = code;
			error->str = g_strdup(chunk->memory);
		}
		else {
			/* test if result contains more than one resource */
			if (single_resource(chunk->memory, "VEVENT")) {
				gchar* url = NULL;
				url = get_url(chunk->memory);
				if (url) {
					etag = get_etag(chunk->memory);
				}
				else {
					error->code = code;
					if (chunk->memory)
						error->str = g_strdup(chunk->memory);
					else
						error->str = g_strdup("No object found");
				}
				g_free(url);
			}
			else {
				error->code = -1;
				error->str = g_strdup("Multiple objects found");
			}
		}
	}
	if (headers.memory)
		free(headers.memory);
	curl_easy_cleanup(curl);
	return etag;
}

gchar* remove_protocol(gchar* text) {
	gchar* protocol;
	gchar* url;
	
	if (! text)
		return NULL;

	if ((protocol = strstr(text, "://")) != NULL)
		url = g_strdup(protocol + 3);
	else
		url = g_strdup(text);
	
	return url;
}