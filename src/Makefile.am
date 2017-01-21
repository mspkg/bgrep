AM_CFLAGS = -I$(top_builddir)/lib -I$(top_srcdir)/lib

bin_PROGRAMS = bgrep
bgrep_SOURCES = bgrep.c parse_integer.c print_output.c byte_pattern.c
bgrep_LDADD = $(top_srcdir)/lib/libgnu.a $(LIBINTL)
