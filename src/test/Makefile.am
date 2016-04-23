AM_CPPFLAGS = -Wall -g $(all_includes)

noinst_PROGRAMS = \
	test_compander

test_compander_SOURCES = test_compander.c

test_compander_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

