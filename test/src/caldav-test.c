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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

gchar* make_url(gchar* uid, gchar* pwd, gchar* url) {
	char* pos;
	char* protocol = NULL;
	char* uri = NULL;
	char* newurl = NULL;

	if (!uid)
		return g_strdup(url);
	if ((pos = strstr(url, "//")) != NULL) {
		uri = g_strdup(&(*(pos + 2)));
		protocol = g_strndup(url, pos + 2 - url);
	}
	else {
	    protocol = g_strdup("http://");
	}
	if (!pwd)
		newurl = g_strdup_printf("%s%s@%s", protocol, uid, uri);
	else
		newurl = g_strdup_printf("%s%s:%s@%s", protocol, uid, pwd, uri);
	g_free(uri);
	g_free(protocol);
	return newurl;
}

#define BUFFER 1000
gchar* read_stream(FILE* stream, gchar* mem) {
	int len, fd;
	char buf[BUFFER];
	struct stat sb;

	fd = fileno(stream);
	fstat(fd, &sb);
	while ((len = read(fd, &buf, sizeof(buf))) > 0) {
		mem = (char *) realloc(mem, sizeof(buf) + 1);
		strncat(mem, buf, sizeof(buf));
	}
	mem[sb.st_size] = '\0';
	fclose(stream);
	return mem;
}

time_t make_time_t(const char* time_elem) {
	struct tm datetime = {0,0,0,0,0,0,0,0,0,0,NULL};
	//struct tm* tmp;
	gchar** elem;
	time_t t;

	//t = time(NULL);
	//tmp = localtime(&t);
	elem = g_strsplit(time_elem, "/", 3);
	if (g_strv_length(elem) == 3) {
	    datetime.tm_year = atoi(elem[0]) - 1900;
	    datetime.tm_mon = atoi(elem[1]) - 1;
	    datetime.tm_mday = atoi(elem[2]);
	    t = mktime(&datetime);
	}
	g_strfreev(elem);
	return t;
}

/* [yyyy/mm/dd[ hh/mm[/ss[/z]]] */
time_t make_fbtime_t(const char* time_elem) {
	struct tm datetime = {0,0,0,0,0,0,0,0,0,0,NULL};
	gboolean utc = FALSE;
	gchar** elem;
	gchar** dt;
	time_t t;
	guint length;

	dt = g_strsplit(time_elem, " ", 2);
	length = g_strv_length(dt);
	if (length > 1) {
		elem = g_strsplit(dt[1], "/", 4);
		length = g_strv_length(elem);
		datetime.tm_hour = atoi(elem[0]);
		datetime.tm_min = atoi(elem[1]);
		if (length > 2)
			datetime.tm_sec = atoi(elem[2]);
		if (length > 3 && strcasecmp(elem[3], "z") == 0)
			utc = TRUE;
		g_strfreev(elem);
	}
	elem = g_strsplit(time_elem, "/", 3);
	datetime.tm_year = atoi(elem[0]) - 1900;
	datetime.tm_mon = atoi(elem[1]) - 1;
	datetime.tm_mday = atoi(elem[2]);
	g_strfreev(elem);
	if (utc) {
		t = mktime(&datetime);
		struct tm* utc_time = gmtime(&t);
		t = mktime(utc_time);
		//g_free(utc_time);
	}
	else
		t = mktime(&datetime);
	return t;
}

static const char* usage[] = 	{
"caldav-test is part of libcaldav for claws-mails "
"vcalendar plugin.\nCopyright (C) Michael Rasmussen, 2008.\n"
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 3 of the License, or\n"
"(at your option) any later version.\n"
"\nusage:\n\tcaldav-test [Options] URL\n"
"\n\tOptions:\n\t\t-a\taction [is-caldav|add|delete|modify|get|get-all|displayname|options|freebusy]\n"
"\t\t-c\tprovide custom cacert (path to cert)\n"
"\t\t-d\tdebug (show request/response)\n"
"\t\t-e\tend [yyyy/mm/dd]. For FREEBUSY [yyyy/mm/dd[ hh/mm[/ss[/z]]]\n"
"\t\t-f\tfile. Alternative is to use IO redirection (<)\n"
"\t\t-p\tpassword\n"
"\t\t-s\tstart [yyyy/mm/dd]. For FREEBUSY [yyyy/mm/dd[ hh/mm[/ss[/z]]]\n"
"\t\t-u\tusername\n"
"\t\t-v\tdisable certificate verification\n"
"\t\t-h|-?\tusage\n"
};

