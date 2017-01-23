#!/bin/bash

BGREP=../src/bgrep

function return_code_test() {
	local input="$1"
	local pattern="$2"
	local expected="$3"

	echo "${input}" | ${BGREP} -q -x "${pattern}" 2>/dev/null
	local result="$?"
	if [[ "${result}" -ne "${expected}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo "Pattern ${pattern} should have returned code ${expected} (was ${result})"
		return 1
	fi
	return 0
}

declare -a bad_patterns
bad_patterns=(
	''
	'('
	')'
	'"'
	'*'
	'1'
	'k'
	'z'
	'()'
	'""'
	'\11'
	'(12'
	'12)'
	'"12'
	'12"'
	'1x'
	'12*'
	'(12")"'
	'12 34 1'
	'12 34 1z'
	'12 34 \11'
	'12* "3"'
	'12* 4z'
	'12*(11)'
	'*2'
	'11*0'
	'1234(5678)(1234(5678)'
	'1234(5678)"1234(5678)'
	'1234(5678)"12\"34(5678)'
)

failcount=0

for p in "${bad_patterns[@]}"
do
	return_code_test "" "${p}" 2 || failcount=$((failcount+1))
done

if [[ ${failcount} -eq 0 ]] ; then
	echo ALL TESTS PASSED.
else
	echo ${failcount} tests failed >&2
	exit 1
fi
