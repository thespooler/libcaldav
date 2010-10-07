/**
 * md5.h - MD5 Message-Digest Algorithm
 *	Copyright (C) 1995, 1996, 1998, 1999 Free Software Foundation, Inc.
 *
 * according to the definition of MD5 in RFC 1321 from April 1992.
 * NOTE: This is *not* the same file as the one from glibc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MD5_HDR_
#define _MD5_HDR_

#include <glib.h>
G_BEGIN_DECLS

/**
 * @typedef u32
 * Kept this typedef for compatibility reasons
 */
#ifndef HAVE_U32_TYPEDEF
	#undef u32
	typedef guint32 u32;
	#define HAVE_U32_TYPEDEF
#endif

typedef struct {  /* Hmm, should be private */
    u32 A,B,C,D;
    u32  nblocks;
    unsigned char buf[64];
    int  count;
    int  finalized;
} MD5_CONTEXT;

void caldav_md5_hex_digest(char *hexdigest, const unsigned char *s);

void caldav_md5_hex_hmac(char *hexdigest,
                  const unsigned char* text, int text_len,
                  const unsigned char* key, int key_len);

G_END_DECLS

#endif /* _MD5_HDR_ */

