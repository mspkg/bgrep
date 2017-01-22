#!/bin/bash

BGREP=../src/bgrep

function return_code_test() {
	local name="$1"
	local input="$2"
	local pattern="$3"
	local expected="$4"

	echo "${input}" | ${BGREP} -q -x "${pattern}" 2>/dev/null
	local result="$?"
	if [[ "${result}" -ne "${expected}" ]] ; then
		echo "${name}: Test FAILED."
		echo "Pattern ${pattern} should have returned code ${expected} (was ${result})"
		return 1
	fi
	return 0
}

function test_imbalanced_parens() {
	local input=''
	local pattern='1234(5678)(1234(5678)'
	local expected="2"
	return_code_test "${FUNCNAME[0]}" "${input}" "${pattern}" "${expected}"
}

function test_imbalanced_quotes() {
	local input=''
	local pattern='1234(5678)"1234(5678)'
	local expected="2"
	return_code_test "${FUNCNAME[0]}" "${input}" "${pattern}" "${expected}"
}

function test_empty_pattern() {
	local input=''
	local pattern=''
	local expected="2"
	return_code_test "${FUNCNAME[0]}" "${input}" "${pattern}" "${expected}"
}

function test_bad_hex1() {
	local input=''
	local pattern='12 34 1z'
	local expected="2"
	return_code_test "${FUNCNAME[0]}" "${input}" "${pattern}" "${expected}"
}

function test_bad_hex2() {
	local input=''
	local pattern='12 34 1'
	local expected="2"
	return_code_test "${FUNCNAME[0]}" "${input}" "${pattern}" "${expected}"
}

function test_bad_hex3() {
	local input=''
	local pattern='12 34 \11'
	local expected="2"
	return_code_test "${FUNCNAME[0]}" "${input}" "${pattern}" "${expected}"
}

function test_bad_repeat1() {
	local input=''
	local pattern='12*'
	local expected="2"
	return_code_test "${FUNCNAME[0]}" "${input}" "${pattern}" "${expected}"
}

failcount=0

test_imbalanced_parens || failcount=$((failcount+1))
test_imbalanced_quotes || failcount=$((failcount+1))

test_bad_hex1 || failcount=$((failcount+1))
test_bad_hex2 || failcount=$((failcount+1))
test_bad_hex3 || failcount=$((failcount+1))

test_bad_repeat1 || failcount=$((failcount+1))

if [[ ${failcount} -eq 0 ]] ; then
	echo ALL TESTS PASSED.
else
	echo ${failcount} tests failed >&2
	exit 1
fi
