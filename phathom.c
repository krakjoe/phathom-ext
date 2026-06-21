/*
  +----------------------------------------------------------------------+
  | phathom                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2026                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_enum.h"

#include "phathom.h"

#include "src/earley/chart.h"
#include "src/earley/evaluator.h"
#include "src/glr/chart.h"
#include "src/glr/evaluator.h"

ZEND_TLS bool php_phathom_ready = false;

/* {{{ words */
#define php_phathom_word(word)                 php_phathom_##word##_word
#define php_phathom_word_decl(word)            static zend_string* php_phathom_word(word) 
/* }}} */

/* {{{ classes */
#define php_phathom_class(class)               class##_ce
#define php_phathom_classn(class)              class##_class
#define php_phathom_classnc(class)             class##_chars
#define php_phathom_classnl(class)             class##_length

#define php_phathom_class_decl(class, name) \
	ZEND_TLS zend_class_entry* php_phathom_class(class);   \
	static zend_string*                                    \
		php_phathom_classn(class);                         \
	static const char* php_phathom_classnc(class) = name;  \
	static size_t php_phathom_classnl(class) =             \
		sizeof(name) - 1;                                  \
/* }}} */

/* {{{ methods */
#define php_phathom_method(class, method) class##_##method##_fe
#define php_phathom_method_decl(class, method)              \
	ZEND_TLS zend_function* php_phathom_method(class, method)
/* }}} */

/* {{{ enums */
#define php_phathom_enum(class, case) class##_##case##_co
#define php_phathom_enum_decl(class, case)            \
	ZEND_TLS zend_object* php_phathom_enum(class, case)
/* }}} */

/* {{{ decls */
php_phathom_class_decl(
	php_phathom_grammar,
	"\\pharos\\phathom\\Grammar");
php_phathom_class_decl(
	php_phathom_buffer,
	"\\pharos\\phathom\\Interface\\Buffer");
php_phathom_class_decl(
	php_phathom_context,
	"\\pharos\\phathom\\Context");
php_phathom_class_decl(
	php_phathom_grammar_quantifier,
	"\\pharos\\phathom\\Grammar\\Quantifier");
php_phathom_class_decl(
	php_phathom_grammar_associativity,
	"\\pharos\\phathom\\Grammar\\Associativity");
php_phathom_class_decl(
	php_phathom_exception_ambiguity,
	"\\pharos\\phathom\\Exception\\Ambiguity");
php_phathom_method_decl(
	php_phathom_exception_ambiguity, range);

php_phathom_class_decl(
	php_phathom_exception_execute,
	"\\pharos\\phathom\\Exception\\Execute");
php_phathom_method_decl(
	php_phathom_exception_execute, nomatch);

php_phathom_enum_decl(php_phathom_grammar_quantifier, NONE);
php_phathom_enum_decl(php_phathom_grammar_quantifier, STAR);
php_phathom_enum_decl(php_phathom_grammar_quantifier, PLUS);
php_phathom_enum_decl(php_phathom_grammar_quantifier, OPTIONAL);

php_phathom_enum_decl(php_phathom_grammar_associativity, NONE);
php_phathom_enum_decl(php_phathom_grammar_associativity, LEFT);
php_phathom_enum_decl(php_phathom_grammar_associativity, RIGHT);

php_phathom_word_decl(scan);
php_phathom_word_decl(contents);
php_phathom_word_decl(range);
php_phathom_word_decl(nomatch);

#undef php_phathom_method_decl
#undef php_phathom_class_decl
#undef php_phathom_enum_decl
#undef php_phathom_word_decl
/* }}} */

/* {{{ fetch */
#define php_phathom_fetch_class(symbol) do { \
	php_phathom_class(symbol) =              \
		zend_lookup_class(symbol##_class);   \
	ZEND_ASSERT(php_phathom_class(symbol));	 \
} while(0)

#define php_phathom_fetch_method(symbol, method) do {   \
	php_phathom_method(symbol, method) =                \
		zend_hash_find_ptr(                             \
			&php_phathom_class(symbol)->function_table, \
				php_phathom_word(method));              \
	ZEND_ASSERT(php_phathom_method(symbol, method));    \
} while(0)

#define php_phathom_fetch_enum(symbol, case) do { \
	php_phathom_enum(symbol, case) = \
		zend_enum_get_case_cstr(\
			php_phathom_class(symbol), #case); \
} while(0)

