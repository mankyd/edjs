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
AC_INIT([EDJS], 0.1, [mankyd@gmail.com])

dnl Checks for programs.

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET


AC_SUBST(CFLAGS)
AC_SUBST(SM_CFLAGS)
AC_SUBST(LIB_CFLAGS)
AC_SUBST(MOD_CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(SM_LDFLAGS)
AC_SUBST(LIB_LDFLAGS)
AC_SUBST(MOD_LDFLAGS)
AC_SUBST(LDLIBS)
AC_SUBST(SM_LDLIBS)
AC_SUBST(LIB_LDLIBS)
AC_SUBST(MOD_LDLIBS)
AC_SUBST(INCLUDES)
AC_SUBST(SM_INCLUDES)
AC_SUBST(LIB_INCLUDES)
AC_SUBST(MOD_INCLUDES)

AC_SUBST(SM_OBJDIR)
AC_SUBST(SM_MFLAGS)

CFLAGS="-I`pwd`/spidermonkey"
INCLUDES=""
LDFLAGS=""
LDLIBS="-lm"

MOD_CFLAGS=""
MOD_INCLUDES=""
MOD_LDFLAGS=""
MOD_LDLIBS=""

LIB_CFLAGS=""
LIB_INCLUDES=""
LIB_LDFLAGS=""
LIB_LDLIBS=""

SM_OBJDIR="edjs"
SM_JS_DIST=""
SM_MFLAGS="JS_THREADSAFE=1 JS_HAS_FILE_OBJECT=1"
SM_CFLAGS=""
SM_LDFLAGS=""
SM_INCLUDES=""

#set the location of the settings file
AC_SUBST(EDJS_SETTINGS_FILE_LOC)
AC_ARG_WITH(modules, [--with-setting=NAME location of edjs settings file [[modules]]],
[EDJS_SETTINGS_FILE_LOC="$with_setting"],
[EDJS_SETTINGS_FILE_LOC="/etc/edjs/settings.ed"])

LIB_CFLAGS="${LIB_CFLAGS} -DEDJS_SETTINGS_FILE_LOC=\"\\\"${EDJS_SETTINGS_FILE_LOC}\\\"\""

#set the location of the core file
AC_SUBST(EDJS_CORE_FILE_LOC)
AC_ARG_WITH(core, [--with-core=NAME location of the core file [[core]]],
[EDJS_CORE_FILE_LOC="$with_core"],
[EDJS_CORE_FILE_LOC="/usr/lib/edjs/core.ed"])

LIB_CFLAGS="${LIB_CFLAGS} -DEDJS_CORE_FILE_LOC=\"\\\"${EDJS_CORE_FILE_LOC}\\\"\""

#set the location of the core file
AC_SUBST(EDJS_MOD_LOC)
AC_ARG_WITH(modules, [--with-modules=NAME location of edjs modules [[modules]]],
[EDJS_MOD_LOC="$with_modules"],
[EDJS_MOD_LOC="/usr/lib/edjs/modules/"])

LIB_CFLAGS="${LIB_CFLAGS} -DEDJS_MOD_LOC=\"\\\"${EDJS_MOD_LOC}\\\"\""


AC_C_CONST #defines preprocessor macro to string "const". not sure if its needed

AC_SUBST(ALL)

ALL="libedjs"

# Now we check for spidermonkey configuration params

JS_OS="notfound"   #the target system flag

#first check for a command line setting for the target os
if test "${JS_OS}" = "notfound"; then
  AC_MSG_CHECKING(for --with-js-os)
  AC_ARG_WITH(js-os, [--with-js-os=NAME	Target OS (XP_UNIX, XP_WIN32, XP_OS2, XP_BEOS)],
  [
    JS_OS="${withval}"
    AC_MSG_RESULT($JS_OS)
  ],
  [
    AC_MSG_RESULT(no)
  ])
fi

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

#look for nspr4

NSPR_CF="notfound"
NSPR_L="notfound"

if test "${NSPR_CF}" = "notfound"; then
  AC_MSG_CHECKING(for --with-nspr-cflags)
  AC_ARG_WITH(nspr-cflags, [--with-nspr-cflags=NAME	CFLAGS needed for nspr (i.e. -I/usr/include/nspr)],
  [
    NSPR_CF="${withval}"
    AC_MSG_RESULT($NSPR_CF)
  ],
  [
    AC_MSG_RESULT(no)
  ])
fi

if test "${NSPR_L}" = "notfound"; then
  AC_MSG_CHECKING(for --with-nspr-libs)
  AC_ARG_WITH(nspr-cflags, [--with-nspr-libs=NAME	libraries needed for nspr (i.e. -lnspr4)],
  [
    NSPR_L="${withval}"
    AC_MSG_RESULT($NSPR_L)
  ],
  [
    AC_MSG_RESULT(no)
  ])
fi

if test "${NSPR_CF}" = "notfound" || test "${$NSPR_L}" = "notfound"; then
  PKG_CHECK_MODULES(NSPR, nspr >= 4,
  [
    if test "${NSPR_CF}" = "notfound"; then
      NSPR_CF=$NSPR_CFLAGS
    fi
    if test "${NSPR_L}" = "notfound"; then
      NSPR_L=$NSPR_LIBS
    fi
  ])
fi

SM_JS_DIST="`echo ${NSPR_CF} | grep -E -o -e \"-I.*/include/nspr\" | sed -e \"s/\/include\/nspr//g\" -e \"s/-I//g\"`"

SM_MFLAGS="${SM_MFLAGS} JS_DIST=${SM_JS_DIST}"

## The goal is to find apxs
AC_CHECKING(whether apxs is available)
AC_SUBST(APXS)

# check for --with-apxs
AC_ARG_WITH(apxs, [--with-apxs=NAME name of the apxs executable [[apxs]]],
            [APXS="$with_apxs"])

if test -z "${APXS}"; then
    AC_PATH_PROGS(APXS, apxs2 apxs, [],
                   [$PATH:/usr/local/apache/bin:/usr/sbin])
fi

# if apxs was still not found, then no DSO
AC_SUBST(LIBEXECDIR)
AC_SUBST(HTTPD)
if test -z "$APXS"; then
  AC_MSG_WARN([**** apxs was not found, DSO compilation will not be available.])
  AC_MSG_WARN([**** You can use --with-apxs to specify where your apxs is.])
  DSO="no_dso"
#  ALL="${ALL} static"

else
  DSO="do_dso"
  ALL="${ALL} mod_edjs"

  # check Apache version
  AC_MSG_CHECKING(Apache version)
  HTTPD="`${APXS} -q SBINDIR`/`${APXS} -q TARGET`"
  ver=`$HTTPD -v | awk '/version/ {print $3}' | awk -F/ '{print $2}'`
  AC_MSG_RESULT($ver)

  # make sure version begins with 2
  if test -z "`echo $ver | egrep \^2`"; then
    AC_MSG_ERROR([This version of mod_edjs only works with Apache 2. The one you have seems to be $ver.])
  fi

  # determine LIBEXEC
  AC_MSG_CHECKING(for Apache libexec directory)
  LIBEXECDIR=`${APXS} -q LIBEXECDIR`
  AC_MSG_RESULT($LIBEXECDIR)

  # determine INCLUDES
  AC_MSG_CHECKING([for Apache include directory])
  AP_INCLUDES="-I`${APXS} -q INCLUDEDIR`"
  AC_MSG_RESULT($AP_INCLUDES)

#  dnl Small hack to work around _eprintf.o problem on Solaris
#  if test "`uname`" = "SunOS"; then
#    AC_MSG_CHECKING([for gcc on Solaris possible missing _eprintf problem])
#    if test "$CC" = "gcc"; then
#      SOLARIS_HACKS="_eprintf.o _floatdidf.o _muldi3.o"
#    fi
#    AC_MSG_RESULT("done")
#  fi

fi

# check for --with-apache
AC_SUBST(AP_SRC)
AC_SUBST(AP_SRC_OWN)
AC_SUBST(AP_SRC_GRP)
## static is disabled, thus no --with-apache
##AC_MSG_CHECKING(for --with-apache)
AC_ARG_WITH(apache, [--with-apache=DIR 	Path to Apache sources],
[

  # temporarily disable static on 2.0 until I figure out how to
  # do it right
  AC_MSG_ERROR([Sorry, --with-apache (static compilation) is not supported at this time!])

  AP_SRC=`cd $withval; pwd`

  dnl  Make sure this looks like Apache source
  if test ! -f "$AP_SRC/include/httpd.h"; then
    AC_MSG_ERROR([$withval does not look like an Apache 2.0 source directory.])
  fi

  AC_MSG_RESULT($AP_SRC)
  AP_INCLUDES="-I${AP_SRC}/src/include -I${AP_SRC}/src/os/unix"

  # note who owns the apache source directory
  AP_SRC_OWN="`ls -ld $AP_SRC | awk '{print $3}'`"
  AP_SRC_GRP="`ls -ld $AP_SRC | awk '{print $4}'`"
],)
##AC_MSG_RESULT(no))

#AC_SUBST(STATIC)
#if test -z "$AP_SRC"; then
##  AC_MSG_WARN([**** No apache sources specified, static compilation will not be available.])
##  AC_MSG_WARN([**** You can use --with-apache to specify where your Apache sources are.])
#  STATIC="no_static"
#else
#  STATIC="do_static"
#fi

#if test "$STATIC" = "no_static" -a "$DSO" = "no_dso"; then
#  AC_MSG_ERROR([Neither static nor DSO option available, there is no point in continuing.])
#fi

## The goal is to find apreq2
AC_CHECKING(whether apreq2 is available)
AC_SUBST(APREQ2_CONFIG)
AC_SUBST(APREQ2_INCLUDES)

# check for --with-apreq2-config
#AC_ARG_WITH(apreq2-config, AC_HELP_STRING([--with-apreq2-config=NAME],
#            [location of the apreq2-config executable [[apreq2]]]),
#            [APREQ2_CONFIG="$with_apreq2_config"])

AC_ARG_WITH(apreq2-config, [--with-apreq2-config=NAME location of the apreq2-config executable [[apreq2-config]]],
            [APREQ2_CONFIG="$with_apreq2_config"])

if test -z "${APREQ2_CONFIG}"; then
  AC_CHECK_PROG(APREQ2_CONFIG, apreq2-config, [apreq2-config],[])
fi

if test -z "${APREQ2_CONFIG}"; then
  AC_MSG_ERROR([apreq2-config not found. can not continue.])
else
  AC_MSG_CHECKING([for apreq2 include directory])
  APREQ2_INCLUDES="-I`${APREQ2_CONFIG} --includedir`"
  AC_MSG_RESULT($APREQ2_INCLUDES)
fi

SM_CFLAGS="-D`echo $SM_MFLAGS | sed \"s/ / -D/g\"`"

SM_MFLAGS="${SM_MFLAGS} NSPR_INCLUDES=\"${NSPR_CF}\" NSPR_LIBS=\"${NSPR_L}\""
CFLAGS="${CFLAGS} -D${JS_OS} -I`pwd`/spidermonkey/${SM_OBJDIR}"

LIB_CFLAGS="${CFLAGS} ${LIB_CFLAGS} ${NSPR_CF} ${SM_CFLAGS} -g -fPIC"
MOD_CFLAGS="${CFLAGS} ${MOD_CFLAGS} ${SM_CFLAGS} -Wc,-g"

LIB_INCLUDES="${INCLUDES} ${LIB_INCLUDES} -I`pwd`/src"
MOD_INCLUDES="${INCLUDES} ${MOD_INCLUDES} -I../src ${APREQ2_INCLUDES}"

LIB_LDFLAGS="${LDFLAGS} ${LIB_LDFLAGS} -shared"
MOD_LDFLAGS="${LDFLAGS} ${MOD_LDFLAGS} -L../src"

LIB_LDLIBS="${LDLIBS} ${LIB_LDLIBS} ${NSPR_L} `pwd`/spidermonkey/${SM_OBJDIR}/libjs.a"
MOD_LDLIBS="${LDLIBS} ${MOD_LDLIBS} -ledjs `${APREQ2_CONFIG} --link-libtool`"

AC_OUTPUT(Makefile src/Makefile apache2/Makefile)

