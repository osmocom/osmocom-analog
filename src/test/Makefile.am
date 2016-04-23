AM_CPPFLAGS = -Wall -g $(all_includes)

noinst_PROGRAMS = \
	test_compander \
	test_emphasis

test_compander_SOURCES = test_compander.c

test_compander_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

test_emphasis_SOURCES = test_emphasis.c

test_emphasis_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

