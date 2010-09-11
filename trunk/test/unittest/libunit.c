/* vim: set textwidth=80 tabstop=4: */

/*
//      libunit.c
//      
//      Copyright 2010 Michael Rasmussen <mir@datanom.net>
//      
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 3 of the License, or
//      (at your option) any later version.
//      
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//      
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "caldav.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <string.h>
#include <sys/time.h>
#include <string.h>

static const char* usage[] = 	{
"unittest is part of libcaldav for claws-mails "
"vcalendar plugin.\nCopyright (C) Michael Rasmussen, 2008.\n"
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 3 of the License, or\n"
"(at your option) any later version.\n"
"\nusage:\n\tunittest [Options]\n"
"\n\tOptions:\n"
"\t\t-h|-?\tusage\n"
"\t\t-a\tURL\n"
"\t\t-d\tEnable debug\n"
"\t\t-l\tDebug library\n"
"\t\t-p\tpassword\n"
"\t\t-u\tusername\n"
};

/*
 * Options can be given in one of three ways:
 * 1) Entered as options on the command line
 * 2) Stored in environment variables:
 *    - LIBCALDAV_UID -> username to use when connection to server
 *    - LIBCALDAV_PWD -> password, if any, to use when connection to server
 *    - LIBCALDAV_URL -> complete URL to server. prefixed with either
 *                       http:// or https://
 * 3) Stored in a file found in the same directory as the program:
 *    [server]
 * 	  password=Big secret
 *    username=username
 *    url=http[s]://bar.tld/foo
 */

#define CONFFILE "caldav-unit-settings.conf"

typedef struct {
	gchar* uid;
	gchar* pwd;
	gchar* url;
} settings;

typedef enum {
	PWD,
	UID,
	URL
} CONFIG;
	
static settings* SETTING = NULL;
gboolean DEBUG = FALSE;
gboolean DEBUG_LIB = FALSE;

void settings_free(settings** setting) {
	settings* s;
	
	if (! *setting)
		return;
	s = *setting;
	g_free(s->url);
	g_free(s->uid);
	g_free(s->pwd);
	g_free(s);
	*setting = s = NULL;
}

settings* parse_cmdline(int argc, char** argv) {
	char c;
	settings* setting;

	setting = g_new0(settings, 1);	
	while ((c = getopt(argc, argv, "ha:dlp:u:?")) != -1) {
		switch (c) {
			case 'h':
			case '?':
				fprintf(stdout, "%s", usage[0]);
				return 0;
			case 'a':
				setting->url = g_strdup(optarg);
				break;
			case 'd':
				DEBUG = TRUE;
				break;
			case 'l':
				DEBUG_LIB = TRUE;
				break;
			case 'p':
				setting->pwd = g_strdup(optarg);
				break;
			case 'u':
				setting->uid = g_strdup(optarg);
				break;
		}
	}
	if (argv[optind]) {
		fprintf(stderr, "No arguments after options expected\n");
		fprintf(stderr, "%s", usage[0]);
		settings_free(&setting);
		return NULL;
	}
	
	return setting;
}

void read_file(const gchar* file) {
	GKeyFile* keyfile;
	
	if (! SETTING) {
		SETTING = g_new0(settings, 1);
		keyfile = g_key_file_new();
		if (g_key_file_load_from_file(keyfile,
									  file,
									  G_KEY_FILE_KEEP_COMMENTS,
									  NULL)) {
			SETTING->pwd = g_key_file_get_string(keyfile,
												 "server",
												 "password",
												 NULL);
			SETTING->uid = g_key_file_get_string(keyfile,
												 "server",
												 "username",
												 NULL);
			SETTING->url = g_key_file_get_string(keyfile,
												 "server",
												 "url",
												 NULL);
		}
		g_key_file_free(keyfile);
	}
}

