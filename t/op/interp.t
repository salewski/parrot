#! perl -w

use Parrot::Test tests => 7;

output_is(<<'CODE', <<'OUTPUT', "runinterp - new style");
	new P0, .ParrotInterpreter
	print "calling\n"
	# set_addr/invoke ?
	runinterp P0, foo
	print "ending\n"
	end
	print "bad things!\n"
foo:
	print "In 2\n"
	end
CODE
calling
In 2
ending
OUTPUT
output_like(<<'CODE', <<'OUTPUT', "restart trace");
	printerr "ok 1\n"
	set I0, 1
	trace I0
	printerr "ok 2\n"
	dec I0
	trace I0
	printerr "ok 3\n"
	end
CODE
/^ok\s1\n
(?:PC=7.*)?\n
ok\s2\n
(?:PC=9.*)?\n
(?:PC=11.*)?\n
ok\s3\n$/x
OUTPUT

output_like(<<'CODE', <<'OUTPUT', "interp - warnings");
	new P0, .PerlUndef
	set I0, P0
	printerr "nada:"
	warningson 1
	new P1, .PerlUndef
	set I0, P1
	end
CODE
/^nada:Use of uninitialized value in integer context/
OUTPUT

output_is(<<'CODE', <<'OUTPUT', "getinterp");
    .include "interpinfo.pasm"
    getinterp P0
    print "ok 1\n"
    set I0, P0[.INTERPINFO_ACTIVE_PMCS]
    interpinfo I1, .INTERPINFO_ACTIVE_PMCS
    eq I0, I1, ok2
    print "not "
ok2:
    print "ok 2\n"
    end
CODE
ok 1
ok 2
OUTPUT

output_is(<<'CODE', <<'OUTPUT', "access argv");
    .include "iglobals.pasm"
    getinterp P1
    set P2, P1[.IGLOBALS_ARGV_LIST]
    set I0, P5
    set I1, P2
    eq I0, I1, ok1
    print "not "
ok1:
    print "ok 1\n"
    set S0, P5[0]
    set S1, P2[0]
    eq S0, S1, ok2
    print "not "
ok2:
    print "ok 2\n"
    end
CODE
ok 1
ok 2
OUTPUT

output_like(<<'CODE', <<'OUTPUT', "runinterp - set flags");
    .include "interpflags.pasm"
    new P0, .ParrotInterpreter
    print "calling\n"
    set P0[-1], .INTERPFLAG_PARROT_TRACE_FLAG
    runinterp P0, foo
    print "ending\n"
    end
    print "bad things!\n"
foo:
    print "In 2\n"
    end
CODE
/calling\n
PC=\d+.*\n
In\s2\n
PC=\d+.*\n
ending\n/x
OUTPUT

output_is(<<'CODE', <<'OUTPUT', "check_events");
    print "before\n"
    check_events
    print "after\n"
    end
CODE
before
after
OUTPUT

1;
