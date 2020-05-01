AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	cnetz_sim

cnetz_sim_SOURCES = \
	sim.c \
	sniffer.c \
	image.c \
	main.c

cnetz_sim_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libserial/libserial.a \
	-lm

