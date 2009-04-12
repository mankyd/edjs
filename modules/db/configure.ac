# Copyright 2004 Apache Software Foundation
 #
 #  Licensed under the Apache License, Version 2.0 (the "License");
 #  you may not use this file except in compliance with the License.
 #  You may obtain a copy of the License at
 #
 #      http://www.apache.org/licenses/LICENSE-2.0
 #
 #  Unless required by applicable law or agreed to in writing, software
 #  distributed under the License is distributed on an "AS IS" BASIS,
 #  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 #  See the License for the specific language governing permissions and
 #  limitations under the License.
 #
 # Originally developed by Gregory Trubetskoy.
 #

dnl Process this file with autoconf to produce a configure script.
AC_INIT()

dnl Checks for programs.

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET


AC_SUBST(INCLUDES)
AC_SUBST(CFLAGS)
AC_SUBST(LDLIBS)
AC_SUBST(LDFLAGS)
CFLAGS="-g -shared -fPIC -I/usr/include/edjs"
# includes
INCLUDES=""
LDFLAGS="-ledjs -lm"


dnl Checks for header files.

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST #defines preprocessor macro to string "const". not sure if its needed

AC_SUBST(ALL)
AC_SUBST(INSTALL_ALL)
ALL="db"
INSTALL_ALL="install-db"

# Now we check for spidermonkey configuration params
AC_SUBST(JS_CFLAGS)
AC_SUBST(JS_LIBS)

JS_CF="notfound"  #whether or not we've set the cflags
JS_L="notfound"   #whether or not we've set the libs
JS_OS="notfound"   #the target system flag


#first set for a command line setting for the target os
if test "${JS_OS}" = "notfound"; then
  AC_MSG_CHECKING(for --with-js-os)
  AC_ARG_WITH(js-os, [--with-js-os=NAME	Target OS (XP_UNIX, XP_WIN32, XP_OS2, XP_BEOS)],
  [
    JS_OS="${withval}"
    CFLAGS="${CFLAGS} -D${JS_OS}"
    AC_MSG_RESULT($JS_OS)
  ],
  [
    AC_MSG_RESULT(no)
  ])
fi

#check for command line specifications of libraries and includes
if test "${JS_CF}" = "notfound"; then
  AC_MSG_CHECKING(for --with-js-inc)
  AC_ARG_WITH(js-inc, [--with-js-inc=DIR Path for spidermonkey includes (i.e. '/usr/include/js/')],
  [
    JS_CF="${withval}"
  ],
  [
    AC_MSG_RESULT(no)
  ])
fi

if test "${JS_L}" = "notfound"; then
  AC_MSG_CHECKING(for --with-js-lib)
  AC_ARG_WITH(js-lib, [--with-js-lib=NAME Library name for spidermonkey (i.e. 'js' or 'mozjs')],
  [
    JS_L="${withval}"
  ],
  [
    AC_MSG_RESULT(no)
  ])
fi

#check for automatic flags that the system can provide
if test "${JS_CF}" = "notfound" || test "${JS_L}" = "notfound"; then
  PKG_CHECK_MODULES(JS, mozilla-js >= 1.7,
  [
    JS_CF=$JS_CFLAGS
#    JS_L=$JS_LIBS
  ],
  [
    PKG_CHECK_MODULES(JS, xulrunner-js >= 1.7,
    [
      JS_CF=$JS_CFLAGS
#      JS_L=$JS_LIBS
    ])
  ])
fi

#if we found automatic flags, use those to grab the target os
#remove the target os from the cflags
if test "${JS_CF}" != "notfound"; then
  if test "${JS_OS}" = "notfound"; then
    JS_OS="`echo $JS_CF | egrep -o -e \"-DXP_[[A-Z]]+\" | sed s/-D//`"
  fi

  JS_CF="`echo $JS_CF | sed s/-DXP_[[A-Z]]\\\+//`"

fi

#if we still don't have the target os, try to determine automatically
if test "${JS_OS}" = "notfound"; then
  AC_CANONICAL_SYSTEM
  AC_MSG_CHECKING([target os flags])
  case "$target_os" in
  *-beos*)
    JS_OS="XP_BEOS"
    ;;

  *cygwin*|*mingw*|*mks*|*msvc*|*wince)
    JS_OS="XP_WIN32"
    ;;

  *os2*)
    JS_OS="XP_OS2"
    ;;

  *-darwin*|*-linux*|*-kfreebsd*-gnu|*-osf*)
    JS_OS="XP_UNIX"
    ;;

  *)
    JS_OS="XP_UNIX"
    ;;
  esac

  AC_MSG_RESULT([$JS_OS])
fi

