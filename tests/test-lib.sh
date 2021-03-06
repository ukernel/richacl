# Library for simple test scripts
# Copyright (C) 2009, 2011-2013 Free Software Foundation, Inc.
#
# Copying and distribution of this file, with or without modification,
# in any medium, are permitted without royalty provided the copyright
# notice and this notice are preserved.

use_tmpdir() {
    tmpdir=$abs_top_builddir/tests/tmp.$$
    mkdir "$tmpdir" && cd "$tmpdir" || exit 2
}

require_su() {
    if ! $abs_top_builddir/tests/su 99 99 true ; then
	echo "This test must be run as root" >&2
	exit 77
    fi
}

require_richacls() {
    $abs_top_builddir/tests/require-richacls || exit $?
}

_SU=
su() {
    _start_test -1 su "$*"
    if [ $# = 0 ]; then
	_SU=
    else
	_SU="$abs_top_builddir/tests/su $*"
    fi
    echo "ok"
}

if diff -u -L expected -L got /dev/null /dev/null 2> /dev/null; then
    eval '_compare() {
	diff -u -L expected -L got "$1" "$2"
    }'
else
    eval '_compare() {
	echo "expected:"
	cat "$1"
	echo "got:"
	cat "$2"
    }'
fi

_check() {
    local frame=$1
    shift
    _start_test "$frame" "$*"
    expected=`cat`
    if got=`set +x; eval "$_SU $*" 3>&2 </dev/null 2>&1` && \
            test "$expected" = "$got" ; then
	echo "ok"
	checks_succeeded="$checks_succeeded + 1"
    else
	echo "FAILED"
	if test "$expected" != "$got" ; then
	    echo "$expected" > expected~
	    echo "$got" > got~
	    _compare expected~ got~
	    rm -f expected~ got~
	fi
	checks_failed="$checks_failed + 1"
    fi
}

check() {
    _check 0 "$@"
}

ncheck() {
    _check 0 "$@" < /dev/null
}

parent_check() {
    _check 1 "$@"
}

parent_ncheck() {
    _check 1 "$@" < /dev/null
}

cleanup() {
    status=$?
    checks_succeeded=`expr $checks_succeeded`
    checks_failed=`expr $checks_failed`
    checks_total=`expr $checks_succeeded + $checks_failed`
    if test $checks_total -gt 0 ; then
	if test $checks_failed -gt 0 && test $status -eq 0 ; then
	    status=1
	fi
	echo "$checks_total tests ($checks_succeeded passed," \
	     "$checks_failed failed)"
    fi
    if test -n "$tmpdir" ; then
	chmod -R u+rwx "$tmpdir" 2>/dev/null
	cd / && rm -rf "$tmpdir"
    fi
    exit $status
}

if test -z "`echo -n`"; then
    if eval 'test -n "${BASH_LINENO[0]}" 2>/dev/null'; then
	eval '
	    _start_test() {
		local frame=$1
		shift
		printf "[${BASH_LINENO[2+frame]}] $* -- "
	    }'
    else
	eval '
	    _start_test() {
		shift
		printf "* $* -- "
	    }'
    fi
else
    eval '
	_start_test() {
	    shift
	    printf "* $*\\n"
	}'
fi

if ! type cat > /dev/null 2> /dev/null; then
    echo "This test requires the cat utility" >&2
    exit 77
fi

export PATH=$abs_top_builddir/tests:$abs_top_builddir/src:$PATH

checks_succeeded=0
checks_failed=0
trap cleanup 0
