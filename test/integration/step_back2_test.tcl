source "test/integration/test_it.tcl"
set ::log [open "test_roms_stepback2_basic.log" w]
proc L {m} { puts $::log $m; flush $::log }
it::boot_cart [it::rom "test_roms_msx_60hz_16kb_ldir_nested.rom"]
set bp [omsx::set_bp "0x4025"]
it::run_to_bp
L [format "1 pc=%04x" [it::reg pc]]
::disasm::step_back2
L [format "2 pc=%04x" [it::reg pc]]
omsx::rm_bp $bp
omsx::exit
close $::log
exit
