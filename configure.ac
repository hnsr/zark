dnl Initialize autoconf
AC_INIT([zark],[0.1],[hnsr@xs4all.nl])

dnl Try to keep as much autoconf stuff as possible out of the root dir..
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([foreign -Wall])

dnl Make building silent..
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])



dnl Check for programs, headers, libs.. ------------------------------------------------------------

AC_PROG_CC
AC_PROG_LEX
AC_PROG_YACC
AC_C_BIGENDIAN

dnl Attempt to enable C99 mode
AC_PROG_CC_C99
AS_IF([test "x$ac_cv_prog_cc_c99" = xno], AC_MSG_WARN([C99 wasn't enabled!]))

dnl TODO: Maybe also check for some of these
dnl time.h signal.h locale.h unistd.h pwd.h sys/types.h sys/stat.h sys/time.h
dnl TODO: Check for Xutf8 functions.

PKG_CHECK_MODULES(x11, x11)
AC_CHECK_HEADER([X11/keysym.h], [], [AC_MSG_ERROR(X11/keysym.h not found)])

dnl Some environments have lua installed without 5.1 postfixed (gentoo)
PKG_CHECK_MODULES(lua, [lua5.1], [],
        [PKG_CHECK_MODULES(lua, [lua], [],
            [AC_MSG_ERROR(lua not found, make sure it is installed.)]
        )]
)

PKG_CHECK_MODULES(ftgl, [ftgl > 2.1.2]) dnl Must be 2.1.3 or newer for ftgl.h

AC_CHECK_HEADER([GL/glew.h], [], [AC_MSG_ERROR(GL/glew.h not found, make sure glew is installed)])
AC_CHECK_HEADER([GL/glxew.h], [], [AC_MSG_ERROR(GL/glxew.h not found, make sure glew is installed)])
AC_CHECK_LIB([GLEW], [main], [], [AC_MSG_ERROR([libGLEW not found, make sure glew is installed])])

AC_CHECK_HEADER([IL/il.h], [], [AC_MSG_ERROR(IL/il.h not found, make sure DevIL is installed)])
AC_CHECK_LIB([IL], [main], [], [AC_MSG_ERROR([libIL not found, make sure DevIL is installed])])

AC_CHECK_HEADER([IL/ilu.h], [], [AC_MSG_ERROR(IL/ilu.h not found, make sure DevIL is installed)])
AC_CHECK_LIB([ILU], [main], [], [AC_MSG_ERROR([libILU not found, make sure DevIL is installed])])


CFLAGS="$CFLAGS $x11_CFLAGS $ftgl_CFLAGS $lua_CFLAGS"
LIBS="$LIBS $x11_LIBS $ftgl_LIBS $lua_LIBS"



dnl Misc stuff -------------------------------------------------------------------------------------

dnl For now I requre the user to pass the path to the asset data..
AC_ARG_WITH(
    [asset-dir],
    AS_HELP_STRING([--with-asset-dir=DIR], [Path to assets (models, textures, etc)]),
    ASSET_DIR=$withval,
    AC_MSG_ERROR([must give asset path with --with-asset-dir])
)
dnl AC_DEFINE_UNQUOTED([ASSET_DIR], "$ASSET_DIR", [Asset directory])
AC_SUBST(ASSET_DIR)

dnl Check if I need to enable debugging stuff.
AC_ARG_ENABLE(
    [debug],
    AS_HELP_STRING([--enable-debug], [Compile with debugging features])
)
if test "x$enable_debug" = xyes; then
    AC_DEFINE([DEBUG], [], [Compile with debugging features])
    dnl I am not entirely sure if I should add -g to CFLAGS here, it's added by default, unless the
    dnl user specified custom CFLAGS. 
fi

dnl Add a few things to CFLAGS. Maybe I shouldnt bother with the warnings, since they are only
dnl relevant to me?
CFLAGS="$CFLAGS -ffast-math -Wall -Wdeclaration-after-statement -Wsign-compare"
   


dnl Output files -----------------------------------------------------------------------------------

dnl Declare output header
AC_CONFIG_HEADERS([config.h])


dnl Declare output files
AC_CONFIG_FILES([ Makefile src/Makefile ])

dnl Create output files
AC_OUTPUT



dnl Debugging stuff --------------------------------------------------------------------------------
echo "Using CFLAGS:      $CFLAGS"
echo "Using LDFLAGS:     $LDFLAGS"
echo "Using CPPFLAGS:    $CPPFLAGS"
echo "Using LIBS:        $LIBS"
echo "Using CC:          $CC"
echo "Using YACC:        $YACC"

