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
CFLAGS="-g -Wall -shared -fPIC -I/usr/include/edjs -I/usr/local/include/libmemcached"
# includes
INCLUDES=""
LDFLAGS="-ledjs -lm -lmemcached"


dnl Checks for header files.

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST #defines preprocessor macro to string "const". not sure if its needed

AC_SUBST(ALL)
AC_SUBST(INSTALL_ALL)
ALL="memcached"
INSTALL_ALL="install-memcached"

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


AC_OUTPUT(Makefile)

