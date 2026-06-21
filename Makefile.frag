phathom-test-coverage-phpunit:
	ZEND_DONT_UNLOAD_MODULES=1 $(TEST_PHP_EXECUTABLE) \
		-dextension=$(top_builddir)/modules/phathom.so \
	../vendor/bin/phpunit -c ../phpunit.xml $(PHPUNIT_ARGS)

phathom-test-coverage-lcov: phathom-test-coverage-phpunit
	lcov -c \
		--directory \
			$(top_srcdir)/src/earley/.libs \
		--directory \
			$(top_srcdir)/src/glr/.libs \
		--output-file \
			$(top_srcdir)/coverage.info
	lcov \
		--extract $(top_srcdir)/coverage.info \
			'$(top_srcdir)/*' \
			'$(top_srcdir)/src/*' \
		--output-file \
			$(top_srcdir)/coverage.info \
		--ignore-errors \
			unused,unused

phathom-test-coverage-html: phathom-test-coverage-lcov
	genhtml $(top_srcdir)/coverage.info --output-directory=$(top_srcdir)/coverage
