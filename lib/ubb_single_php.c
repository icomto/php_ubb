#define UBB_SINGLE_PHP(fn, param) \
	ubb_single_php_fn, ubb_single_php_new(fn, param), ubb_single_php_free

void * ubb_single_php_new(zval *fn, zval *param) {
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

void ubb_single_php_free(void *ptr) {
	zval **zv = (zval**)ptr;
	zval_dtor(zv[0]);
	zval_dtor(zv[1]);
	free(zv[0]);
	free(zv[1]);
	free(zv);
}

int ubb_single_php_fn(ubb_t *ubb, char *data, int data_len, int *matches, int nmatches, char **out, int *out_alloc, void *param, void *gparam) {
	int rv;
	zval *fn = (zval*)((void**)param)[0];
	zval *fn_param = (zval*)((void**)param)[1];
	zval *retval;
	zval **args[5];
	zval *zdata, *zdata_len, *zmatches, *znmatches, *zparam;
	
	MAKE_STD_ZVAL(zdata);
	MAKE_STD_ZVAL(zdata_len);
	MAKE_STD_ZVAL(zmatches);
	MAKE_STD_ZVAL(znmatches);
	MAKE_STD_ZVAL(zparam);
	
	ZVAL_STRING(zdata, data, 1);
	ZVAL_LONG(zdata_len, data_len);
	ZVAL_NULL(zmatches);//////////TODO
	ZVAL_LONG(znmatches, nmatches);
	*zparam = *fn_param;
	zval_copy_ctor(zparam);
	
	args[0] = &zdata;
	args[1] = &zdata_len;
	args[2] = &zmatches;
	args[3] = &znmatches;
	args[4] = &zparam;
	args[5] = (zval**)&gparam;
	
	rv = call_user_function_ex(CG(function_table), NULL, fn, &retval, 6, args TSRMLS_CC, 0, NULL);
	
	zval_dtor(zdata);
	zval_dtor(zdata_len);
	zval_dtor(zmatches);
	zval_dtor(znmatches);
	zval_dtor(zparam);
	
	FREE_ZVAL(zdata);
	FREE_ZVAL(zdata_len);
	FREE_ZVAL(zmatches);
	FREE_ZVAL(znmatches);
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