php_phathom_t php_phathom_fetch(void) {
    if (!php_phathom_ready) {
		php_phathom_fetch_class(php_phathom_grammar);
		php_phathom_fetch_class(php_phathom_buffer);
		php_phathom_fetch_class(php_phathom_context);
		php_phathom_fetch_class(php_phathom_grammar_quantifier);
		php_phathom_fetch_class(php_phathom_grammar_associativity);
		php_phathom_fetch_class(php_phathom_exception_ambiguity);
		php_phathom_fetch_class(php_phathom_exception_execute);

		php_phathom_fetch_method(php_phathom_exception_ambiguity, range);
		php_phathom_fetch_method(php_phathom_exception_execute, nomatch);

		php_phathom_fetch_enum(php_phathom_grammar_quantifier, NONE);
		php_phathom_fetch_enum(php_phathom_grammar_quantifier, PLUS);
		php_phathom_fetch_enum(php_phathom_grammar_quantifier, STAR);
		php_phathom_fetch_enum(php_phathom_grammar_quantifier, OPTIONAL);

		php_phathom_fetch_enum(php_phathom_grammar_associativity, NONE);
		php_phathom_fetch_enum(php_phathom_grammar_associativity, LEFT);
		php_phathom_fetch_enum(php_phathom_grammar_associativity, RIGHT);

		php_phathom_ready = true;
    }

	return (php_phathom_t) {
		.class.grammar    = php_phathom_class(php_phathom_grammar),
		.class.buffer     = php_phathom_class(php_phathom_buffer),
		.class.context    = php_phathom_class(php_phathom_context),
		.class.quantifier = php_phathom_class(php_phathom_grammar_quantifier),
		.class.exception  = {
			.ambiguity    = php_phathom_class(php_phathom_exception_ambiguity),
			.execute      = php_phathom_class(php_phathom_exception_execute),
		},
		.enumerated       = {
			.quantifier   = {
				.none     = php_phathom_enum(php_phathom_grammar_quantifier, NONE),
				.plus     = php_phathom_enum(php_phathom_grammar_quantifier, PLUS),
				.star     = php_phathom_enum(php_phathom_grammar_quantifier, STAR),
				.optional = php_phathom_enum(php_phathom_grammar_quantifier, OPTIONAL),
			},
			.associativity = {
				.none  = php_phathom_enum(php_phathom_grammar_associativity, NONE),
				.left  = php_phathom_enum(php_phathom_grammar_associativity, LEFT),
				.right = php_phathom_enum(php_phathom_grammar_associativity, RIGHT),
			},
		},
		.exception       = {
			.ambiguity    = {
				.range    = php_phathom_method(php_phathom_exception_ambiguity, range),
			},
			.execute      = {
				.nomatch  = php_phathom_method(php_phathom_exception_execute, nomatch)
			}
		},
		.word.scan        = php_phathom_word(scan),
		.word.contents    = php_phathom_word(contents),
	};
}

#undef php_phathom_fetch_class
#undef php_phathom_fetch_method
#undef php_phathom_fetch_enum
/* }}} */

/* {{{ MINIT */
#define php_phathom_class_minit(class)         \
	php_phathom_classn(class) =                \
		zend_string_init_interned(             \
			php_phathom_classnc(class),        \
			php_phathom_classnl(class), true); \
	ZEND_ASSERT(php_phathom_classn(class));    \

#define php_phathom_word_minit(word) \
	php_phathom_word(word) = \
			zend_string_init_interned(\
				ZEND_STRL(#word), true);

static PHP_MINIT_FUNCTION(phathom)
{
	php_phathom_class_minit(php_phathom_grammar);
	php_phathom_class_minit(php_phathom_buffer);
	php_phathom_class_minit(php_phathom_context);

	php_phathom_class_minit(php_phathom_grammar_quantifier);
	php_phathom_class_minit(php_phathom_grammar_associativity);
	php_phathom_class_minit(php_phathom_exception_ambiguity);
	php_phathom_class_minit(php_phathom_exception_execute);
	
	php_phathom_word_minit(scan);
	php_phathom_word_minit(contents);
	php_phathom_word_minit(range);
	php_phathom_word_minit(nomatch);

	PHP_MINIT(PHATHOM_EARLEY_CHART)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MINIT(PHATHOM_EARLEY_EVALUATOR)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PHATHOM_GLR_CHART)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(PHATHOM_GLR_EVALUATOR)(INIT_FUNC_ARGS_PASSTHRU);
}

#undef php_phathom_class_minit
#undef php_phathom_word_minit
/* }}} */

/* {{{ MSHUTDOWN */
#define php_phathom_class_mshutdown(class)  \
	zend_string_release(                    \
		php_phathom_classn(class))

#define php_phathom_word_mshutdown(word) \
		zend_string_release(             \
			php_phathom_word(word));

static PHP_MSHUTDOWN_FUNCTION(phathom)
{
	PHP_MSHUTDOWN(PHATHOM_EARLEY_CHART)(INIT_FUNC_ARGS_PASSTHRU);
	PHP_MSHUTDOWN(PHATHOM_EARLEY_EVALUATOR)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PHATHOM_GLR_CHART)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MSHUTDOWN(PHATHOM_GLR_EVALUATOR)(INIT_FUNC_ARGS_PASSTHRU);
	php_phathom_class_mshutdown(php_phathom_buffer);
	php_phathom_class_mshutdown(php_phathom_context);

	php_phathom_class_mshutdown(php_phathom_grammar_quantifier);
	php_phathom_class_mshutdown(php_phathom_grammar_associativity);
	php_phathom_class_mshutdown(php_phathom_exception_ambiguity);
	php_phathom_class_mshutdown(php_phathom_exception_execute);

	php_phathom_word_mshutdown(scan);
	php_phathom_word_mshutdown(contents);
	php_phathom_word_mshutdown(range);
	php_phathom_word_mshutdown(nomatch);

	return SUCCESS;
}

#undef php_phathom_class_mshutdown
#undef php_phathom_word_mshutdown
/* }}} */

static PHP_RINIT_FUNCTION(phathom)
{
#ifdef ZTS
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
} /* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
static PHP_RSHUTDOWN_FUNCTION(phathom)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
static PHP_MINFO_FUNCTION(phathom)
{
	php_info_print_table_start();
	php_info_print_table_header(2,
		"phathom support", PHP_PHATHOM_VERSION);
	php_info_print_table_end();

 	DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ phathom_module_entry
 */
zend_module_entry phathom_module_entry = {
	STANDARD_MODULE_HEADER,
	PHP_PHATHOM_EXTNAME,
	NULL,
	PHP_MINIT(phathom),
	PHP_MSHUTDOWN(phathom),
	PHP_RINIT(phathom),
	PHP_RSHUTDOWN(phathom),
	PHP_MINFO(phathom),
	PHP_PHATHOM_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PHATHOM
ZEND_GET_MODULE(phathom)
#ifdef ZTS
	ZEND_TSRMLS_CACHE_DEFINE();
#endif
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
