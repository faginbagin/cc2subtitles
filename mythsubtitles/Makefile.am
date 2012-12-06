## Process this file with automake to produce Makefile.in

noinst_SCRIPTS = mythsubtitles.pro mythsubtitles.mak
bin_SCRIPTS = mythsubtitles
DISTCLEANFILES = $(noinst_SCRIPTS) $(srcdir)/mythsubtitles.pro
EXTRA_DIST = mythsubtitles.pro.in
EXTRA_DIST += main.cpp
EXTRA_DIST += Decoder.cpp Decoder.h
EXTRA_DIST += IvtvDecoder.cpp IvtvDecoder.h
EXTRA_DIST += MpegDecoder.cpp MpegDecoder.h
EXTRA_DIST += NuvDecoder.cpp NuvDecoder.h
EXTRA_DIST += vbi.cpp vbi.h

do_subst = sed \
	-e 's,[@]MYTHTV_SRC[@],$(MYTHTV_SRC),g'

mythsubtitles.pro: mythsubtitles.pro.in
	$(do_subst) < $(srcdir)/mythsubtitles.pro.in > mythsubtitles.pro
	set -x; if [ $(srcdir) != . ]; then \
         chmod 755 $(srcdir); \
         mv -f mythsubtitles.pro $(srcdir) || rm mythsubtitles.pro; \
    fi

mythsubtitles.mak: mythsubtitles.pro
	qmake -o mythsubtitles.mak $(srcdir)/mythsubtitles.pro

mythsubtitles: mythsubtitles.mak force
	$(MAKE) -f mythsubtitles.mak

clean-local: mythsubtitles.mak
	$(MAKE) -f mythsubtitles.mak clean

force:

