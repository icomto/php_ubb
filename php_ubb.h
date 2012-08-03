#ifndef PHP_UBB_H
#define PHP_UBB_H

#ifdef ZTS
#include "TSRM.h"
#endif

#define PHP_UBB_VERSION "1.0b"
#define PHP_UBB_EXTNAME "ubb"

PHP_MINIT_FUNCTION(ubb);
PHP_MSHUTDOWN_FUNCTION(ubb);

PHP_RSHUTDOWN_FUNCTION(ubb);

PHP_FUNCTION(ubb_init);
PHP_FUNCTION(ubb_free);

PHP_FUNCTION(ubb_pinit);
PHP_FUNCTION(ubb_pfree);

PHP_FUNCTION(ubb_filter_rebuild);

PHP_FUNCTION(ubb_group_del);

PHP_FUNCTION(ubb_block_add);
PHP_FUNCTION(ubb_block_del);

PHP_FUNCTION(ubb_single_add);
PHP_FUNCTION(ubb_single_del);

PHP_FUNCTION(ubb_parse);

extern zend_module_entry ubb_module_entry;
#define phpext_ubb_ptr &ubb_module_entry

#endif
