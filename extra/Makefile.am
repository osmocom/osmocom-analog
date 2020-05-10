AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	cnetz_memory_card_generator

cnetz_memory_card_generator_SOURCES = \
	main.c
cnetz_memory_card_generator_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/liboptions/liboptions.a

