AM_CPPFLAGS = -Wall -g $(all_includes)

noinst_PROGRAMS = \
	test_compandor \
	test_emphasis \
	test_dms

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

test_dms_SOURCES = \
	$(top_builddir)/src/nmt/dms.c \
	test_dms.c

test_dms_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

