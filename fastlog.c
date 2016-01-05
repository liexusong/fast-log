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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <pthread.h>

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_fastlog.h"
#include "spin.h"

/* If you declare any globals in php_fastlog.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(fastlog)
*/

#define FASTLOG_CLASS_INSTANCE_FIELD  "__instance__"
#define FASTLOG_CLASS_LEVEL_FIELD     "__level__"
#define FASTLOG_CLASS_LOGPATH_FIELD   "__logpath__"
#define FASTLOG_CLASS_FILENAME_FIELD  "__filename__"

/* True global resources - no need for thread safety here */
static int le_fastlog;
static int env_init = 0;
static zend_class_entry *fastlog_ce; /* Class entry struct */
static fastlog_manager_t __manager, *manager_ptr = &__manager;

/* {{{ fastlog_functions[]
 *
 * Every user visible function must have an entry in fastlog_functions[].
 */
const zend_function_entry fastlog_functions[] = {
	PHP_FE_END	/* Must be the last line in fastlog_functions[] */
};
/* }}} */

/* {{{ fastlog_module_entry
 */
zend_module_entry fastlog_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"fastlog",
	fastlog_functions,
	PHP_MINIT(fastlog),
	PHP_MSHUTDOWN(fastlog),
	PHP_RINIT(fastlog),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(fastlog),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(fastlog),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_FASTLOG_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_FASTLOG
ZEND_GET_MODULE(fastlog)
#endif

typedef enum {
	FASTLOG_DEUBG_LEVEL = 0,
	FASTLOG_NOTICE_LEVEL,
	FASTLOG_ERROR_LEVEL
} fastlog_level_t;


static char *level_prefixs[] = {"debug", "notice", "error"};


int
get_datetime()
{
	time_t ts;
	struct tm *tmp;
	int retval = 0;

	ts = time(NULL);
	tmp = localtime(&ts);

	retval = (tmp->tm_year + 1900) << 16
		   | tmp->tm_mon << 8
		   | tmp->tm_mday;

	return retval;
}


int
fastlog_update_logfile(void)
{
	int datetime;
	int year, mon, date;
	char fullpath[2048];
	zval *instance, *logpath, *filename;

	instance = get_instance();

	logpath = zend_read_property(fastlog_ce, instance,
		ZEND_STRL(FASTLOG_CLASS_LOGPATH_FIELD), 0 TSRMLS_CC);
	filename = zend_read_property(fastlog_ce, instance,
		ZEND_STRL(FASTLOG_CLASS_FILENAME_FIELD), 0 TSRMLS_CC);

	datetime = get_datetime();

	year = datetime >> 16;
	mon = (datetime >> 8) & (0x000000FF);
	date = datetime & 0x000000FF;

	sprintf(fullpath, "%s/%s_%04d-%02d-%02d.log",
		Z_STRVAL_P(logpath), Z_STRVAL_P(filename), year, mon, date);

	if (manager_ptr->logfd != -1) {
		close(manager_ptr->logfd);
	}

	manager_ptr->logfd = open(fullpath, O_CREAT|O_WRONLY|O_APPEND, 0777);

	return manager_ptr->logfd == -1 ? -1 : 0;
}


void *
fastlog_thread_worker(void *arg)
{
	fd_set read_set;
	int notify = manager_ptr->notifiers[0];
	struct timeval tv = {10, 0}; /* 10 seconds to update log file */

	FD_ZERO(&read_set);

	for (;;) {

		FD_SET(notify, &read_set);

		(void)select(notify + 1, &read_set, NULL, NULL, &tv);

		if (tv.tv_sec == 0 && tv.tv_usec == 0) { /* update log file */
			tv.tv_sec = 10;
			tv.tv_usec = 0;
do_again:
			if (fastlog_update_logfile() == -1) {
				sleep(1);
				goto do_again;
			}
		}

		if (FD_ISSET(notify, &read_set)) {
			char tmp;
			int result;
			fastlog_item_t *item;

			for (;;) {
				result = read(notify, &tmp, 1);
				if (result == -1 || tmp != '\0') {
					break;
				}

				/* Get item from worker queue */

				spin_lock(&manager_ptr->qlock);

				item = manager_ptr->head;

				if (item) {
					manager_ptr->head = item->next;
				} else {
					manager_ptr->head = NULL;
				}

				if (!manager_ptr->head) {
					manager_ptr->tail = NULL;
				}

				spin_unlock(&manager_ptr->qlock);

				if (item) {
					result = write(manager_ptr->logfd, item->buffer, item->size);
					free(item);
				}
			}
		}
	}

	return NULL;
}


