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
#   shared subroutine: one LDIR subroutine called with a different BC from
#                      several sites (CALL/RET/LD overhead between runs)
#   exhausted Replay : reverse history starts mid-block, so the exponential
#                      goback cannot move (time frozen) and step_back errors
#                      instead of looping forever
#   handler-gap exit : a backoff landing falls inside the IRQ handler gap
#                      mid-span; Phase 3 must not mistake it for pre-block
#                      code, or Phase 4 would land on the depleted resume
#                      run instead of the true first iteration
#   non-block        : normal step_back (one boundary back) when neither the
#                      current instruction nor its predecessor is a block
#   marker fast path : with marker emulation on, step_back consults
#                      `reverse blockstart` (all other tests leave markers
#                      off to keep covering the heuristic fallback)

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
tcltest::test $ctx "is_return catches all return forms" {
	# RETI/RETN (interrupt returns) and plain RET / RET cc (subroutine
	# returns, and C-BIOS-style handler exits) must all match: any of them
	# as a run-start predecessor withholds "fresh" evidence.
	foreach op {ret reti retn {ret z} {ret nz}} {
		tcltest::is $ctx [disasm::is_return $op] "return $op"
	}
	foreach op {nop halt call ld jr ldir rst jp} {
		tcltest::not $ctx [disasm::is_return $op] "non-return $op"
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
	lappend tl [list $t 0x003A 2]; incr t                        ;# handler exit (plain RET, like C-BIOS)
	foreach k {2 1} { lappend tl [list $t $blk $k]; incr t }     ;# resumed block
	lappend tl [list $t 0x4005 0]                                ;# after block, call here
	# NOTE: the handler must END with a return at its own address (here a
	# plain RET, mirroring C-BIOS, which exits with EI/RET rather than
	# RETI): the resume run's predecessor has to disassemble as a return
	# for the predecessor half of the acceptance rule (a shared 0x0038 for
	# work+exit would read as a fresh entry instead). The gap-exit test
	# below models a RETI-ending handler instead, so both arms are covered.
	set instrs {0x4000 "ld bc,n" 0x4003 ldir 0x4005 ret 0x0038 handler 0x003A ret}
	set last [expr {[llength $tl] - 1}]
	set res [run_case $tl $instrs $last]
	# Must land on the INITIAL entry (max counter), not the resumed entry (2).
	tcltest::eq $ctx [dict get $res t] 1 "landed on first block entry"
	tcltest::eq_hex $ctx [dict get $res pc] $blk "landed on block addr"
	tcltest::eq_hex $ctx [dict get $res bc] $n "counter at initial maximum"
}

###############################################################################
# Scenario 6b: exhausted replay history -> loud error, not a hang
###############################################################################
tcltest::test $ctx "history exhausted mid-block raises internal error" {
	# Timeline starts AT the block (no pre-block boundary exists), as when
	# reverse recording started in the middle of a block execution. The
	# exponential goback then clamps at the oldest boundary without moving
	# (mock time frozen, like emulator time in the engine), so step_back
	# must raise instead of looping forever.
	set blk 0x4003
	set tl [list [list 0 $blk 4] [list 1 $blk 3] [list 2 $blk 2] \
	             [list 3 $blk 1] [list 4 0x4005 0]]
	set instrs {0x4003 ldir 0x4005 ret}
	mock::reset $tl
	foreach {pc mnem} $instrs {
		mock::set_instr $pc $mnem
	}
	set ::mock::cur 2
	set rc [catch {step_back} msg]
	tcltest::is $ctx {$rc != 0} "step_back raises instead of hanging"
	tcltest::is $ctx {[string match "*reverse system record unavailable*" $msg]} \
		"error names the unavailable reverse record"
}

###############################################################################
# Scenario 6c: backoff exit inside the IRQ gap still finds the true start
###############################################################################
tcltest::test $ctx "backoff landing in IRQ gap rewinds to first iteration" {
	# 48-iteration LDIR with the handler placed so that a backoff landing
	# falls inside the gap (t=41,42) mid-span. Exiting Phase 3 there must
	# not strand the forward scan inside the span: the resume run (BC=8)
	# must be rejected in favour of the true first iteration (BC=48).
	set blk 0x4003
	set tl [list [list 0 0x4000 48]]
	set bc 48
	for {set t 1} {$t <= 40} {incr t} {
		lappend tl [list $t $blk $bc]; incr bc -1
	}
	lappend tl [list 41 0x0038 8] [list 42 0x0039 8]
	for {set t 43} {$t <= 50} {incr t} {
		lappend tl [list $t $blk $bc]; incr bc -1
	}
	lappend tl [list 51 0x4005 0]
	set instrs {0x4000 "ld bc,n" 0x4003 ldir 0x4005 ret 0x0038 handler 0x0039 reti}
	set res [run_case $tl $instrs 51]
	tcltest::eq $ctx [dict get $res t] 1 "landed on first block entry, not the resume run"
	tcltest::eq_hex $ctx [dict get $res pc] $blk "landed on block addr"
	tcltest::eq_hex $ctx [dict get $res bc] 48 "counter at initial maximum, not depleted"
}

###############################################################################
# Scenario 8: shared LDIR subroutine called with varying BC per call site
###############################################################################
tcltest::test $ctx "shared subroutine with varying BC rewinds current execution" {
	# Subroutine 'LDIR; RET' at 0x4003/0x4005, called with BC=5, then 3,
	# then 4 from three sites (LD/ CALL/RET overhead between runs).
	# NOTE (mock time-scale blind spot): mock strides sit far below the
	# 1.0 boundary spacing, so Phase 3 single-steps back and exits at the
	# nearest call overhead; the scan window then covers only the current
	# execution, and the max rule never sees the earlier higher-count run.
	# Varying counts across executions that share one scan window need
	# real-hardware strides -- covered by the integration test.
	set blk 0x4003
	set tl [list [list 0 0x4010 5] [list 1 0x4013 5]]
	set t 2
	foreach k {5 4 3 2 1} { lappend tl [list $t $blk $k]; incr t }
	lappend tl [list $t 0x4005 0]; incr t
	lappend tl [list $t 0x4020 3]; incr t
	lappend tl [list $t 0x4023 3]; incr t
	foreach k {3 2 1} { lappend tl [list $t $blk $k]; incr t }
	lappend tl [list $t 0x4005 0]; incr t
	lappend tl [list $t 0x4030 4]; incr t
	lappend tl [list $t 0x4033 4]; incr t
	set t3 $t
	foreach k {4 3 2 1} { lappend tl [list $t $blk $k]; incr t }
	lappend tl [list $t 0x4005 0]; incr t
	lappend tl [list $t 0x4036 0]
	set instrs {0x4010 "ld bc,n" 0x4013 call 0x4020 "ld bc,n" 0x4023 call \
	            0x4030 "ld bc,n" 0x4033 call 0x4003 ldir 0x4005 ret 0x4036 halt}
	# step_back from the subroutine RET right after the third execution
	# (BC=4): must land on its first iteration, not on an earlier call.
	set res [run_case $tl $instrs [expr {[llength $tl] - 2}]]
	tcltest::eq $ctx [dict get $res t] $t3 "landed on current execution start"
	tcltest::eq_hex $ctx [dict get $res pc] $blk "landed on block addr"
	tcltest::eq_hex $ctx [dict get $res bc] 4 "counter of the current execution"
}

###############################################################################
# Scenario 9: marker fast path is taken when markers exist
###############################################################################
tcltest::test $ctx "marker fast path lands via blockstart, not heuristics" {
	# Same single-run timeline as Scenario 1, but with marker emulation on:
	# step_back must consult `reverse blockstart` (used_marker set) and
	# land on the recorded entry. All other tests leave markers disabled
	# so they keep covering the heuristic fallback path.
	set n 4
	set tl [ldir_tl $n]
	set instrs {0x4000 "ld bc,n" 0x4003 ldir 0x4005 ret}
	mock::reset $tl
	foreach {pc mnem} $instrs {
		mock::set_instr $pc $mnem
	}
	set ::mock::markers_enabled 1
	set ::mock::cur [expr {[llength $tl] - 1}]
	step_back
	set i [mock::cur]
	tcltest::is $ctx {$::mock::used_marker} "blockstart consulted (fast path)"
	tcltest::eq_hex $ctx [mock::pc $i] 0x4003 "landed on block addr"
	tcltest::eq_hex $ctx [mock::bc $i] $n "counter back at maximum"
	tcltest::eq $ctx [mock::time $i] 1 "landed at first-iteration boundary"
	set ::mock::markers_enabled 0
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
