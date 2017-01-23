#!/bin/bash

#!/bin/bash

BGREP=../src/bgrep


function return_code_test() {
	local input="$1"
	local pattern="$2"
	local expected="$3"

	echo -en "${input}" | ${BGREP} -q -x "${pattern}"
	local result="$?"
	if [[ "${result}" -ne "${expected}" ]] ; then
		echo "${FUNCNAME[0]}: Test FAILED."
		echo "Pattern ${pattern} should have returned code ${expected} (was ${result})"
		echo "Input was: ${input}"
		return 1
	fi
	return 0
}

declare -A code_0_inputs=(
	['??']='a'
	['??*2']='an'
	['01']='foo \x01 bar'
	['0101']='foo \x01\x01 bar'
	['01 01']='aaaa \x01\x01 bbbbb'
	['01*2']='\x01\x01'
	['"foo"']='1234foo1234'
	['"foo"*2']='1234foofoo1234'
	['("foo"??)*w']='fooxfooy'
	['12*3 44']='---\x12\x12\x12D---'
	['((12 ??)*2)*2']='\x12u\x12v\x12w\x12x'
	['"header"??*10"trailer"']='header0123456789trailer'
	['"a"*k']=$(printf "12341111" ; printf "a%.0s" {1..1024})
	['(("foo"*3 ??)*1k ff "bar") * 2']=$(
		printf "foofoofoox%.0s" {1..1024} ; echo -en "\xffbar";
		printf "foofoofooy%.0s" {1..1024} ; echo -en "\xffbar";
	)
)

declare -A code_1_inputs=(
	['??']=''
	['??*2']='a'
	['01']='foo \x02 bar'
	['0101']='foo \x02\x01 bar'
	['01 01']='aaaa \x01\x02 bbbbb'
	['01*2']='\x01\x03'
	['"foo"']='1234Foo1234'
	['"foo"*2']='1234fooFoo1234'
	['("foo"??)*w']='fOoxfooy'
	['12*3 44']='---\x12\x12\x12E---'
	['((12 ??)*2)*2']='\x12u\x12v\x13w\x12x'
	['"header"??*10"trailer"']='header0123456789atrailer'
	['"a"*k']=$(echo "12341123" ; printf "a%.0s" {1..1023})
	['(("foo"*3 ??)*1k ff "bar") * 2']=$(
		printf "foof0ofoox%.0s" {1..1024} ; echo -en "\xffbar";
		printf "foofoofooy%.0s" {1..1024} ; echo -en "\xffbar";
	)
)

# Begin tests
failcount=0

# Verify all matches cause return code 0
for p in "${!code_0_inputs[@]}"
do
	return_code_test "${code_0_inputs[$p]}" "${p}" 0 || failcount=$((failcount+1))
done

# Verify all non-matches cause return code 1
for p in "${!code_1_inputs[@]}"
do
	return_code_test "${code_1_inputs[$p]}" "${p}" 1 || failcount=$((failcount+1))
done


if [[ ${failcount} -eq 0 ]] ; then
	echo ALL TESTS PASSED.
else
	echo ${failcount} tests failed >&2
	exit 1
fi


