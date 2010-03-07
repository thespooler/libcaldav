AUTOMAKE_OPTIONS = gnu

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src test

if BUILD_DOC
include aminclude.am

MOSTLYCLEANFILES = \
				   $(DX_CLEANFILES) \
				   doc

doxydir = @DX_DOCDIR@
dist_man3_MANS = $(doxydir)/man/man3/caldav.h.3
$(dist_man3_MANS): doxygen-doc

libcaldav_apidoc_filedir = $(docdir)
libcaldav_apidoc_file_DATA = \
						  doc/libcaldav.pdf
endif

libcaldav_configdir = $(libdir)/pkgconfig
libcaldav_config_DATA = libcaldav.pc

EXTRA_DIST = \
			 README \
			 COPYING \
			 AUTHORS \
			 ChangeLog \
			 INSTALL \
			 NEWS \
			 TODO \
			 rfc4791.pdf \
			 version.sh \
			 $(dx_config) \
			 doxygen-api.cfg \
			 libcaldav.pc \
			 autogen.sh
