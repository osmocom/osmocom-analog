AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -Wall -g $(all_includes)

noinst_PROGRAMS = \
	test_compandor \
	test_emphasis \
	test_dms \
	test_sms

test_compandor_SOURCES = test_compandor.c

test_compandor_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

test_emphasis_SOURCES = test_emphasis.c dummy.c

test_emphasis_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	-lm

test_dms_SOURCES = \
	$(top_builddir)/src/nmt/dms.c \
	test_dms.c \
	dummy.c

test_dms_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	-lm

test_sms_SOURCES = \
	$(top_builddir)/src/nmt/sms.c \
	dummy.c \
	test_sms.c

test_sms_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	-lm

