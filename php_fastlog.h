/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2015 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:  Liexusong <liexusong@qq.com>                                |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_FASTLOG_H
#define PHP_FASTLOG_H

extern zend_module_entry fastlog_module_entry;
#define phpext_fastlog_ptr &fastlog_module_entry

#define PHP_FASTLOG_VERSION "0.1.0" /* Replace with version number for your extension */

typedef struct fastlog_item_s {
    struct fastlog_item_s *next;
    int size;
    char buffer[1];
} fastlog_item_t;

typedef struct {
    fastlog_item_t *head,
                   *tail;
    int logfd;
    int notifiers[2];
    volatile int qlock;
} fastlog_manager_t;

#ifdef PHP_WIN32
#	define PHP_FASTLOG_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_FASTLOG_API __attribute__ ((visibility("default")))
#else
#	define PHP_FASTLOG_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(fastlog);
PHP_MSHUTDOWN_FUNCTION(fastlog);
PHP_RINIT_FUNCTION(fastlog);
PHP_RSHUTDOWN_FUNCTION(fastlog);
PHP_MINFO_FUNCTION(fastlog);

PHP_FUNCTION(confirm_fastlog_compiled);	/* For testing, remove later. */

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     

ZEND_BEGIN_MODULE_GLOBALS(fastlog)
	long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(fastlog)
*/

/* In every utility function you add that needs to use variables 
   in php_fastlog_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as FASTLOG_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define FASTLOG_G(v) TSRMG(fastlog_globals_id, zend_fastlog_globals *, v)
#else
#define FASTLOG_G(v) (fastlog_globals.v)
#endif

extern zend_class_entry *fastlog_ce;

PHP_METHOD(FastLog, __construct);
PHP_METHOD(FastLog, getInstance);
PHP_METHOD(FastLog, init);
PHP_METHOD(FastLog, debug);
PHP_METHOD(FastLog, notice);
PHP_METHOD(FastLog, error);

#endif	/* PHP_FASTLOG_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
