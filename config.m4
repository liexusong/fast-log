dnl $Id$
dnl config.m4 for extension fastlog

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(fastlog, for fastlog support,
dnl Make sure that the comment is aligned:
[  --with-fastlog             Include fastlog support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(fastlog, whether to enable fastlog support,
dnl Make sure that the comment is aligned:
dnl [  --enable-fastlog           Enable fastlog support])

if test "$PHP_FASTLOG" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-fastlog -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/fastlog.h"  # you most likely want to change this
  dnl if test -r $PHP_FASTLOG/$SEARCH_FOR; then # path given as parameter
  dnl   FASTLOG_DIR=$PHP_FASTLOG
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for fastlog files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       FASTLOG_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$FASTLOG_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the fastlog distribution])
  dnl fi

  dnl # --with-fastlog -> add include path
  dnl PHP_ADD_INCLUDE($FASTLOG_DIR/include)

  dnl # --with-fastlog -> check for lib and symbol presence
  dnl LIBNAME=fastlog # you may want to change this
  dnl LIBSYMBOL=fastlog # you most likely want to change this

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
PHP_ADD_LIBRARY_WITH_PATH(pthread,, FASTLOG_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_FASTLOGLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong fastlog lib version or lib not found])
  dnl ],[
  dnl   -L$FASTLOG_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
PHP_SUBST(FASTLOG_SHARED_LIBADD)

  PHP_NEW_EXTENSION(fastlog, fastlog.c spin.c, $ext_shared)
fi