int
fastlog_env_init(void)
{
	pthread_t tid;

	manager_ptr->head = NULL;
	manager_ptr->tail = NULL;
	manager_ptr->qlock = 0; /* init can lock */
	manager_ptr->logfd = -1;

	if (pipe(manager_ptr->notifiers) == -1) {
		return -1;
	}

	if (fastlog_update_logfile() == -1) {
		close(manager_ptr->notifiers[0]);
		close(manager_ptr->notifiers[1]);
		return -1;
	}

	if (pthread_create(&tid, NULL,
		fastlog_thread_worker, NULL) == -1)
	{
		close(manager_ptr->notifiers[0]);
		close(manager_ptr->notifiers[1]);
		close(manager_ptr->logfd);
		return -1;
	}

	return 0;
}


zval *
get_instance(void)
{
	zval *instance;

	instance = zend_read_static_property(fastlog_ce,
		ZEND_STRL(FASTLOG_CLASS_INSTANCE_FIELD), 0 TSRMLS_CC);
	if (IS_OBJECT == Z_TYPE_P(instance)
		&& instanceof_function(Z_OBJCE_P(instance), fastlog_ce TSRMLS_CC))
	{
		return instance;
	}

	MAKE_STD_ZVAL(instance);
	object_init_ex(instance, fastlog_ce);
	/* Add instance to static property */
	zend_update_static_property(fastlog_ce,
		ZEND_STRL(FASTLOG_CLASS_INSTANCE_FIELD), instance TSRMLS_CC);

	return instance;
}


int
fastlog_write_log(int level, char *content, int length)
{
	fastlog_item_t *item;
	zval *instance, *max_level;
	int prefix;

	time_t ts;
	struct tm *tmp;
	int result;

	if (level < FASTLOG_DEUBG_LEVEL
		|| level > FASTLOG_ERROR_LEVEL)
	{
		return -1;
	}

	instance = get_instance();

	max_level = zend_read_property(fastlog_ce,
		instance, ZEND_STRL(FASTLOG_CLASS_LEVEL_FIELD), 0 TSRMLS_CC);
	if (Z_LVAL_P(max_level) > level) {
		return 0;
	}

	/* 8 is level string example: "debug: " */
	prefix = sizeof("[1900/00/00 00:00:00] ") + 8;
	length = sizeof(*item) + prefix + length + 1;

	item = (fastlog_item_t *)malloc(length);
	if (!item) {
		return -1;
	}

	ts = time(NULL);
	tmp = localtime(&ts);

	length = sprintf(item->buffer,
		"[%04d/%02d/%02d %02d:%02d:%02d] %s: %s\n",
		tmp->tm_year + 1900, tmp->tm_mon, tmp->tm_mday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
		level_prefixs[level], content);

	item->size = length;
	item->next = NULL;

	/* Add item to worker queue */

	spin_lock(&manager_ptr->qlock);

	if (!manager_ptr->head) {
		manager_ptr->head = item;
	}

	if (manager_ptr->tail) {
		manager_ptr->tail->next = item;
	}

	manager_ptr->tail = item;

	spin_unlock(&manager_ptr->qlock);

	/* notify worker thread */
	result = write(manager_ptr->notifiers[1], "\0", 1);

	return 0;
}


