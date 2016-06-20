AM_CPPFLAGS = -Wall -g $(all_includes)

noinst_PROGRAMS = \
	test_compandor \
	test_emphasis

test_compandor_SOURCES = test_compandor.c

test_compandor_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

test_emphasis_SOURCES = test_emphasis.c

test_emphasis_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

