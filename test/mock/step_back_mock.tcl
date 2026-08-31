# test/mock/step_back_mock.tcl
#
# Logic-level mock of the openMSX API surface that disasm::step_back (in
# share/scripts/_disasm.tcl) relies on:
#     get_active_cpu, machine_info, reverse status/goto/goback,
#     reg PC/BC, debug disasm
# plus the load-time dependency set_help_text.
#
# The mock models the openMSX reverse feature as a synthetic timeline of
# instruction boundaries. Each boundary records {time pc bc} -- the time
# (arbitrary units; boundaries are 1.0 apart) and the state of PC/BC at that
# boundary ("about to execute" the instruction at pc).
#
# Reverse semantics mirrored from the engine:
#   reverse goback <duration>  -> land on the last boundary whose time is
#                                 <= (current_time - duration)
#   reverse goto  <T>          -> land on the first boundary whose time is
#                                 >= T (reverse goto only stops on complete
#                                 instruction boundaries, rounding forward)
#
# The mock intentionally does NOT reimplement the step_back algorithm; it only
# provides a deterministic world the real algorithm runs against, so the tests
# exercise the actual share/scripts/_disasm.tcl proc.

namespace eval mock {}

# mock::reset <boundaries>
#   boundaries: list of {time pc bc}, ascending time (1.0 apart).
#   Sets current to the first boundary.
proc mock::reset {boundaries} {
	set ::mock::bounds [list]
	foreach b $boundaries {
		lappend ::mock::bounds [dict create t [lindex $b 0] pc [lindex $b 1] bc [lindex $b 2]]
	}
	set ::mock::cur 0
}

# mock::set_instr <pc> <mnemonic>  -- what debug disasm <pc> returns (lindex 0)
proc mock::set_instr {pc mnemonic} {
	set ::mock::instr($pc) $mnemonic
}

proc mock::n {} { llength $::mock::bounds }
proc mock::time {i} { dict get [lindex $::mock::bounds $i] t }
proc mock::pc {i} { dict get [lindex $::mock::bounds $i] pc }
proc mock::bc {i} { dict get [lindex $::mock::bounds $i] bc }
proc mock::cur {} { expr {$::mock::cur} }

# index of last boundary with time <= T (clamp to [0, n-1])
proc mock::index_le {T} {
	set idx -1
	for {set i 0} {$i < [mock::n]} {incr i} {
		if {[mock::time $i] <= $T} { set idx $i } else { break }
	}
	if {$idx < 0} { set idx 0 }
	return $idx
}

# index of first boundary with time >= T (clamp to [0, n-1])
proc mock::index_ge {T} {
	for {set i 0} {$i < [mock::n]} {incr i} {
		if {[mock::time $i] >= $T} { return $i }
	}
	return [expr {[mock::n] - 1}]
}

proc mock::goto {T} { set ::mock::cur [mock::index_ge $T] }
proc mock::goback {duration} {
	set target [expr {[mock::time [mock::cur]] - $duration}]
	set ::mock::cur [mock::index_le $target]
}

# Current register/mnemonic accessors (what step_back reads via reg/debug)
proc mock::regPC {} { mock::pc [mock::cur] }
proc mock::regBC {} { mock::bc [mock::cur] }
proc mock::circuit_instr {} {
	set pc [mock::regPC]
	if {[info exists ::mock::instr($pc)]} { return [list [set ::mock::instr($pc)] $pc] }
	return [list "UNKNOWN" $pc]
}

###############################################################################
# openMSX API stubs (defined in the global namespace so that the procs inside
# the disasm namespace resolve to them)
###############################################################################

proc set_help_text {name text} { }
proc get_active_cpu {} { return z80 }
proc machine_info {what} {
	if {$what eq "z80_freq"} { return 3579545 }
	error "mock: unknown machine_info $what"
}
proc reverse {args} {
	# syntax: reverse <sub> [-novideo] <value>
	set sub [lindex $args 0]
	set rest [lrange $args 1 end]
	if {[lindex $rest 0] eq "-novideo"} { set rest [lrange $rest 1 end] }
	switch -- $sub {
		status { return [dict create current [mock::time [mock::cur]]] }
		goto   { mock::goto [lindex $rest 0]; return [mock::time [mock::cur]] }
		goback { mock::goback [lindex $rest 0]; return [mock::time [mock::cur]] }
		default { error "mock: unknown reverse $sub" }
	}
}
proc reg {name} {
	switch -- $name {
		PC - pc { return [mock::regPC] }
		BC - bc { return [mock::regBC] }
		default { error "mock: unknown reg $name" }
	}
}
proc debug {args} {
	if {[lindex $args 0] eq "disasm"} {
		set pc [lindex $args 1]
		if {[info exists ::mock::instr($pc)]} {
			return [list [set ::mock::instr($pc)] $pc]
		}
		return [list "UNKNOWN" $pc]
	}
	error "mock: unknown debug $args"
}
