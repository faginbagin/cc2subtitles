#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.63)
AC_INIT([cc2subtitles], [0.7], [mythtv@hbuus.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CONFIG_SRCDIR([src/vbi.h])
AC_CONFIG_HEADERS([config.h])

#
# Custom options
#
MYTHARCHIVEDIR=/usr/share/mythtv/mytharchive
AC_ARG_WITH([mytharchive-dir],
    [AS_HELP_STRING([--with-mytharchive-dir=ARG],
        [mytharchive install directory @<:@default=/usr/share/mythtv/mytharchive@:>@])
    ],
    [MYTHARCHIVEDIR=$withval]
)
AC_MSG_CHECKING([$MYTHARCHIVEDIR])
if test -f "$MYTHARCHIVEDIR/scripts/mythburn.py"; then
    AC_MSG_RESULT(ok)
else
    AC_MSG_WARN([mytharchive is not installed in $MYTHARCHIVEDIR])
fi
AC_SUBST([MYTHARCHIVEDIR])

MYTHARCHIVETMP="UNDEFINED"
AC_ARG_WITH([mytharchive-tmp],
    [AS_HELP_STRING([--with-mytharchive-tmp=ARG],
        [mytharchive temp directory @<:@default=UNDEFINED@:>@])
    ],
    [MYTHARCHIVETMP=$withval]
)
AC_MSG_CHECKING([$MYTHARCHIVETMP])
if test -d "$MYTHARCHIVETMP"; then
    AC_MSG_RESULT(ok)
else
    AC_MSG_WARN([mytharchive temp directory, \"$MYTHARCHIVETMP\", does not exist])
fi
AC_SUBST([MYTHARCHIVETMP])

DEVDVD="/dev/dvd"
AC_ARG_WITH([dev-dvd],
    [AS_HELP_STRING([--with-dev-dvd=ARG],
        [dvd device @<:@default=/dev/dvd@:>@])
    ],
    [DEVDVD=$withval]
)
AC_MSG_CHECKING([$DEVDVD])
if test -b "$DEVDVD"; then
    AC_MSG_RESULT(ok)
else
    AC_MSG_ERROR([dvd device, \"$DEVDVD\", does not exist or is not a block device])
fi
AC_SUBST([DEVDVD])

USEFIFOS=true
AC_ARG_ENABLE([use-fifos],
    [AS_HELP_STRING([--enable-use-fifos],
        [use fifos instead of temporary files @<:@default=yes@:>@])
    ]
)
if test "$enable_use_fifos" = "no"
then
    USEFIFOS=false
fi
AC_SUBST([USEFIFOS])

CREATEISO=false
AC_ARG_ENABLE([create-iso],
    [AS_HELP_STRING([--enable-create-iso],
        [create iso file with mkisofs @<:@default=no@:>@])
    ]
)
if test "$enable_create_iso" = "yes"
then
    CREATEISO=true
fi
AC_SUBST([CREATEISO])

MYTHTV_SRC=/usr/src/mythtv
SUBTITLE_PROG=mythsubtitles
AC_ARG_WITH([mythtv-src],
    [AS_HELP_STRING([--with-mythtv-src=ARG],
        [mythtv source directory @<:@default=/usr/src/mythtv@:>@])
    ],
    [MYTHTV_SRC=$withval]
)
AC_MSG_CHECKING([$MYTHTV_SRC])
if test -f "$MYTHTV_SRC/programs/mythfrontend/mythfrontend"; then
    AC_MSG_RESULT(ok)
else
    AC_MSG_WARN([mythtv source is not installed and built in $MYTHTV_SRC])
    SUBTITLE_PROG=cc2subtitles
fi
AC_SUBST([MYTHTV_SRC])
AC_SUBST([SUBTITLE_PROG])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([zvbi], [vbi_decoder_new])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])
AC_CHECK_HEADERS([libzvbi.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_HEADER_STDBOOL
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FSEEKO
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([alarm memset strdup strerror])

AC_CONFIG_FILES([Makefile
                 mythsubtitles/Makefile
                 src/Makefile])
AC_OUTPUT
