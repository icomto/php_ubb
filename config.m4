PHP_ARG_ENABLE(ubb, whether to enable UBB support,
[ --enable-ubb   Enable UBB support])

if test "$PHP_UBB" = "yes"; then
  AC_DEFINE(HAVE_UBB, 1, [Whether you have UBB])
  PHP_NEW_EXTENSION(ubb, php_ubb.c, $ext_shared)
fi
