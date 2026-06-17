#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CFLAGS="-std=c11 -D_GNU_SOURCE -DNDEBUG"
INCLUDES="$(/usr/bin/php-config --includes) -Iext -Iext/src"
SRC="ext/smoke/hash_stress.c"
BIN="/tmp/hash_stress_cov"
GCNO="/tmp/hash_stress_cov-hash_stress.gcno"
GCDA="/tmp/hash_stress_cov-hash_stress.gcda"
HTML_DIR="$ROOT/ext/smoke/coverage-html"
HTML_INDEX="$HTML_DIR/index.html"

rm -f "$GCNO" "$GCDA" hash_stress.c.gcov hash.h.gcov zend_arena.h.gcov zend_string.h.gcov zend_types.h.gcov

cc $CFLAGS -O0 -g --coverage $INCLUDES "$SRC" -o "$BIN"
"$BIN" 300000 5000000

gcov -b -c "$SRC" -o "$GCNO"

echo "--- smoke coverage summary (boilerplate excluded) ---"
awk '
BEGIN {
	in_excl = 0;
}
FNR == NR {
	if ($0 ~ /GCOV_EXCL_START/) {
		in_excl = 1;
	}
	if (in_excl) {
		excluded[FNR] = 1;
	}
	if ($0 ~ /GCOV_EXCL_STOP/) {
		in_excl = 0;
	}
	next;
}
{
	split($0, cols, ":");
	if (length(cols) < 3) {
		next;
	}

	count = cols[1];
	line = cols[2] + 0;

	gsub(/^[ \t]+|[ \t]+$/, "", count);

	if (line <= 0 || excluded[line]) {
		next;
	}

	if (count == "-" || count == "=====") {
		next;
	}

	executable++;

	if (count != "#####" && count !~ /^=====/) {
		executed++;
	}
}
END {
	pct = (executable > 0) ? (100.0 * executed / executable) : 0;
	printf("smoke source lines executed (filtered): %.2f%% (%d/%d)\n", pct, executed, executable);
}
' "$SRC" hash_stress.c.gcov

echo "--- hash.h coverage summary ---"
rg -n "File 'ext/src/hash.h'|Lines executed|Branches executed|Taken at least once|Calls executed" hash.h.gcov || true

echo "--- hash core branch coverage (filtered) ---"
awk '
BEGIN {
	in_cf = 0;
	total = 0;
	hit = 0;
}

/^[^:]+:[[:space:]]*[0-9]+:/ {
	split($0, cols, ":");
	src = cols[3];

	gsub(/^[ \t]+|[ \t]+$/, "", src);

	in_cf = (src ~ /(if|for|while)[[:space:]]*\(/) &&
			(src !~ /ZEND_ASSERT/) &&
			(src !~ /EXPECTED\(/) &&
			(src !~ /UNEXPECTED\(/);
	next;
}

/^[[:space:]]*branch[[:space:]]+[0-9]+[[:space:]]+taken/ {
	if (!in_cf) {
		next;
	}

	total++;
	if ($0 !~ /taken 0/ && $0 !~ /never executed/) {
		hit++;
	}
	next;
}

END {
	pct = (total > 0) ? (100.0 * hit / total) : 0;
	printf("hash.h control-flow branches hit: %.2f%% (%d/%d)\n", pct, hit, total);
}
' hash.h.gcov

echo "--- uncovered branches (sample) ---"
rg -n "branch .*taken 0|branch .*never executed" hash.h.gcov | head -n 40 || true

echo "--- generating html report ---"
mkdir -p "$HTML_DIR"

if command -v gcovr >/dev/null 2>&1; then
	gcovr \
		--root "$ROOT" \
		--object-directory /tmp \
		--filter "$ROOT/ext/src/hash.h" \
		--filter "$ROOT/ext/smoke/hash_stress.c" \
		--exclude-directories ".*/ext/src/\\.libs" \
		--exclude-unreachable-branches \
		--exclude-throw-branches \
		--gcov-ignore-errors no_working_dir_found \
		--gcov-ignore-errors source_not_found \
		--exclude-lines-by-pattern "GCOV_EXCL_(START|STOP)" \
		--html-details "$HTML_INDEX" \
		--html-title "phathom hash smoke coverage" \
		--print-summary
elif command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then
	INFO_FILE="$HTML_DIR/hash-coverage.info"
	lcov --capture --directory /tmp --base-directory "$ROOT" --output-file "$INFO_FILE"
	genhtml "$INFO_FILE" --output-directory "$HTML_DIR" --title "phathom hash smoke coverage"
else
	echo "No html coverage tool found (need gcovr or lcov+genhtml)." >&2
	exit 1
fi

echo "html report: $HTML_INDEX"
