AC_INIT(QShield, 0.1, cyx.xjtu@gmail.com)

AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AM_PROG_CC_C_O

SGX_INIT()

SGX_TSTDC_CHECK_HEADERS([stdlib.h string.h sys/time.h])
SGX_TSTDC_CHECK_FUNCS([floor gettimeofday memmove memset pow sqrt strchr strdup strcpy rand])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile enc-src/Makefile])

AC_OUTPUT()
