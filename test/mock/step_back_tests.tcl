# test/mock/step_back_tests.tcl
#
# Automated tests for disasm::step_back using a logic-level mock of the
# openMSX reverse timeline. These exercise the REAL algorithm in
# share/scripts/_disasm.tcl without needing a running MSX.
#
# Scenarios:
#   is_block_repeat  : all 8 block-repeat opcodes + positives/negatives
#   single LDIR      : step_back from after the block lands before first iter
#   LDIR-then-RET    : the original bug -- current instruction is the RET that
#                      immediately follows the block; still lands before the
#                      first iteration, NOT on the RET predecessor
#   current-is-block : step_back called while PC is mid-block (inside a pass)
#                      also lands before the first iteration
#   IRQ interrupted  : block interrupted by a handler then resumed; lands on
#                      the initial (max-counter) block entry
#   multi-pass loop  : block inside a DJNZ loop; lands on the LATEST pass's
#                      first iteration, not an earlier pass
#   OTIR (B counter) : B-only block counter handling for an OUT block repeat
#   non-block        : normal step_back (one boundary back) when neither the
#                      current instruction nor its predecessor is a block

source [file join [file dirname [info script]] .. common tcltest.tcl]
source [file join [file dirname [info script]] step_back_mock.tcl]
source [file join [file dirname [info script]] .. .. share scripts _disasm.tcl]

# run_case: (re)load a set of instructions and a timeline, set current to
# boundary index <from>, call step_back, and return a dict with the resulting
# current boundary {t pc bc}.
proc run_case {timeline instrs from} {
	mock::reset $timeline
	foreach {pc mnem} $instrs {
		mock::set_instr $pc $mnem
	}
	set ::mock::cur $from
	step_back
	set i [mock::cur]
	return [dict create t [mock::time $i] pc [mock::pc $i] bc [mock::bc $i]]
}

# Build the timeline for a single execution of an LDIR-style block:
#   LD BC,n @0x4000 ; block @0x4003 ; RET @0x4005
# counter runs n, n-1, ..., 1 at boundaries then exits to 'after' with bc=0.
proc ldir_tl {n} {
	set tl [list [list 0 0x4000 $n]]
	set t 1
	for {set k $n} {$k >= 1} {incr t; set k [expr {$k - 1}]} {
		lappend tl [list $t 0x4003 $k]
	}
	lappend tl [list $t 0x4005 0]
	return $tl
}


###############################################################################
# is_block_repeat unit tests
###############################################################################
set ctx [tcltest::new mockctx]
tcltest::test $ctx "is_block_repeat catches all 8 opcodes" {
	# debug disasm returns mnemonics in lowercase; string match is
	# case-sensitive intentionally.
	foreach op {ldir lddr cpir cpdr inir indr otir otdr} {
		tcltest::is $ctx [disasm::is_block_repeat $op] "opcode $op"
	}
}
tcltest::test $ctx "is_block_repeat rejects non-block opcodes" {
	# These must not start with any of the 8 block prefixes
	# (ldir/lddr/cpir/cpdr/inir/indr/otir/otdr).
	foreach op {ldi ldd cpi cpd ini ind outi outd nop ret halt call ld jr lda cpid halt_x} {
		tcltest::not $ctx [disasm::is_block_repeat $op] "non-block $op"
	}
}

###############################################################################
# Scenario 1: single LDIR -- step_back from after the block
###############################################################################
tcltest::test $ctx "single LDIR, step_back from after block" {
	# Program: LD BC,n / LDIR @0x4003 / RET @0x4005
	set n 4
	set tl [ldir_tl $n]
	set instrs {0x4000 "ld bc,n" 0x4003 ldir 0x4005 ret}
	# current = last boundary (after the block), PC=0x4005 RET
	set res [run_case $tl $instrs [expr {[llength $tl] - 1}]]
	tcltest::eq_hex $ctx [dict get $res pc] 0x4003 "landed on block addr"
	tcltest::eq_hex $ctx [dict get $res bc] $n "counter back at maximum"
	tcltest::eq $ctx [dict get $res t] 1 "landed at first-iteration boundary"
}

###############################################################################
# Scenario 3: the original bug -- current instruction is the RET after LDIR
###############################################################################
tcltest::test $ctx "LDIR-then-RET: step_back from the RET rewinds before block" {
	set n 4
	set tl [ldir_tl $n]
	set instrs {0x4000 "ld bc,n" 0x4003 ldir 0x4005 ret}
	# The RET at 0x4005 is the last boundary (index n+1). PC=0x4005 is not a
	# block, but it immediately follows the block -- must still rewind to the
	# first iteration. This is regression for the original bug.
	set last [expr {[llength $tl] - 1}]
	set res [run_case $tl $instrs $last]
	tcltest::eq_hex $ctx [dict get $res pc] 0x4003 "landed on block addr"
	tcltest::eq_hex $ctx [dict get $res bc] $n "counter back at maximum"
	tcltest::eq $ctx [dict get $res t] 1 "landed at first-iteration boundary"
}

