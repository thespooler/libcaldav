AUTOMAKE_OPTIONS = gnu

if BUILD_UNITTEST
INCLUDES = \
	   @CURL_CFLAGS@ \
	   @GLIB_CFLAGS@ \
	   -I$(top_srcdir) \
	   -I$(top_srcdir)/src

bin_PROGRAMS = unittest

unittest_SOURCES = \
		libunit.c
				
unittest_LDFLAGS = \
		      -L$(top_builddir)/src

unittest_LDADD = \
		    @CURL_LIBS@ \
		    @GLIB_LIBS@ \
		    -lcaldav
endif
