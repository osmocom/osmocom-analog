AM_CPPFLAGS = -Wall -Wextra -Wmissing-prototypes -g $(all_includes)

bin_PROGRAMS = \
	cnetz_sim

cnetz_sim_SOURCES = \
	sim.c \
	sniffer.c \
	image.c \
	main.c

cnetz_sim_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libserial/libserial.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	-lm

