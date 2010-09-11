/* vim: set textwidth=80 tabstop=4: */

/* Copyright (c) 2010 Michael Rasmussen (mir@datanom.net)
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

#include "response-parser.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int CALDAV_DELETE_OK[] = {200, 202, 204};
static int CALDAV_DELETE_OK_SIZE = sizeof(CALDAV_DELETE_OK)/sizeof(int);

static int CALDAV_GET_OK[] = {200};
static int CALDAV_GET_OK_SIZE = sizeof(CALDAV_GET_OK)/sizeof(int);

static int CALDAV_LOCK_OK[] = {200};
static int CALDAV_LOCK_OK_SIZE = sizeof(CALDAV_LOCK_OK)/sizeof(int);

static int CALDAV_OPTIONS_OK[] = {200};
static int CALDAV_OPTIONS_OK_SIZE = sizeof(CALDAV_OPTIONS_OK)/sizeof(int);

static int CALDAV_PROPFIND_OK[] = {207};
static int CALDAV_PROPFIND_OK_SIZE = sizeof(CALDAV_PROPFIND_OK)/sizeof(int);

static int CALDAV_PUT_OK[] = {200, 201, 204};
static int CALDAV_PUT_OK_SIZE = sizeof(CALDAV_PUT_OK)/sizeof(int);

static int CALDAV_REPORT_OK[] = {207};
static int CALDAV_REPORT_OK_SIZE = sizeof(CALDAV_REPORT_OK)/sizeof(int);

static int CALDAV_UNLOCK_OK[] = {204};
static int CALDAV_UNLOCK_OK_SIZE = sizeof(CALDAV_UNLOCK_OK)/sizeof(int);

static gboolean
caldav_delete(int response_code, gchar* response_body) {
	gboolean result = FALSE;
	int i;
	
	for (i = 0; i < CALDAV_DELETE_OK_SIZE; i++) {
		if (response_code == CALDAV_DELETE_OK[i]) {
			result = TRUE;
			break;
		}
	}
	
	return result;
}

static gboolean
caldav_options(int response_code, gchar* response_body) {
	gboolean result = FALSE;
	int i;
	
	for (i = 0; i < CALDAV_OPTIONS_OK_SIZE; i++) {
		if (response_code == CALDAV_OPTIONS_OK[i]) {
			result = TRUE;
			break;
		}
	}
	
	return result;
}

static gboolean
caldav_propfind(int response_code, gchar* response_body) {
	gboolean result = FALSE;
	int i;
	
	for (i = 0; i < CALDAV_PROPFIND_OK_SIZE; i++) {
		if (response_code == CALDAV_PROPFIND_OK[i]) {
			result = TRUE;
			break;
		}
	}
	
	return result;
}

static gboolean
caldav_put(int response_code, gchar* response_body) {
	gboolean result = FALSE;
	int i;
	
	for (i = 0; i < CALDAV_PUT_OK_SIZE; i++) {
		if (response_code == CALDAV_PUT_OK[i]) {
			result = TRUE;
			break;
		}
	}
	
	return result;
}

static gboolean
caldav_get(int response_code, gchar* response_body) {
	gboolean result = FALSE;
	int i;
	
	for (i = 0; i < CALDAV_GET_OK_SIZE; i++) {
		if (response_code == CALDAV_GET_OK[i]) {
			result = TRUE;
			break;
		}
	}
	
	return result;
}

static gboolean
caldav_lock(int response_code, gchar* response_body) {
	gboolean result = FALSE;
	int i;
	
	for (i = 0; i < CALDAV_LOCK_OK_SIZE; i++) {
		if (response_code == CALDAV_LOCK_OK[i]) {
			result = TRUE;
			break;
		}
	}
	
	return result;
}

static gboolean
caldav_unlock(int response_code, gchar* response_body) {
	gboolean result = FALSE;
	int i;
	
	for (i = 0; i < CALDAV_UNLOCK_OK_SIZE; i++) {
		if (response_code == CALDAV_UNLOCK_OK[i]) {
			result = TRUE;
			break;
		}
	}
	
	return result;
}

static gboolean
caldav_report(int response_code, gchar* response_body) {
	gboolean result = FALSE;
	int i;
	
	for (i = 0; i < CALDAV_REPORT_OK_SIZE; i++) {
		if (response_code == CALDAV_REPORT_OK[i]) {
			result = TRUE;
			break;
		}
	}
	
	return result;
}

gboolean
parse_response(CALDAV_METHOD method, int response_code, gchar* response_body) {
	gboolean result = FALSE;
	
	switch (method) {
		case CALDAV_DELETE:
			result = caldav_delete(response_code, response_body);
			break;
		case CALDAV_GET:
			result = caldav_get(response_code, response_body);
			break;
		case CALDAV_LOCK:
			result = caldav_lock(response_code, response_body);
			break;
		case CALDAV_OPTIONS:
			result = caldav_options(response_code, response_body);
			break;
		case CALDAV_PROPFIND:
			result = caldav_propfind(response_code, response_body);
			break;
		case CALDAV_PUT:
			result = caldav_put(response_code, response_body);
			break;
		case CALDAV_REPORT:
			result = caldav_report(response_code, response_body);
			break;
		case CALDAV_UNLOCK:
			result = caldav_unlock(response_code, response_body);
			break;
	}
	return result;
}
