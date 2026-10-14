source "test/integration/test_it.tcl"
set ::log [open "test_mock_stepback2.log" w]
proc L {m} { puts $::log $m; flush $::log }
L [info procs ::disasm::step_back2]
omsx::exit
close $::log