CFLAGS="${CFLAGS} -D${JS_OS} ${JS_CF}"
LDFLAGS="${LDFLAGS}" # ${JS_L}"




ARCHFLAGS=""
#LDFLAGS="${LDFLAGS}"
CFLAGS="${CFLAGS} ${JS_CFLAGS}"


## The goal is to find postgresql support
#AC_CHECKING(whether pg_config is available)
AC_SUBST(PG_CONFIG)
AC_SUBST(PG_CFLAGS)
AC_SUBST(PG_INCLUDES)
AC_SUBST(PG_LDFLAGS)
AC_SUBST(PG_LIBS)

# check for --with-apreq2-config
#AC_ARG_WITH(apreq2-config, AC_HELP_STRING([--with-apreq2-config=NAME],
#            [location of the apreq2-config executable [[apreq2]]]),
#            [APREQ2_CONFIG="$with_apreq2_config"])

#AC_ARG_WITH(pg-config, [--with-pg-config=NAME location of the pg_config executable [[pg_config]]],
#            [PG_CONFIG="$with_pg_config"])

if test -z "${PG_CONFIG}"; then
  AC_CHECK_PROG(PG_CONFIG, pg_config, [pg_config],[])
fi

if test -z "${PG_CONFIG}"; then
  AC_MSG_WARN([pg_config not found. excluding postgres support.])
else
  AC_MSG_CHECKING([for pg cflags])
  PG_CFLAGS="-I`${PG_CONFIG} --includedir`"
  AC_MSG_RESULT($PG_CFLAGS)
  PG_CFLAGS="${CFLAGS} ${PG_CFLAGS}"
  AC_MSG_CHECKING([for pg lib directory])
  PG_LIBS="-L`${PG_CONFIG} --libdir`"
  AC_MSG_RESULT($PG_LIBS)

  ALL="${ALL} db_pg"
  INSTALL_ALL="${INSTALL_ALL} install-db_pg"
fi


## The goal is to find mysql support
#AC_CHECKING(whether pg_config is available)
AC_SUBST(MYSQL_CONFIG)
AC_SUBST(MYSQL_CFLAGS)
AC_SUBST(MYSQL_INCLUDES)
AC_SUBST(MYSQL_LDFLAGS)
AC_SUBST(MYSQL_LIBS)

# check for --with-apreq2-config
#AC_ARG_WITH(apreq2-config, AC_HELP_STRING([--with-apreq2-config=NAME],
#            [location of the apreq2-config executable [[apreq2]]]),
#            [APREQ2_CONFIG="$with_apreq2_config"])

#AC_ARG_WITH(pg-config, [--with-pg-config=NAME location of the pg_config executable [[pg_config]]],
#            [MYSQL_CONFIG="$with_pg_config"])

if test -z "${MYSQL_CONFIG}"; then
  AC_CHECK_PROG(MYSQL_CONFIG, mysql_config, [mysql_config],[])
fi

if test -z "${MYSQL_CONFIG}"; then
  AC_MSG_WARN([mysql_config not found. excluding mysql support.])
else
  AC_MSG_CHECKING([for mysql cflags])
  MYSQL_CFLAGS="`${MYSQL_CONFIG} --cflags`"
  #MYSQL_INCLUDES="-I`${MYSQL_CONFIG} --includedir`"
  AC_MSG_RESULT($MYSQL_CFLAGS)
  MYSQL_CFLAGS="${CFLAGS} ${MYSQL_CFLAGS}"
  AC_MSG_CHECKING([for mysql lib flags])
  MYSQL_LIBS="`${MYSQL_CONFIG} --libs_r`"
  AC_MSG_RESULT($MYSQL_LIBS)

  ALL="${ALL} db_mysql"
  INSTALL_ALL="${INSTALL_ALL} install-db_mysql"
fi



## The goal is to find sqlite3 support
#AC_CHECKING(whether pg_config is available)
AC_SUBST(SQLITE3_CONFIG)
AC_SUBST(SQLITE3_CFLAGS)
AC_SUBST(SQLITE3_INCLUDES)
AC_SUBST(SQLITE3_LDFLAGS)
AC_SUBST(SQLITE3_LIBS)

#check for automatic flags that the system can provide
PKG_CHECK_MODULES(SQLITE3, sqlite3 >= 3.5,
[
  SQLITE3_CFLAGS="${CFLAGS} $SQLITE3_CFLAGS"
  SQLITE3_LIBS=$SQLITE3_LIBS
  ALL="${ALL} db_sqlite3"
  INSTALL_ALL="${INSTALL_ALL} install-db_sqlite3"
],
[
  AC_MSG_WARN([sqlite 3.5 not found. excluding sqlite support.])
])


AC_OUTPUT(Makefile)

