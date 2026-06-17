# phathom/ext

This extension re-implements the internal API of `pharos\phathom\Earley`, mirroring the userland implementation but removing the cost of
object allocation (in this namespace).

Specifically, `pharos\phathom\Earley\Chart` and `pharos\phathom\Earley\Evaluator` are implemented, and will be used transparently anywhere the extension is loaded.

*This extension is intended to be a faithful mirror; A competent `PHP` programmer ought to be able to navigate this `C` code if they are familiar with the `PHP` implementation and they like to fix or add a thing.*

## Install

To configure the extension for normal installation

`./configure`

Run a normal build:

`make`

Install:

`make install`

## Testing

To configure the extension with coverage:

`./configure --with-phathom-coverage`

Run a normal build:

`make`

To run coverage collection with lcov over phpunit tests:

`TEST_PHP_EXECUTABLE=/usr/bin/php make phathom-test-coverage-html`

Will generate a `coverage` directory with html and `coverage.info` with data.

*Note: the coverage recipe from Makefile.frag does not require installation of the extension*

### Notes

It's important that the extension and PHP code are kept in sync; The extension treats the layout of PHP objects as stable ABI (ie, they must not change order).

**!WIP!**