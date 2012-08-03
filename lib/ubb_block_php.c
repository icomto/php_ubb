#define UBB_BLOCK_PHP(fn, param) \
	ubb_block_php_fn, ubb_block_php_new(fn, param), ubb_block_php_free

void * ubb_block_php_new(zval *fn, zval *param) {
	zval **zv;
	
	zv = (zval**)malloc(sizeof(zval*)*2);
	
	zv[0] = (zval*)malloc(sizeof(zval));
	*zv[0] = *fn;
	zval_copy_ctor(zv[0]);
	
	zv[1] = (zval*)malloc(sizeof(zval));
	if(param) {
		*zv[1] = *param;
		zval_copy_ctor(zv[1]);
	}
	else {
		INIT_ZVAL((*zv[1]));
		ZVAL_NULL(zv[1]);
	}
	
	return (void*)zv;
}

void ubb_block_php_free(void *ptr) {
	zval **zv = (zval**)ptr;
	zval_dtor(zv[0]);
	zval_dtor(zv[1]);
	free(zv[0]);
	free(zv[1]);
	free(zv);
}

int ubb_block_php_fn(ubb_t *ubb, char *data, int data_len, char *arg, int arg_len, char **out, int *out_alloc, void *param, void *gparam) {
	int rv;
	zval *fn = (zval*)((void**)param)[0];
	zval *fn_param = (zval*)((void**)param)[1];
	zval *retval;
	zval **args[6];
	zval *zdata, *zdata_len, *zarg, *zarg_len, *zparam;
	char *callback;
	
	MAKE_STD_ZVAL(zdata);
	MAKE_STD_ZVAL(zdata_len);
	MAKE_STD_ZVAL(zarg);
	MAKE_STD_ZVAL(zarg_len);
	MAKE_STD_ZVAL(zparam);
	
	ZVAL_STRING(zdata, data, 1);
	ZVAL_LONG(zdata_len, data_len);
	ZVAL_STRING(zarg, arg_len ? arg : "", 1);
	ZVAL_LONG(zarg_len, arg_len);
	*zparam = *fn_param;
	zval_copy_ctor(zparam);
	
	args[0] = &zdata;
	args[1] = &zdata_len;
	args[2] = &zarg;
	args[3] = &zarg_len;
	args[4] = &zparam;
	args[5] = (zval**)&gparam;
	
	php_printf("CALLGIN FUNCTION PTR %p <<<<<<<<<<<<<<<<<<<<<<<<\n", fn);
	if(!zend_is_callable(fn, 0, &callback TSRMLS_CC)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is not a callback function");
		efree(callback);
		return;
	}
	php_printf("CALLGIN FUNCTION %s <<<<<<<<<<<<<<<<<<<<<<<<\n", callback);
	efree(callback);
	
	rv = call_user_function_ex(CG(function_table), NULL, fn, &retval, 6, args TSRMLS_CC, 0, NULL);
	
	zval_dtor(zdata);
	zval_dtor(zdata_len);
	zval_dtor(zarg);
	zval_dtor(zarg_len);
	zval_dtor(zparam);
	
	FREE_ZVAL(zdata);
	FREE_ZVAL(zdata_len);
	FREE_ZVAL(zarg);
	FREE_ZVAL(zarg_len);
	FREE_ZVAL(zparam);
	
	if(rv != SUCCESS) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Call of user function failed");
		return -1;
	}
	
	if(Z_LVAL_P(retval)) *out = Z_STRVAL_P(retval);//memcpy(out, Z_STRVAL_P(retval), Z_STRLEN_P(retval));
	zval_dtor(retval);
	FREE_ZVAL(retval);
	return Z_STRLEN_P(retval);
}
