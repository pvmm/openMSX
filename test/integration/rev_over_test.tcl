# test/integration/step_back_integration_test.tcl
#
# Headless openMSX integration tests for the `step_back` improvements in
# share/scripts/_disasm.tcl (block-repeat instructions: LDIR/LDDR/CPIR/CPDR/
# INIR/INDR/OTIR/OTDR).
#
# These run on a REAL emulated Z80 via the openMSX console XML "stdio"
# control protocol (see test/common/openmsx_driver.tcl). They use the
# cartridge ROM fixtures in test/roms/:
#
# The KEY regression these guard: previously step_back only recognised a
# block-repeat instruction as "current" -- it did NOT rewind past a block
# that had JUST completed (the instruction immediately AFTER the block), and
# it did not reliably undo an interrupted block to before its first
# iteration. The block tests below call step_back from a block or from the
# instruction just after it, and assert we land on the block PC with the
# registers restored to their ORIGINAL (pre-block) values.
#
# Requires an openMSX binary (see run_integration.tcl). This file is
# standalone so it can be run directly:
#   tclsh test/integration/step_back_integration_test.tcl

set here [file dirname [file normalize [info script]]]
source [file join $here .. common tcltest.tcl]
source [file join $here .. common openmsx_driver.tcl]

namespace eval it {}

set ::it::romdir [file normalize [file join $here .. roms]]

# it::rom <basename>  -- full path to a rom fixture
proc it::rom {basename} { return [file join $::it::romdir $basename] }

# it::boot_cart <rom>  -- spawn, power on, wait until the cart is mapped at
# page 1 (0x4000 == 0x41). Returns when the cartridge is live.
proc it::boot_cart {rom} {
	omsx::spawn -machine C-BIOS_MSX1 -cart $rom
	omsx::sleepms 600
	omsx::power_on
	omsx::ensure_reverse
	set dl [expr {[clock milliseconds] + 20000}]
	while {1} {
		set v [omsx::ok "debug read memory 0x4000"]
		if {$v eq "65"} { break }        ;# 0x41 = 'A' (cart header)
		if {[clock milliseconds] > $dl} { error "cart never mapped at 0x4000" }
		omsx::sleepms 150
	}
	omsx::sleepms 100
}

# it::run_to_bp  -- reset so the cart re-runs (block executes again), then
# continue so the machine actually runs, and wait until the CPU is breaked
# by a breakpoint. (reset itself leaves the CPU stalled at the reset vector
# in the debug/control session, hence the explicit `debug cont`.)
proc it::run_to_bp {} {
	set dl [expr {[clock milliseconds] + 30000}]
	omsx::reset
	omsx::continue_cpu
	while {1} {
		if {[omsx::is_breaked]} { break }
		if {[clock milliseconds] > $dl} { error "CPU never reached breakpoint (breaked=[omsx::is_breaked])" }
		omsx::sleepms 50
	}
}

proc it::reg {name} { return [omsx::reg $name] }

# it::cont_to_bp  -- like run_to_bp but WITHOUT reset: continue from the
# current breaked position (e.g. a previous breakpoint hit) and wait for
# the next hit of a persistent breakpoint. Used to observe the 2nd, 3rd,
# ... firing of one breakpoint without re-running the ROM.
proc it::cont_to_bp {} {
	set dl [expr {[clock milliseconds] + 30000}]
	omsx::continue_cpu
	while {1} {
		if {[omsx::is_breaked]} { break }
		if {[clock milliseconds] > $dl} { error "CPU never reached breakpoint (breaked=[omsx::is_breaked])" }
		omsx::sleepms 50
	}
}

# it::scan_ldirs <rom> -- scan a ROM image for 'LD BC,nn; LDIR' pairs.
# Returns a list of {ldir_addr bc_value} in CPU address space (the image is
# mapped at 0x4000). This lets the many-random-BC test derive the expected
# sizes from the ROM itself instead of duplicating the table in Tcl.
proc it::scan_ldirs {rom} {
	set f [open $rom rb]
	set data [read $f]
	close $f
	binary scan $data H* hex
	set hex [string tolower $hex]
	set result {}
	set pos 0
	while {1} {
		set h [string first "edb0" $hex $pos]
		if {$h < 0} break
		set i [expr {$h / 2}]
		if {($i >= 3) &&
		    ([string range $hex [expr {2 * ($i - 3)}] [expr {2 * ($i - 3) + 1}]] eq "01")} {
			set lo [scan [string range $hex [expr {2 * ($i - 2)}] [expr {2 * ($i - 2) + 1}]] %x]
			set hi [scan [string range $hex [expr {2 * ($i - 1)}] [expr {2 * ($i - 1) + 1}]] %x]
			lappend result [list [expr {0x4000 + $i}] [expr {$lo + 256 * $hi}]]
		}
		set pos [expr {$h + 2}]
	}
	return $result
}

set ctx [tcltest::new step_back_integration]

# --- Test cases -----------------------------------------------------------

# 1) step_back_over: reverse step over a CALL/RET
tcltest::test $ctx "step_back_over" {
	it::boot_cart [it::rom call_ret.rom]
	# Ensure local script is loaded
	catch {omsx::ok "source /home/pedro/projects/emu/openMSX/share/scripts/_disasm.tcl"}
	set bp [omsx::set_bp "0x4011"]              ;# LD DE after first call returns
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x4011 "break fires after call returns"
	omsx::step_back_over
	# Should step back over the entire call to before CALL
	set pc [it::reg pc]
	tcltest::is $ctx {$pc == 0x400e} "step_back_over lands before or at call (pc=[format 0x%04X $pc])"
	omsx::rm_bp $bp
	omsx::exit
}

set fails [tcltest::summary $ctx]
if {$fails > 0} { exit 1 }
exit 0
