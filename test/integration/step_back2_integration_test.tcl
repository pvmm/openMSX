# test/integration/step_back2_integration_test.tcl
#
# Headless openMSX integration tests for `step_back2` in
# share/scripts/_disasm.tcl -- the blockInstruction-probe based variant of
# step_back. Instead of marker lookups / heuristic scans, step_back2:
#   * registers temporary `debug probe set_bp` callbacks on the
#     z80.blockInstruction and r800.blockInstruction probes,
#   * rewinds by a large amount (1000 then 65536 block iterations),
#   * runs the emulation forward to the original time, collecting the LAST
#     block run start seen,
#   * then jumps back to that last trigger (the start of the block run).
#
# These run on a REAL emulated Z80 via the openMSX console XML "stdio" control
# protocol (see test/common/openmsx_driver.tcl), using the cartridge ROM
# fixtures in test/roms/ (same fixtures as the step_back suite).
#
# Each test breaks the CPU at-or-just-after a block repeat instruction, calls
# step_back2, and asserts we land on the block PC with the registers restored
# to their ORIGINAL (pre-block) values.
#
# Requires an openMSX binary (see run_integration.tcl). This file is
# standalone so it can be run directly:
#   tclsh test/integration/step_back2_integration_test.tcl

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
# by a breakpoint.
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

set ctx [tcltest::new step_back2_integration]

# --- Test cases -----------------------------------------------------------

# 1) LDIR, after-block: break at the JR right after the finished LDIR; step_back2
#    must rewind to the start of that LDIR run (BC restored to the initial $2000).
tcltest::test $ctx "ldir after-block (step_back2 past finished block)" {
	it::boot_cart [it::rom msx_60hz_16kb_ldir.rom]
	set bp [omsx::set_bp "0x401d"]              ;# JR FOREVER (after LDIR)
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x401d "break fires after block"
	omsx::step_back2
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "lands on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x2000 "BC restored to 8192"
	tcltest::eq_hex $ctx [it::reg hl] 0x4000 "HL restored to source"
	tcltest::eq_hex $ctx [it::reg de] 0xc000 "DE restored to dest"
	omsx::rm_bp $bp
	omsx::exit
}

# 2) LDIR, mid-block (interrupted by IRQ): a condition breakpoint fires halfway
#    through the LDIR (BC==0x1000); step_back2 must rewind to before the FIRST
#    iteration (BC=$2000). This exercises the 65536-iteration retry path, since
#    the initial 1000-iteration rewind does not reach back past the run start.
tcltest::test $ctx "ldir mid-block interrupted by IRQ rewinds to first iteration" {
	it::boot_cart [it::rom msx_60hz_16kb_ldir.rom]
	set bp [omsx::set_bp "0x401b" {[reg BC] == 0x1000}]
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "break fires on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x1000 "BC mid-block (0x1000)"
	omsx::step_back2
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "lands on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x2000 "BC restored to 8192 (first iteration)"
	tcltest::eq_hex $ctx [it::reg hl] 0x4000 "HL restored to source"
	tcltest::eq_hex $ctx [it::reg de] 0xc000 "DE restored to dest"
	omsx::rm_bp $bp
	omsx::exit
}

# 3) OTIR, after-block (B-only counter): breaking after the OTIR and stepping back
#    must land on the OTIR with B=0 (256 iters) restored.
tcltest::test $ctx "otir after-block B-only counter reverts to B=0" {
	it::boot_cart [it::rom msx_60hz_16kb_otir.rom]
	set bp [omsx::set_bp "0x401b"]              ;# JR FOREVER (after OTIR)
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "break fires after OTIR"
	omsx::step_back2
	tcltest::eq_hex $ctx [it::reg pc] 0x4019 "lands on OTIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x00a0 "B=0,C=0xA0 preserved (256 iters)"
	omsx::rm_bp $bp
	omsx::exit
}

# 4) LDIR inside a DJNZ outer loop (4 passes, DI so no IRQ): the block PC is the
#    same on every pass. Breaking mid-LDIR and stepping back must land on the
#    first iteration of the CURRENT block execution (BC=$2000), never an earlier
#    loop pass.
tcltest::test $ctx "ldir inside DJNZ loop mid-block rewinds to first iteration" {
	it::boot_cart [it::rom msx_60hz_16kb_ldir_loop.rom]
	set bp [omsx::set_bp "0x4019" {[reg BC] == 0x1000}]
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x4019 "break fires on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x1000 "BC mid-block (0x1000)"
	omsx::step_back2
	tcltest::eq_hex $ctx [it::reg pc] 0x4019 "lands on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x2000 "BC restored to first iteration"
	tcltest::eq_hex $ctx [it::reg hl] 0x4000 "HL restored to source"
	tcltest::eq_hex $ctx [it::reg de] 0xc000 "DE restored to dest"
	omsx::rm_bp $bp
	omsx::exit
}

set fails [tcltest::summary $ctx]
if {$fails > 0} { exit 1 }
exit 0
