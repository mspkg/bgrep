#!/bin/bash

BGREP=../src/bgrep
XXD=${XXD:-xxd}

function XXDFUN() {
	${XXD} "$@"
}

if (echo | ${XXD} | grep -q "^........:") ; then
	echo XXD has 8-digit offsets. Adjusting.
	function XXDFUN() {
		${XXD} "$@" | sed -e 's/^.\(.......:\)/\1/g'
	}
fi

# Test xxd output against a known expected output string.
# expected should be:
#    0000004: 666f 6f                                  foo
#    000000b: 666f 6f                                  foo
# EOF
function test_xxd_output() {
	input="1234foo89abfoof0123"
	expected="$(echo ${input} | XXDFUN -s 4 -l 3 ; echo ${input} | XXDFUN -s 11 -l 3)"
	actual="$(echo -n "${input}" | ${BGREP} \"foo\")"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---\n${expected}"
		echo -e "+++ Actual +++\n${actual}"
		return 1
	fi
}

# bgrep for a single wildcard character matches everything.  Output should be identical to streaming through "xxd" with default ("normal") settings.
function test_bgrep_matches_xxd() {
	# Run some random data through xxd
	expected="$(dd if=/dev/urandom bs=1 count=1k status=none | XXDFUN)"
	actual="$(echo "${expected}" | XXDFUN -r | ${BGREP} '??')"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---\n${expected}"
		echo -e "+++ Actual +++\n${actual}"
		return 1
	fi
}


# bgrep for "foo" should be compatible with "xxd -r" as long as we don't mess up the column matching
# Note that "xxd -r" pads with null characters when there are gaps in the data.
function test_partial_match_xxd_revert() {
	teststring="1234foo89abfoof0123"
	expected="$(echo -en '\x00\x00\x00\x00foo\x00\x00\x00\x00foo' | XXDFUN)"
	actual="$(echo ${teststring} | ${BGREP} \"foo\" | XXDFUN -c 3 -r | XXDFUN)"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---\n${expected}"
		echo -e "+++ Actual +++\n${actual}"
		return 1
	fi
}

function test_offset() {
	teststring="1234foo89abfoof0123"
	expected=$'00000004\n0000000b'
	actual="$(echo -e "${teststring}" | ${BGREP} -b \"foo\")"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_count() {
	teststring="1234foo89abfoof0123"
	expected="2"
	actual="$(echo -e "${teststring}" | ${BGREP} -c \"foo\")"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_find_first() {
	teststring="1234foo89abfoof0123"
	expected="00000004"
	actual="$(echo -e "${teststring}" | ${BGREP} -Fb \"foo\")"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_overlap() {
	teststring="oofoofoofoo"
	expected=$'stdin:00000002\nstdin:00000005'
	actual="$(echo -e "${teststring}" | ${BGREP} -Hb \"foof\")"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_wildcard() {
	teststring="oof11f22foo"
	expected="0000002: 6631 3166 3232 66                        f11f22f"
	actual="$(echo -e "${teststring}" | ${BGREP} 66????66)"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_skip() {
	teststring="oof11f22foo"
	expected="0000005: 6632 3266                                f22f"
	actual="$(echo -e "${teststring}" | ${BGREP} -s 3 '66????66')"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_dd_skip() {
	teststring="$((echo foo; dd if=/dev/urandom bs=1 count=2k status=none; echo foofoo; dd if=/dev/urandom bs=1 count=1k status=none) | XXDFUN)"
	expected="0000804: 666f 6f66 6f6f                           foofoo"
	actual="$(echo "${teststring}" | XXDFUN -r | ${BGREP} -s 1k \"foo\" )"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_bytes_before() {
	input="1234foo89abfoof0123"
	expected="$(echo ${input} | XXDFUN -s 1 -l 6)"
	actual="$(echo -n "${input}" | ${BGREP} -FB 3 \"foo\")"

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_bytes_after() {
	# "bytes after" requires the ability to seek in the file.
	# You can't seek on stdin, so we have to make a file
	(dd if=/dev/urandom count=2 status=none ; echo "1234foo89abfoof0123" ) > tst.bin

	expected=$'0000404: 666f 6f38 3961                           foo89a\n000040b: 666f 6f66 3031                           foof01'
	actual="$(${BGREP} -A 3 \"foo\" tst.bin)"
	rm tst.bin

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

function test_bytes_around() {
	# The -C flag requires the ability to seek in the file, just like -B
	# You can't seek on stdin, so we have to make a file
	(dd if=/dev/urandom count=2 status=none ; echo "1234foo89abfoof0123" ) > tst.bin

	expected="0000401: 3233 3466 6f6f 3839 6162 666f 6f66 3031  234foo89abfoof01"
	actual="$(${BGREP} -C 3 \"foo\" tst.bin)"
	rm tst.bin

	if [[ "${expected}" != "${actual}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo -e "--- Expected ---"
		echo "${expected}" | XXDFUN
		echo -e "+++ Actual +++"
		echo "${actual}" | XXDFUN
		return 1
	fi
}

failcount=0

test_xxd_output || failcount=$((failcount+1))
test_bgrep_matches_xxd || failcount=$((failcount+1))
test_partial_match_xxd_revert || failcount=$((failcount+1))
test_offset || failcount=$((failcount+1))
test_count || failcount=$((failcount+1))
test_find_first || failcount=$((failcount+1))
test_overlap || failcount=$((failcount+1))
test_wildcard || failcount=$((failcount+1))
test_skip || failcount=$((failcount+1))
test_dd_skip || failcount=$((failcount+1))
test_bytes_before || failcount=$((failcount+1))
test_bytes_after || failcount=$((failcount+1))
test_bytes_around || failcount=$((failcount+1))

if [[ ${failcount} -eq 0 ]] ; then
	echo ALL TESTS PASSED.
else
	echo ${failcount} tests failed >&2
	exit 1
fi