zend_function_entry fastlog_methods[] = {
	PHP_ME(FastLog, __construct, NULL, ZEND_ACC_PRIVATE|ZEND_ACC_CTOR)
	PHP_ME(FastLog, getInstance, NULL, ZEND_ACC_STATIC|ZEND_ACC_PUBLIC)
	PHP_ME(FastLog, init,        NULL, ZEND_ACC_PUBLIC)
	PHP_ME(FastLog, debug,       NULL, ZEND_ACC_PUBLIC)
	PHP_ME(FastLog, notice,      NULL, ZEND_ACC_PUBLIC)
	PHP_ME(FastLog, error,       NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};


PHP_METHOD(FastLog, __construct)
{
}

/*
 * FastLog::getInstance()
 */
PHP_METHOD(FastLog, getInstance)
{
	zval *instance;

	instance = get_instance();

	RETURN_ZVAL(instance, 1, 0);
}


PHP_METHOD(FastLog, init)
{
	long max_level;
	char *logpath, *filename;
	int logpath_len, filename_len;
	zval *instance;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"lss", &max_level, &logpath, &logpath_len, &filename, &filename_len)
		== FAILURE)
	{
		RETURN_FALSE;
	}

	instance = get_instance();
	if (!instance) {
		RETURN_FALSE;
	}

	zend_update_property_long(fastlog_ce, instance,
		ZEND_STRL(FASTLOG_CLASS_LEVEL_FIELD), max_level TSRMLS_CC);
	zend_update_property_string(fastlog_ce, instance,
		ZEND_STRL(FASTLOG_CLASS_LOGPATH_FIELD), logpath TSRMLS_CC);
	zend_update_property_string(fastlog_ce, instance,
		ZEND_STRL(FASTLOG_CLASS_FILENAME_FIELD), filename TSRMLS_CC);

	if (!env_init) {
		if (fastlog_env_init() == -1) {
			RETURN_FALSE;
		}
		env_init = 1;
	}

	RETURN_ZVAL(instance, 1, 0);
}


/*
 * FastLog::debug()
 */
PHP_METHOD(FastLog, debug)
{
	char *msg;
	int msg_len;

	if (!env_init
		|| zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &msg, &msg_len)
		== FAILURE)
	{
		RETURN_FALSE;
	}

	if(fastlog_write_log(FASTLOG_DEUBG_LEVEL, msg, msg_len) == -1) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}


/*
 * FastLog::notice()
 */
PHP_METHOD(FastLog, notice)
{
	char *msg;
	int msg_len;

	if (!env_init
		|| zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &msg, &msg_len)
		== FAILURE)
	{
		RETURN_FALSE;
	}

	if(fastlog_write_log(FASTLOG_NOTICE_LEVEL, msg, msg_len) == -1) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}


/*
 * FastLog::error()
 */
PHP_METHOD(FastLog, error)
{
	char *msg;
	int msg_len;

	if (!env_init
		|| zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &msg, &msg_len)
		== FAILURE)
	{
		RETURN_FALSE;
	}

	if(fastlog_write_log(FASTLOG_ERROR_LEVEL, msg, msg_len) == -1) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(fastlog)
{
	zend_class_entry ce;

	spin_init();

	INIT_CLASS_ENTRY(ce, "FastLog", fastlog_methods);

	fastlog_ce = zend_register_internal_class_ex(&ce, NULL, NULL TSRMLS_CC);

	zend_declare_property_null(fastlog_ce,
		ZEND_STRL(FASTLOG_CLASS_LEVEL_FIELD), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(fastlog_ce,
		ZEND_STRL(FASTLOG_CLASS_LOGPATH_FIELD), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(fastlog_ce,
		ZEND_STRL(FASTLOG_CLASS_FILENAME_FIELD), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(fastlog_ce,
		ZEND_STRL(FASTLOG_CLASS_INSTANCE_FIELD), ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);

	zend_declare_class_constant_long(fastlog_ce,
		ZEND_STRL("DEBUG"), FASTLOG_DEUBG_LEVEL TSRMLS_CC);
	zend_declare_class_constant_long(fastlog_ce,
		ZEND_STRL("NOTICE"), FASTLOG_NOTICE_LEVEL TSRMLS_CC);
	zend_declare_class_constant_long(fastlog_ce,
		ZEND_STRL("ERROR"), FASTLOG_ERROR_LEVEL TSRMLS_CC);

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(fastlog)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */


/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(fastlog)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(fastlog)
{
	return SUCCESS;
}

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(fastlog)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "FastLog support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
