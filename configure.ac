#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([cc2subtitles], [0.3], [mythtv@hbuus.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CONFIG_SRCDIR([src/vbi.h])
AC_CONFIG_HEADER([config.h])

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
    AC_MSG_ERROR([mytharchive is not installed in $MYTHARCHIVEDIR])
fi
AC_SUBST([MYTHARCHIVEDIR])

MYTHARCHIVETMP=""
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
    AC_MSG_ERROR([mytharchive temp directory, \"$MYTHARCHIVETMP\", does not exist])
fi
AC_SUBST([MYTHARCHIVETMP])

RUNTWEAKDVDXML=false
AC_ARG_ENABLE([tweakdvxml],
    [AS_HELP_STRING([--enable-tweakdvdxml],
        [let tweakdvdxml.py post process dvdauthor.xml @<:@default=no@:>@])
    ]
)
if test "$enable_tweakdvdxml" = "yes"
then
    RUNTWEAKDVDXML=true
fi
AC_SUBST([RUNTWEAKDVDXML])

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

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([zvbi], [vbi_decoder_new])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])
AC_CHECK_HEADERS([libzvbi.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T

# Checks for library functions.
AC_FUNC_FSEEKO
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([alarm memset strdup strerror])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
