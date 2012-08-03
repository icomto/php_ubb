/*
apt-get install php5-dev libpcre3-dev uthash-dev
wget -O/dev/stdout -q 'http://downloads.sourceforge.net/project/uthash/uthash/uthash-1.9.5/uthash-1.9.5.tar.bz2?r=http%3A%2F%2Futhash.sourceforge.net%2F&ts=1333157148&use_mirror=switch'|bunzip2|tar x && cp uthash-1.9.5/src/uthash.h /usr/include/uthash.h && rm -rf uthash-1.9.5
phpize
./configure
make -j4 && make install && php test.php
echo extension=ubb.so >/etc/php5/conf.d/ubb.ini
/etc/init.d/php5-fpm restart
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ubb.h"

#include "lib/ubb.c"

#include "lib/ubb_single_php.c"
#include "lib/ubb_single_replace.c"
#include "lib/ubb_single_replace_regex.c"

#include "lib/ubb_block_php.c"
#include "lib/ubb_block_snippet.c"
#include "lib/ubb_block_template.c"

static zend_function_entry ubb_functions[] = {
    PHP_FE(ubb_init, NULL)
    PHP_FE(ubb_free, NULL)
	
    PHP_FE(ubb_pinit, NULL)
    PHP_FE(ubb_pfree, NULL)
	
    PHP_FE(ubb_group_del, NULL)
	
    PHP_FE(ubb_single_add, NULL)
    PHP_FE(ubb_single_del, NULL)
	
    PHP_FE(ubb_block_add, NULL)
    PHP_FE(ubb_block_del, NULL)
	
    PHP_FE(ubb_parse, NULL)
	
    {NULL, NULL, NULL}
};

zend_module_entry ubb_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_UBB_EXTNAME,
    ubb_functions,
    PHP_MINIT(ubb),
    PHP_MSHUTDOWN(ubb),
    NULL,
    PHP_RSHUTDOWN(ubb),
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_UBB_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_UBB
ZEND_GET_MODULE(ubb)
#endif



typedef struct ubb_non_persist_tags_s {
	UT_hash_handle hh;
	char *name;
	int type;
} ubb_non_persist_tags_t;

typedef struct ubb_handle_s {
	ubb_t *ubb;
	
	long rsrc_id;
	
	UT_hash_handle hh;
	int persist;
	char *name;
	int name_len;
	
	ubb_non_persist_tags_t *non_persist_tags;
} ubb_handle_t;

static int le_ubb_resource;
#define le_ubb_resource_name "UBB"

void ubb_resource_destruction_handler(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
	ubb_handle_t *h = (ubb_handle_t*)rsrc->ptr;
	if(h->persist) h->rsrc_id = 0;
	else {
		ubb_free(h->ubb);
		efree(h);
	}
}


ubb_handle_t *ubb_persist = NULL;



PHP_MINIT_FUNCTION(ubb) {
	le_ubb_resource = zend_register_list_destructors_ex(ubb_resource_destruction_handler, NULL, le_ubb_resource_name, module_number);
	
	REGISTER_LONG_CONSTANT("UBB_CALLBACK",		0, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("UBB_REPLACE",		1, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("UBB_REPLACE_REGEX",	2, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("UBB_SNIPPET",		3, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("UBB_TEMPLATE",		4, CONST_CS|CONST_PERSISTENT);
	
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(ubb) {
	ubb_handle_t *h, *h_tmp;
	ubb_non_persist_tags_t *p, *p_tmp;
	HASH_ITER(hh, ubb_persist, h, h_tmp) {
		HASH_DELETE(hh, ubb_persist, h);
		if(h->rsrc_id) zend_list_delete(h->rsrc_id);
		if(h->name) free(h->name);
		ubb_free(h->ubb);
		HASH_ITER(hh, h->non_persist_tags, p, p_tmp) {
			HASH_DELETE(hh, h->non_persist_tags, p);
			free(p->name);
			free(p);
		}
		free(h);
	}
	return SUCCESS;
}



PHP_RSHUTDOWN_FUNCTION(ubb) {
	ubb_handle_t *h, *h_tmp;
	ubb_non_persist_tags_t *p, *p_tmp;
	HASH_ITER(hh, ubb_persist, h, h_tmp) {
		HASH_ITER(hh, h->non_persist_tags, p, p_tmp) {
			HASH_DELETE(hh, h->non_persist_tags, p);
			switch(p->type) {
			case 1:
				ubb_single_del(h->ubb, p->name);
				break;
			case 2:
				ubb_block_del(h->ubb, p->name);
				break;
			}
			free(p->name);
			free(p);
		}
	}
}



PHP_FUNCTION(ubb_init) {
	ubb_handle_t *h;
	
	h = emalloc(sizeof(ubb_handle_t));
	h->ubb = ubb_init(0);
	h->name = NULL;
	h->persist = 0;
	h->non_persist_tags = NULL;
	
	h->rsrc_id = ZEND_REGISTER_RESOURCE(return_value, h, le_ubb_resource);
}

PHP_FUNCTION(ubb_free) {
	zval *ubb;
	ubb_handle_t *h;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ubb) == FAILURE) return;
	ZEND_FETCH_RESOURCE(h, ubb_handle_t*, &ubb, -1, le_ubb_resource_name, le_ubb_resource);
	ZEND_VERIFY_RESOURCE(h);
	
	zend_list_delete(Z_LVAL_P(ubb));
	ZVAL_NULL(ubb);
	
	RETURN_TRUE
}



PHP_FUNCTION(ubb_pinit) {
	ubb_handle_t *h;
	char *name = NULL;
	int name_len;
	zval *cached;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &name_len, &cached) == FAILURE) return;
	
	HASH_FIND(hh, ubb_persist, name, name_len, h);
	if(h) {
		ZVAL_TRUE(cached);
	}
	else {
		h = (ubb_handle_t*)malloc(sizeof(ubb_handle_t));
		h->persist = 1;
		h->non_persist_tags = NULL;
		h->name = strdup(name);
		h->name_len = name_len;
		h->ubb = ubb_init(0);
		
		HASH_ADD_KEYPTR(hh, ubb_persist, h->name, h->name_len, h);
		ZVAL_FALSE(cached);
	}
	
	h->rsrc_id = ZEND_REGISTER_RESOURCE(return_value, h, le_ubb_resource);
}

PHP_FUNCTION(ubb_pfree) {
	zval *ubb;
	ubb_handle_t *h;
	ubb_non_persist_tags_t *p, *p_tmp;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ubb) == FAILURE) return;
	ZEND_FETCH_RESOURCE(h, ubb_handle_t*, &ubb, -1, le_ubb_resource_name, le_ubb_resource);
	ZEND_VERIFY_RESOURCE(h);
	
	if(!h->persist) zend_list_delete(Z_LVAL_P(ubb));
	else {
		HASH_DELETE(hh, ubb_persist, h);
		if(h->rsrc_id) zend_list_delete(h->rsrc_id);
		if(h->name) free(h->name);
		ubb_free(h->ubb);
		
		HASH_ITER(hh, h->non_persist_tags, p, p_tmp) {
			HASH_DELETE(hh, h->non_persist_tags, p);
			free(p->name);
			free(p);
		}
		
		free(h);
	}
	
	ZVAL_NULL(ubb);
	
	RETURN_TRUE
}



PHP_FUNCTION(ubb_group_del) {
	zval *zubb;
	ubb_handle_t *h;
	char *name = NULL;
	int name_len;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zubb, &name, &name_len) == FAILURE) return;
	ZEND_FETCH_RESOURCE(h, ubb_handle_t*, &zubb, -1, le_ubb_resource_name, le_ubb_resource);
	ZEND_VERIFY_RESOURCE(h);
	
	ubb_group_del(h->ubb, name);
	RETURN_TRUE;
}



PHP_FUNCTION(ubb_single_add) {
	zval *zubb;
	ubb_handle_t *h;
	ubb_non_persist_tags_t *p;
	
	char *group;
	int group_len;
	char *name;
	int name_len;
	
	long is_regex;
	long priority;
	
	long fn;
	
	zval *z1 = NULL, *z2 = NULL;
	
	char *callback;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsslll|zz", &zubb, &group, &group_len, &name, &name_len, &is_regex, &priority, &fn, &z1, &z2) == FAILURE) return;
	ZEND_FETCH_RESOURCE(h, ubb_handle_t*, &zubb, -1, le_ubb_resource_name, le_ubb_resource);
	ZEND_VERIFY_RESOURCE(h);
	
	switch(fn) {
	case 0://UBB_CALLBACK(fn, param)
		if(!zend_is_callable(z1, 0, &callback TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is not a callback function");
			efree(callback);
			return;
		}
		efree(callback);
		ubb_single_add(h->ubb, group, name, is_regex, priority, UBB_SINGLE_PHP(z1, z2));
		if(h->persist) {
			p = (ubb_non_persist_tags_t*)malloc(sizeof(ubb_non_persist_tags_t));
			p->name = strdup(name);
			p->type = 1;
			HASH_ADD_KEYPTR(hh, h->non_persist_tags, p->name, name_len, p);
		}
		break;
	case 1://UBB_REPLACE(to)
		ubb_single_add(h->ubb, group, name, is_regex, priority, UBB_SINGLE_REPLACE(Z_STRVAL_P(z1)));
		break;
	case 2://UBB_REPLACE_REGEX(to)
		ubb_single_add(h->ubb, group, name, is_regex, priority, UBB_SINGLE_REPLACE_REGEX(Z_STRVAL_P(z1)));
		break;
	default:
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown or invalid parser function");
		return;
	}
	
	RETURN_TRUE;
}

PHP_FUNCTION(ubb_single_del) {
	zval *zubb;
	ubb_handle_t *h;
	char *from = NULL;
	int from_len;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zubb, &from, &from_len) == FAILURE) return;
	ZEND_FETCH_RESOURCE(h, ubb_handle_t*, &zubb, -1, le_ubb_resource_name, le_ubb_resource);
	ZEND_VERIFY_RESOURCE(h);
	
	ubb_single_del(h->ubb, from);
	RETURN_TRUE;
}



PHP_FUNCTION(ubb_block_add) {
	zval *zubb;
	ubb_handle_t *h;
	ubb_non_persist_tags_t *p;
	
	char *group;
	int group_len;
	char *name;
	int name_len;
	
	char *filter;
	int filter_len;
	
	long fn;
	
	zval *z1 = NULL, *z2 = NULL, *z3 = NULL, *z4 = NULL, *z5 = NULL;
	
	char *callback;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsssl|zzzzz", &zubb, &group, &group_len, &name, &name_len, &filter, &filter_len, &fn, &z1, &z2, &z3, &z4, &z5) == FAILURE) return;
	ZEND_FETCH_RESOURCE(h, ubb_handle_t*, &zubb, -1, le_ubb_resource_name, le_ubb_resource);
	ZEND_VERIFY_RESOURCE(h);
	
	switch(fn) {
	case 0://UBB_CALLBACK(fn, param)
		if(!zend_is_callable(z1, 0, &callback TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is not a callback function");
			efree(callback);
			return;
		}
		efree(callback);
		ubb_block_add(h->ubb, group, name, filter, UBB_BLOCK_PHP(z1, z2));
		if(h->persist) {
			p = (ubb_non_persist_tags_t*)malloc(sizeof(ubb_non_persist_tags_t));
			p->name = strdup(name);
			p->type = 2;
			HASH_ADD_KEYPTR(hh, h->non_persist_tags, p->name, name_len, p);
		}
		break;
	case 3://UBB_SNIPPET(begin, middle, end, default_arg, arg_fallback_to_data)
		ubb_block_add(h->ubb, group, name, filter, UBB_BLOCK_SNIPPET(Z_STRVAL_P(z1), Z_STRVAL_P(z2), Z_STRVAL_P(z3), Z_STRVAL_P(z4), Z_LVAL_P(z5)));
		break;
	case 4://UBB_TEMPLATE(without_arg, with_arg, default_arg, use_data_as_default_arg)
		ubb_block_add(h->ubb, group, name, filter, UBB_BLOCK_TEMPLATE(z1 ? Z_STRVAL_P(z1) : NULL, z2 ? Z_STRVAL_P(z2) : NULL, z3 ? Z_STRVAL_P(z3) : NULL, z4 ? Z_LVAL_P(z4) : 0));
		break;
	default:
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown or invalid parser function");
		return;
	}
}

PHP_FUNCTION(ubb_block_del) {
	zval *zubb;
	ubb_handle_t *h;
	char *name = NULL;
	int name_len;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zubb, &name, &name_len) == FAILURE) return;
	ZEND_FETCH_RESOURCE(h, ubb_handle_t*, &zubb, -1, le_ubb_resource_name, le_ubb_resource);
	ZEND_VERIFY_RESOURCE(h);
	
	ubb_block_del(h->ubb, name);
	RETURN_TRUE;
}


PHP_FUNCTION(ubb_parse) {
	zval *ubb, *gparam = NULL;
	ubb_handle_t *h;
	
	char *data;
	int data_len;
	
	char *out = NULL;
	int out_alloc = 0;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|z", &ubb, &data, &data_len, &gparam) == FAILURE) return;
	ZEND_FETCH_RESOURCE(h, ubb_handle_t*, &ubb, -1, le_ubb_resource_name, le_ubb_resource);
	ZEND_VERIFY_RESOURCE(h);
	
	UBB_MALLOC_TRASH(out, out_alloc, data_len);
	
	memcpy(out, data, data_len + 1);
	ubb_parse(h->ubb, &out, data_len, &out_alloc, (void*)gparam);
	
	ZVAL_STRING(return_value, out, 1);
	free(out);
}