const gchar* get_setting_from_file(CONFIG config) {
	gchar* cwd;
	gchar* path;
	gchar* pos;
	gchar* setting = NULL;
	int step;
	
	gchar* filename = g_get_prgname();
	if (filename[0] == '/') {
		/* absolute path */
		cwd = g_path_get_dirname(filename);
	}
	else if ((pos = strchr(filename, '/')) != NULL) {
		/* relative path */
		if (filename[0] == '.') {
			char* tmp = pos;
			while (tmp) {
				if (strncmp(tmp, "..", 2) == 0)
					step = 2;
				else
					step = 1;
				tmp = strchr(pos + step, '/');
				if (tmp)
					pos = tmp;
			}
			if (pos == NULL) {
				pos = strchr(filename, '/');
			}
		}
		path = g_get_current_dir();
		cwd = g_build_filename(path, pos + 1, NULL);
		g_free(path);
		if (g_file_test(cwd, G_FILE_TEST_IS_REGULAR)) {
			pos = strrchr(cwd, '/');
			if (pos) {
				path = g_strndup(cwd, pos - cwd);
				g_free(cwd);
				cwd = g_strdup(path);
				g_free(path);
			}
		}
	}
	else {
		/* find cwd in PATH */
		cwd = g_find_program_in_path(filename);
	}
	path = g_build_filename(cwd, CONFFILE, NULL);
	g_free(cwd);
	if (g_file_test(path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		read_file(path);
		switch (config) {
			case UID: setting = SETTING->uid; break;
			case PWD: setting = SETTING->pwd; break;
			case URL: setting = SETTING->url; break;
		}
	}
	g_free(path);
	return setting;
}

gboolean find_missing(settings** setting) {
	settings* s = *setting;
	const char* env;
	gboolean error = FALSE;

	if (! s)
		return TRUE;
		
	if (! s->uid) {
		if ((env = getenv("LIBCALDAV_UID")) == NULL) {
			env = get_setting_from_file(UID);
			if (env == NULL)
				error = TRUE;
		}
		s->uid = g_strdup(env);
	}
	if (! s->pwd) {
		if ((env = getenv("LIBCALDAV_PWD")) == NULL) {
			env = get_setting_from_file(PWD);
			if (env == NULL)
				error = TRUE;
		}
		s->pwd = g_strdup(env);
	}
	if (! s->url) {
		if ((env = getenv("LIBCALDAV_URL")) == NULL) {
			env = get_setting_from_file(URL);
			if (env == NULL)
				error = TRUE;
		}
		s->url = g_strdup(env);
	}
	settings_free(&SETTING);
	return error;
}

time_t make_time_t(const char* time_elem) {
	struct tm datetime = {0,0,0,0,0,0,0,0,0,0,NULL};
	struct tm* tmp;
	gchar** elem;
	time_t t;

	t = time(NULL);
	tmp = localtime(&t);
	elem = g_strsplit(time_elem, "/", 3);
	datetime.tm_year = atoi(elem[0]) - 1900;
	datetime.tm_mon = atoi(elem[1]) - 1;
	datetime.tm_mday = atoi(elem[2]);
	g_strfreev(elem);
	t = mktime(&datetime);
	return t;
}

gboolean compare_freebusy(const gchar* s1, const gchar* s2) {
	const char* TOKEN = "FREEBUSY:";
	gchar* pos1;
	gchar* pos2;
	gchar* id1;

	if (! s1 && ! s2)
		return TRUE;
	if (! s1 || ! s2)
		return FALSE;
	pos1 = strstr(s2, TOKEN);
	if (! pos1)
		return FALSE;
	pos1 += strlen(TOKEN);
	pos2 = strchr(pos1, '\n');
	if (! pos2)
		return FALSE;
	if (*(pos2 - 1) == '\r')
		pos2--;
	id1 = g_strndup(pos1, pos2 - pos1);
	int res = strcmp(s1, id1);
	g_free(id1);
	return (res == 0);
}

gboolean compare_object(const gchar* TOKEN, const gchar* s1, const gchar* s2) {
	gchar* pos1;
	gchar* pos2;
	gchar* id1;
	gchar* id2;
	
	if (! s1 && ! s2)
		return TRUE;
	if (! s1 || ! s2)
		return FALSE;
	pos1 = strstr(s1, TOKEN);
	if (! pos1)
		return FALSE;
	pos1 += strlen(TOKEN) + 1;
	pos2 = strchr(pos1, '\n');
	if (! pos2)
		return FALSE;
	if (*(pos2 - 1) == '\r')
		pos2--;
	id1 = g_strndup(pos1, pos2 - pos1);
	pos2 = strstr(s2, TOKEN);
	if (! pos2) {
		g_free(id1);
		return FALSE;
	}
	pos2 += strlen(TOKEN) + 1;
	pos1 = strchr(pos2, '\n');	
	if (! pos1) {
		g_free(id1);
		return FALSE;
	}
	if (*(pos1 - 1) == '\r')
		pos1--;
	id2 = g_strndup(pos2, pos1 - pos2);
	int res = strcmp(id1, id2);
	g_free(id1);
	g_free(id2);
	return (res == 0);
}

void run_tests(settings* s) {
	runtime_info* info = caldav_get_runtime_info();
	response* resp = caldav_get_response();
	gchar** parts;
	gchar* url;
	gchar* object;
	
	if (DEBUG_LIB) {
	    info->options->debug = 1;
	    info->options->trace_ascii = 1;
	}
	parts = g_strsplit(s->url, "//", 2);
	url = g_strconcat(
		parts[0], "//", s->uid, ":", s->pwd, "@", parts[1], NULL);
	g_strfreev(parts);
	fprintf(stdout, "Test caldav_enabled_resource:\t\t\t");
	if (caldav_enabled_resource(url, info)) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "caldav enabled server\n");
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	fprintf(stdout, "Test caldav_get_displayname:\t\t\t");
	if (caldav_get_displayname(resp, url, info) == OK) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "Display Name: %s\n", resp->msg);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(resp->msg);
	resp->msg = NULL;
	fprintf(stdout, "Test caldav_get_server_options:\t\t\t");
	if ((parts = caldav_get_server_options(url, info)) != NULL) {
		fprintf(stdout, "OK\n");
		if (DEBUG) {
			char** tmp = parts;
			while (*tmp) {
				fprintf(stdout, "--> %s\n", *tmp++);
			}
		}
		g_strfreev(parts);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_file_get_contents("../ics/add.ics", &object, NULL, NULL);
	fprintf(stdout, "Test caldav_add_object:\t\t\t\t");
	if (caldav_add_object(object, url, info) == OK) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "Added successfully\n");
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	fprintf(stdout, "Test if object was added:\t\t\t");
	if (caldav_get_object(resp, make_time_t("2010/07/12"),
		make_time_t("2010/07/13"), url, info) == OK) {
		if (compare_object("UID", object, resp->msg))
			fprintf(stdout, "OK\n");
		else
			fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%s\n", resp->msg);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(resp->msg);
	resp->msg = NULL;
	g_free(object);
	fprintf(stdout, "Test FREEBUSY search the same day:\t\t");
	if (caldav_get_freebusy(resp, make_time_t("2010/07/12"),
		make_time_t("2010/07/13"), url, info) == OK) {
			if (compare_freebusy("20100712T151500Z/20100712T162500Z", resp->msg))
				fprintf(stdout, "OK\n");
			else
				fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%s\n", resp->msg);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(resp->msg);
	resp->msg = NULL;
	g_file_get_contents("../ics/modify.ics", &object, NULL, NULL);
	fprintf(stdout, "Test caldav_modify_object:\t\t\t");
	if (caldav_modify_object(object, url, info) == OK) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "Modified successfully\n");
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	fprintf(stdout, "Test if object was modified:\t\t\t");
	if (caldav_get_object(resp, make_time_t("2010/07/13"),
		make_time_t("2010/07/14"), url, info) == OK) {
		if (compare_object("DTEND", object, resp->msg))
			fprintf(stdout, "OK\n");
		else
			fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%s\n", resp->msg);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(resp->msg);
	resp->msg = NULL;
	fprintf(stdout, "Test caldav_getall_object:\t\t\t");
	if (caldav_getall_object(resp, url, info) == OK) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "%s\n", resp->msg);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	fprintf(stdout, "Test if object exists:\t\t\t\t");
	if (compare_object("UID", object, resp->msg))
		fprintf(stdout, "FAIL\n");
	else
		fprintf(stdout, "OK\n");
	g_free(resp->msg);
	resp->msg = NULL;
	g_free(object);
	g_file_get_contents("../ics/delete.ics", &object, NULL, NULL);
	fprintf(stdout, "Test caldav_delete_object:\t\t\t");
	if (caldav_delete_object(object, url, info) == OK) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "Deleted successfully\n");
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	fprintf(stdout, "Test if object was deleted:\t\t\t");
	if (caldav_get_object(resp, make_time_t("2010/07/13"),
		make_time_t("2010/07/14"), url, info) == OK) {
		if (compare_object("UID", object, resp->msg))
			fprintf(stdout, "FAIL\n");
		else
			fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "%s\n", (resp->msg) ? resp->msg : "No object found");
		g_free(resp->msg);
		resp->msg = NULL;
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(object);
	fprintf(stdout, "\nTesting without using locks\n");
	info->options->use_locking = 0;
	g_file_get_contents("../ics/add.ics", &object, NULL, NULL);
	fprintf(stdout, "Test caldav_add_object:\t\t\t\t");
	if (caldav_add_object(object, url, info) == OK) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "Added successfully\n");
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	fprintf(stdout, "Test if object was added:\t\t\t");
	if (caldav_get_object(resp, make_time_t("2010/07/12"),
		make_time_t("2010/07/13"), url, info) == OK) {
		if (compare_object("UID", object, resp->msg))
			fprintf(stdout, "OK\n");
		else
			fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%s\n", resp->msg);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(resp->msg);
	resp->msg = NULL;
	g_free(object);
/*	fprintf(stdout, "Test FREEBUSY search the same day:\t\t");
	if (caldav_get_freebusy(resp, make_time_t("2008/04/15"),
		make_time_t("2008/04/16"), url, info) == OK) {
			if (compare_freebusy("20080415T151500Z/20080415T162500", resp->msg))
				fprintf(stdout, "OK\n");
			else
				fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%s\n", resp->msg);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(resp->msg);
	resp->msg = NULL;*/
	g_file_get_contents("../ics/modify.ics", &object, NULL, NULL);
	fprintf(stdout, "Test caldav_modify_object:\t\t\t");
	if (caldav_modify_object(object, url, info) == OK) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "Modified successfully\n");
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	fprintf(stdout, "Test if object was modified:\t\t\t");
	if (caldav_get_object(resp, make_time_t("2010/07/13"),
		make_time_t("2010/07/14"), url, info) == OK) {
		if (compare_object("DTEND", object, resp->msg))
			fprintf(stdout, "OK\n");
		else
			fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%s\n", resp->msg);
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(resp->msg);
	resp->msg = NULL;
	g_free(object);
	g_file_get_contents("../ics/delete.ics", &object, NULL, NULL);
	fprintf(stdout, "Test caldav_delete_object:\t\t\t");
	if (caldav_delete_object(object, url, info) == OK) {
		fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "Deleted successfully\n");
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	fprintf(stdout, "Test if object was deleted:\t\t\t");
	if (caldav_get_object(resp, make_time_t("2010/07/13"),
		make_time_t("2010/07/14"), url, info) == OK) {
		if (compare_object("UID", object, resp->msg))
			fprintf(stdout, "FAIL\n");
		else
			fprintf(stdout, "OK\n");
		if (DEBUG) fprintf(stdout, "%s\n", (resp->msg) ? resp->msg : "No object found");
		g_free(resp->msg);
		resp->msg = NULL;
	}
	else {
		fprintf(stdout, "FAIL\n");
		if (DEBUG) fprintf(stdout, "%ld: %s\n", info->error->code, info->error->str);
	}
	g_free(url);
	caldav_free_response(&resp);
	caldav_free_runtime_info(&info);
}

int main(int argc, char** argv) {
	settings* setting;
	
	g_set_prgname(argv[0]);
	setting = parse_cmdline(argc, argv);
	if (find_missing(&setting)) {
		fprintf(stderr, "missing required information\n");
		fprintf(stdout, "%s", usage[0]);
		settings_free(&setting);
		return 1;
	}
	run_tests(setting);
	settings_free(&setting);
	return 0;
}
