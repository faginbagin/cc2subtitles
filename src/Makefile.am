## Process this file with automake to produce Makefile.in

bin_PROGRAMS = cc2subtitles decodempeg stripivtv

noinst_LIBRARIES = libcc2sub.a

AM_CPPFLAGS = -D_FILE_OFFSET_BITS=64
LDADD = libcc2sub.a

libcc2sub_a_SOURCES = mpg2util.cpp PES_packet.cpp PS_pack.cpp vbi.cpp
libcc2sub_a_SOURCES += mpg2util.h PES_packet.h PS_pack.h stream_id.h vbi.h

cc2subtitles_SOURCES = cc2subtitles.cpp

decodempeg_SOURCES = decodempeg.cpp

stripivtv_SOURCES = stripivtv.cpp

bin_SCRIPTS = myburn tweakdvdxml.py mytranscode
CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST = myburn.in tweakdvdxml.py.in mytranscode.in

do_subst = sed \
	-e 's,[@]MYTHARCHIVEDIR[@],$(MYTHARCHIVEDIR),g' \
	-e 's,[@]MYTHARCHIVETMP[@],$(MYTHARCHIVETMP),g' \
	-e 's,[@]DEVDVD[@],$(DEVDVD),g' \
	-e 's,[@]USEFIFOS[@],$(USEFIFOS),g' \
	-e 's,[@]CREATEISO[@],$(CREATEISO),g' \
	-e 's,[@]SUBTITLE_PROG[@],$(SUBTITLE_PROG),g' \
	-e 's,[@]libdir[@],$(libdir),g'

myburn: myburn.in
	$(do_subst) < $(srcdir)/myburn.in > myburn
	chmod +x myburn

tweakdvdxml.py: tweakdvdxml.py.in
	cat $(srcdir)/tweakdvdxml.py.in > tweakdvdxml.py
	chmod +x tweakdvdxml.py

mytranscode: mytranscode.in
	cat $(srcdir)/mytranscode.in > mytranscode
	chmod +x mytranscode

