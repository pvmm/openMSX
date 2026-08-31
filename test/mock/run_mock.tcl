# test/mock/run_mock.tcl
#
# Entry point for the logic-level mock test suite. Run from the repository
# root with:
#     tclsh test/mock/run_mock.tcl
# (no openMSX needed). Exits 0 on success, 1 if any test failed.
source [file join [file dirname [info script]] step_back_tests.tcl]
