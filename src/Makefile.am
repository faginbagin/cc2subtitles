## Process this file with automake to produce Makefile.in

bin_PROGRAMS = cc2subtitles decodempeg

cc2subtitles_SOURCES = cc2subtitles.cpp mpg2util.cpp PES_packet.cpp PS_pack.cpp vbi.cpp
cc2subtitles_SOURCES += mpg2util.h PES_packet.h PS_pack.h stream_id.h vbi.h
cc2subtitles_CPPFLAGS = -D_FILE_OFFSET_BITS=64

decodempeg_SOURCES = decodempeg.cpp mpg2util.cpp PES_packet.cpp PS_pack.cpp
decodempeg_SOURCES += mpg2util.h PES_packet.h PS_pack.h stream_id.h
decodempeg_CPPFLAGS = -D_FILE_OFFSET_BITS=64

bin_SCRIPTS = myburn tweakdvdxml.py
CLEANFILES = myburn tweakdvdxml.py
EXTRA_DIST = myburn.in tweakdvdxml.py.in

do_subst = sed \
	-e 's,[@]MYTHARCHIVEDIR[@],$(MYTHARCHIVEDIR),g' \
	-e 's,[@]MYTHARCHIVETMP[@],$(MYTHARCHIVETMP),g' \
	-e 's,[@]DEVDVD[@],$(DEVDVD),g' \
	-e 's,[@]USEFIFOS[@],$(USEFIFOS),g' \
	-e 's,[@]CREATEISO[@],$(CREATEISO),g' \
	-e 's,[@]libdir[@],$(libdir),g'

myburn: myburn.in
	$(do_subst) < $(srcdir)/myburn.in > myburn
	chmod +x myburn

tweakdvdxml.py:
	cat $(srcdir)/tweakdvdxml.py.in > tweakdvdxml.py
	chmod +x tweakdvdxml.py
