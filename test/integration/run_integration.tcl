#!/usr/bin/env tclsh
# test/integration/run_integration.tcl
#
# Launcher for the openMSX headless integration tests.
#
# The tests drive a REAL openMSX instance through the console XML "stdio"
# control protocol, so they need:
#   * an openmsx binary on $PATH (or set OPENMSX_BIN=/path/to/openmsx), and
#   * the "C-BIOS_MSX1" machine definition (shipped with openMSX).
#
# Usage:
#   tclsh test/integration/run_integration.tcl        # all tests
#   OPENMSX_BIN=/path/to/openmsx tclsh test/integration/run_integration.tcl

source [file join [file dirname [file normalize [info script]]] step_back_integration_test.tcl]
