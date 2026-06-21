PHP_ARG_ENABLE([phathom],
  [whether to enable phathom support],
  [AS_HELP_STRING([--enable-phathom],
    [Enable phathom support])])

PHP_ARG_WITH([phathom-coverage],
  [whether to enable phathom coverage support],
  [AS_HELP_STRING([--with-phathom-coverage],
    [Build phathom with gcov coverage support])],
  [no],
  [no])

PHP_ARG_WITH([phathom-sanitize],
  [whether to enable AddressSanitizer for phathom],
  [AS_HELP_STRING([--with-phathom-sanitize],
    [Build phathom with AddressSanitizer support])],
  [no],
  [no])

if test "$PHP_PHATHOM" != "no"; then
  AS_VAR_IF([PHP_PHATHOM_SANITIZE], [no],, [
    EXTRA_LDFLAGS="-lasan"
    EXTRA_CFLAGS="-fsanitize=address -fno-omit-frame-pointer"
    PHP_SUBST([EXTRA_LDFLAGS])
    PHP_SUBST([EXTRA_CFLAGS])
  ])

  AS_VAR_IF([PHP_PHATHOM_COVERAGE], [no],, [
    EXTRA_LDFLAGS="$EXTRA_LDFLAGS -fprofile-arcs"
    case $host_os in
      darwin*)
        ;;
      *)
        EXTRA_LDFLAGS="$EXTRA_LDFLAGS -lgcov"
        ;;
    esac

    EXTRA_CFLAGS="$EXTRA_CFLAGS -fprofile-arcs -ftest-coverage"
    PHP_SUBST([EXTRA_LDFLAGS])
    PHP_SUBST([EXTRA_CFLAGS])
  ])

  PHP_NEW_EXTENSION([phathom], m4_normalize([
      src/earley/chart.c
      src/earley/evaluator.c
      src/glr/chart.c
      src/glr/evaluator.c
      phathom.c
    ]),
    [$ext_shared],,
    [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1])
  PHP_ADD_BUILD_DIR([$ext_builddir/src])
  PHP_ADD_BUILD_DIR([$ext_builddir/src/earley])
  PHP_ADD_BUILD_DIR([$ext_builddir/src/glr])

  PHP_ADD_INCLUDE([$ext_builddir])
  PHP_ADD_INCLUDE([$ext_builddir/src])

  AS_VAR_IF([PHP_PHATHOM_COVERAGE], [no],, [PHP_ADD_MAKEFILE_FRAGMENT])
fi
