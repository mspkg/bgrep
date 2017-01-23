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


function pattern_test() {
	local pattern="$1"
	local expected="$2"
	local result=$(${BGREP} -x "${pattern}" --bgrep-dump-pattern | xxd -p -c 131072)
	if [[ "${result}" != "${expected}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo "Pattern: ${pattern}"
		echo "---Expected---"
		echo "${expected}"
		echo "+++Actual---"
		echo "${result}"
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

declare -A good_patterns=(
	['??']="0000"
	['??*2']="00000000"
	['01']="01ff"
	['0101']="0101ffff"
	['01 01']="0101ffff"
	['01*2']="0101ffff"
	['01  *2']="0101ffff"
	['01* 2']="0101ffff"
	['"foo"']="666f6fffffff"
	['"foo"*2']="666f6f666f6fffffffffffff"
	['("foo"??)*w']="666f6f00666f6f00ffffff00ffffff00"
	['12*3 44']="12121244ffffffff"
	['((12 ??)*2)*2']="1200120012001200ff00ff00ff00ff00"
	['"header"??*10"trailer"']="68656164657200000000000000000000747261696c6572ffffffffffff00000000000000000000ffffffffffffff"
	['"a"*k']=$(printf "61%.0s" {1..1024} ; printf "ff%.0s" {1..1024})
	['(("foo"*3 ??)*1k ff "bar") * 2']=$(
		printf "666f6f666f6f666f6f00%.0s" {1..1024} ; echo -n "ff626172";
		printf "666f6f666f6f666f6f00%.0s" {1..1024} ; echo -n "ff626172";
		printf "ffffffffffffffffff00%.0s" {1..1024} ; echo -n "ffffffff";
		printf "ffffffffffffffffff00%.0s" {1..1024} ; echo -n "ffffffff";
	)
)


# Begin tests
failcount=0

# Verify all bad patterns cause return code 2
for p in "${bad_patterns[@]}"
do
	return_code_test "" "${p}" 2 || failcount=$((failcount+1))
done

# Verify all good patterns expand as expected
for p in "${!good_patterns[@]}"
do
	pattern_test "$p" "${good_patterns[$p]}" || failcount=$((failcount+1))
done


if [[ ${failcount} -eq 0 ]] ; then
	echo ALL TESTS PASSED.
else
	echo ${failcount} tests failed >&2
	exit 1
fi