###############################################################################
# Scenario 6: current instruction IS mid-block
###############################################################################
tcltest::test $ctx "step_back from mid-block lands before first iteration" {
	set n 4
	set tl [ldir_tl $n]
	set instrs {0x4000 "ld bc,n" 0x4003 ldir 0x4005 ret}
	# Call from boundary index 3 = PC=0x4003, BC=2 (mid-run)
	set res [run_case $tl $instrs 3]
	tcltest::eq_hex $ctx [dict get $res pc] 0x4003 "landed on block addr"
	tcltest::eq_hex $ctx [dict get $res bc] $n "counter back at maximum"
	tcltest::eq $ctx [dict get $res t] 1 "landed at first-iteration boundary"
}

###############################################################################
# Scenario 5: OTIR (B-only counter)
###############################################################################
tcltest::test $ctx "OTIR with B-only counter rewinds to first iteration" {
	# otir at 0x4004, initial B=4, C=0xA0 (port). BC values B*256+C.
	set c 0xA0
	set n 4
	set tl [list [list 0 0x4002 [expr {0x04A0}]]]   ;# t0: load instruction
	set t 1
	for {set k $n} {$k >= 1} {incr t; set k [expr {$k - 1}]} {
		lappend tl [list $t 0x4004 [expr {($k << 8) | $c}]]
	}
	lappend tl [list $t 0x4005 [expr {0x00A0}]]
	set instrs {0x4002 "ld bc,0" 0x4004 otir 0x4005 ret}
	set last [expr {[llength $tl] - 1}]
	set res [run_case $tl $instrs $last]
	tcltest::eq_hex $ctx [dict get $res pc] 0x4004 "landed on OTIR addr"
	tcltest::eq_hex $ctx [dict get $res bc] [expr {($n << 8) | $c}] "B back at max, C preserved"
	tcltest::eq $ctx [dict get $res t] 1 "landed at first-iteration boundary"
}

###############################################################################
# Scenario 4: multi-pass loop -> latest pass first iteration
###############################################################################
tcltest::test $ctx "multi-pass loop lands on latest pass first iteration" {
	# Two passes each doing LDIR with counter 3. Block at 0x401A, DJNZ at 0x401B.
	set blk 0x401A
	set n 3
	set tl [list]
	# pass 1
	lappend tl [list 0 0x4017 $n]
	set t 1
	for {set k $n} {$k >= 1} {incr t; set k [expr {$k - 1}]} { lappend tl [list $t $blk $k] }
	lappend tl [list $t 0x401B 0]                          ;# DJNZ (pass1 done)
	incr t
	lappend tl [list $t 0x4017 $n]                         ;# loop back, reload counter
	incr t
	# pass 2
	set t2 $t
	for {set k $n} {$k >= 1} {incr t; set k [expr {$k - 1}]} { lappend tl [list $t $blk $k] }
	lappend tl [list $t 0x401B 0]                          ;# DJNZ (pass2 done) <- call here
	set instrs {0x4017 "ld bc,n" 0x401A ldir 0x401B djnz}
	set last [expr {[llength $tl] - 1}]
	set res [run_case $tl $instrs $last]
	tcltest::eq $ctx [dict get $res t] $t2 "landed on pass2 first iteration"
	tcltest::eq_hex $ctx [dict get $res pc] $blk "landed on block addr"
	tcltest::eq_hex $ctx [dict get $res bc] $n "counter at max for pass2"
}

###############################################################################
# Scenario 5b: IRQ-interrupted block -> lands on initial (max) entry
###############################################################################
tcltest::test $ctx "IRQ-interrupted block lands on initial max-counter entry" {
	set blk 0x4003
	set n 5
	# boundaries: 5,4,3 block iters, then IRQ->handler, handler work,
	# resume block 2,1, then after.
	set tl [list]
	lappend tl [list 0 0x4000 $n]
	set t 1
	foreach k {5 4 3} { lappend tl [list $t $blk $k]; incr t }   ;# before IRQ
	lappend tl [list $t 0x0038 2]; incr t                        ;# IRQ handler entry
	lappend tl [list $t 0x0038 2]; incr t                        ;# handler work
	foreach k {2 1} { lappend tl [list $t $blk $k]; incr t }     ;# resumed block
	lappend tl [list $t 0x4005 0]                                ;# after block, call here
	set instrs {0x4000 "ld bc,n" 0x4003 ldir 0x4005 ret 0x0038 handler}
	set last [expr {[llength $tl] - 1}]
	set res [run_case $tl $instrs $last]
	# Must land on the INITIAL entry (max counter), not the resumed entry (2).
	tcltest::eq $ctx [dict get $res t] 1 "landed on first block entry"
	tcltest::eq_hex $ctx [dict get $res pc] $blk "landed on block addr"
	tcltest::eq_hex $ctx [dict get $res bc] $n "counter at initial maximum"
}

###############################################################################
# Scenario 7: non-block -> normal one-boundary step back
###############################################################################
tcltest::test $ctx "non-block instruction steps back one boundary" {
	# program: NOP @0x4000, NOP @0x4001, NOP @0x4002
	set tl [list [list 0 0x4000 0] [list 1 0x4001 0] [list 2 0x4002 0] [list 3 0x4003 0]]
	set instrs {0x4000 nop 0x4001 nop 0x4002 nop 0x4003 nop}
	set res [run_case $tl $instrs 2]
	tcltest::eq $ctx [dict get $res t] 1 "stepped back exactly one boundary"
	tcltest::eq_hex $ctx [dict get $res pc] 0x4001 "PC back one instruction"
}

set fails [tcltest::summary $ctx]
if {$fails > 0} { exit 1 }
exit 0