int main(int argc, char **argv) {
	int c;
	CALDAV_ACTION ACTION = UNKNOWN;
	gboolean debug = FALSE;
	gboolean verify_ssl_certificate = TRUE;
	FILE* stream = NULL;
	gchar* username = NULL;
	gchar* password = NULL;
	gchar* url = NULL;
	gchar* start = NULL;
	gchar* end = NULL;
	response* result;
	CALDAV_RESPONSE res = UNKNOWN;
	gchar* input = NULL;
	char** options = NULL;
	runtime_info* opt;
	gchar* custom_cacert = NULL;

	opt = caldav_get_runtime_info();
	while ((c = getopt(argc, argv, "a:c:de:f:hp:s:u:v?")) != -1) {
		switch (c) {
			case 'h':
			case '?':
				fprintf(stdout, "%s", usage[0]);
				return 0;
			case 'a':
				if (strcmp("add", optarg) == 0) {
					ACTION = ADD;
				}
				else if (strcmp("delete", optarg) == 0) {
					ACTION = DELETE;
				}
				else if (strcmp("modify", optarg) == 0) {
					ACTION = MODIFY;
				}
				else if (strcmp("get", optarg) == 0) {
					ACTION = GET;
				}
				else if (strcmp("get-all", optarg) == 0) {
					ACTION = GETALL;
				}
				else if (strcmp("displayname", optarg) == 0) {
					ACTION = GETCALNAME;
				}
				else if (strcmp("is-caldav", optarg) == 0) {
					ACTION = ISCALDAV;
				}
				else if (strcmp("options", optarg) == 0) {
					ACTION = OPTIONS;
				}
				else if (strcmp("freebusy", optarg) == 0) {
					ACTION = FREEBUSY;
				}
				else {
					fprintf(stderr, "Unknown action: %s\n", optarg);
					fprintf(stderr, "%s", usage[0]);
					return 1;
				}
				break;
			case 'c':
				custom_cacert = g_strdup(optarg);
				break;
			case 'd':
				debug = TRUE;
				break;
			case 'e':
				end = optarg;
				break;
			case 'f':
				stream = fopen(optarg, "r");
				if (!stream) {
					perror("File");
					caldav_free_runtime_info(&opt);
					return 1;
				}
			case 'p':
				password = optarg;
				break;
			case 's':
				start = optarg;
				break;
			case 'u':
				username = optarg;
				break;
			case 'v':
				verify_ssl_certificate = FALSE;
				break;
			default:
				return 1;
		}
	}
	if (optind < argc - 1) {
		fprintf(stderr, "Error: Only enter one URL\n");
		fprintf(stderr, "%s", usage[0]);
		caldav_free_runtime_info(&opt);
		return 1;
	}
	if (optind == argc) {
		fprintf(stderr, "Error: Missing URL\n");
		fprintf(stderr, "%s", usage[0]);
		caldav_free_runtime_info(&opt);
		return 1;
	}
	if (ACTION == UNKNOWN) {
		fprintf(stderr, "Error: Missing action\n");
		fprintf(stderr, "%s", usage[0]);
		caldav_free_runtime_info(&opt);
		return 1;
	}
	if (ACTION != GETALL && ACTION != GET && ACTION != GETCALNAME &&
			ACTION != ISCALDAV && ACTION != OPTIONS && ACTION != FREEBUSY) {
		struct stat sb;
		if (fstat(fileno(stdin), &sb) == -1) {
			if (!stream) {
				perror("stat");
				caldav_free_runtime_info(&opt);
				return 1;
			}
		}
		else {
			if (stream && sb.st_size > 0) {
				fprintf(stderr, "Error: Option -f is in use. Cannot redirect stdin\n");
				caldav_free_runtime_info(&opt);
				return 1;
			}
			else 
				stream = (stream) ? stream : stdin;
		}
		input = read_stream(stream, input);
		if (!input) {
			fprintf(stderr, "Error: Could not read from file\n");
			caldav_free_runtime_info(&opt);
			return 1;
		}
	}
	if (ACTION == GET || ACTION == FREEBUSY) {
		if (start == NULL || end == NULL) {
			fprintf(stderr, "Error: Option '-a get' or '-a freebusy' requires option e and s\n");
			fprintf(stderr, "%s", usage[0]);
			caldav_free_runtime_info(&opt);
			return 1;
		}
	}
	url = make_url(username, password, argv[optind]);
	if (debug) {
		opt->options->debug = 1;
		opt->options->trace_ascii = 1;
	}
	opt->options->verify_ssl_certificate = verify_ssl_certificate;
	opt->options->custom_cacert = g_strdup(custom_cacert);
	g_free(custom_cacert);
	result = caldav_get_response();
	switch (ACTION) {
		case GETALL: res = caldav_getall_object(result, url, opt); break;
		case GET: res = caldav_get_object(
			result, make_time_t(start), make_time_t(end), url, opt); break;
		case FREEBUSY: res = caldav_get_freebusy(
			result, make_fbtime_t(start), make_fbtime_t(end), url, opt); break;
		case ADD: res = caldav_add_object(input, url, opt); break;
		case DELETE: res = caldav_delete_object(input, url, opt); break;
		case MODIFY: res = caldav_modify_object(input, url, opt); break;
		case GETCALNAME: res = caldav_get_displayname(result, url, opt); break;
		case ISCALDAV:
					res = caldav_enabled_resource(url, opt);
					if (res)
						res = OK;
					else {
						if (opt->error->code > 0) {
							switch (opt->error->code) {
								case 401:
								case 403: res = FORBIDDEN; break;
								case 409: res = CONFLICT; break;
								case 423: res = LOCKED; break;
								case 501: res = NOTIMPLEMENTED; break;
								default: res = CONFLICT; break;
							}
						}
						else
							res = CONFLICT;
					}
					break;
		case OPTIONS:
					options = caldav_get_server_options(url, opt);
					if (options)
						res = OK;
					else {
						if (opt->error->code > 0) {
							switch (opt->error->code) {
								case 401:
								case 403: res = FORBIDDEN; break;
								case 409: res = CONFLICT; break;
								case 423: res = LOCKED; break;
								case 501: res = NOTIMPLEMENTED; break;
								default: res = CONFLICT; break;
							}
						}
						else
							res = CONFLICT;
					}
					break;
		default: break;
	}
	g_free(url);
	g_free(input);
	if (res != OK) {
		fprintf(stderr, "Error\nCode: %ld\n%s\n", opt->error->code, opt->error->str);
		caldav_free_runtime_info(&opt);
		caldav_free_response(&result);
		return 1;
	}
	if (result->msg && ACTION != OPTIONS) {
		fprintf(stdout, "%s", result->msg);
		gchar* endline = strrchr(result->msg, '\n');
		if (endline) {
			if (strlen(endline) != 1)
				fprintf(stdout, "\n");
		}
		else
			fprintf(stdout, "\n");
	}
	else if (ACTION == OPTIONS) {
		char** tmp = options;
		while (*options) {
			fprintf(stdout, "%s\n", *options++);
		}
		g_strfreev(tmp);
	}
	else if (ACTION == GET || ACTION == GETALL || ACTION == GETCALNAME || ACTION == FREEBUSY) {
		fprintf(stdout, "empty collection\n");
	}
	fprintf(stdout, "OK\n");
	caldav_free_response(&result);
	caldav_free_runtime_info(&opt);
	return 0;
}
