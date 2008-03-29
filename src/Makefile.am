AUTOMAKE_OPTIONS = gnu

INCLUDES = @CURL_CFLAGS@ @GLIB_CFLAGS@ \
		   -I$(top_srcdir) -I$(top_builddir)

#noinst_LTLIBRARIES = libcaldav.la

# Uncomment and comment out above to use outside vcalendar plugin
lib_LTLIBRARIES = libcaldav.la

libcaldav_la_LDFLAGS = -version-info @LIBVERSION@

libcaldav_la_SOURCES = \
		      caldav.h \
		      caldav.c \
		      add-caldav-object.c \
		      add-caldav-object.h \
		      delete-caldav-object.c \
		      delete-caldav-object.h \
		      modify-caldav-object.c \
		      modify-caldav-object.h \
		      get-caldav-report.c \
		      get-caldav-report.h \
			  get-display-name.c \
			  get-display-name.h \
		      caldav-utils.c \
		      caldav-utils.h \
			  md5.c \
			  md5.h

libcaldav_includedir=$(includedir)/libcaldav-@VERSION@
libcaldav_include_HEADERS = caldav.h

noinst_HEADERS = \
		 add-caldav-object.h \
		 delete-caldav-object.h \
		 modify-caldav-object.h \
		 get-caldav-report.h \
		 caldav-utils.h

libcaldav_la_LIBADD = @CURL_LIBS@ @GLIB_LIBS@

