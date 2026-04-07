# symboltracer: a profiler for the Probe/Trace viewer
#
# symboltracer uses the new Probe/Trace viewer as a profiler and symbol files
# to register functions.

namespace eval symboltracer {
namespace export add list remove start stop
namespace ensemble create -prefixes 0

variable names {}         ;# user traces
variable context {}       ;# store expression result from recursive calls
variable symbolfiles {}   ;# symbols file name

set_help_proc symboltracer [namespace code symboltracer_help]
proc symboltracer_help {args} {
	if {[llength $args] == 1} {
		return {symboltracer is a profiler that traces function calls in the Trace Viewer widget

Recognized commands: start, stop, add, list, remove

Type 'help symboltracer <command>' for more information about each command.
}
    }
	switch -- [lindex $args 1] {
		"start" { return {Opens a symbol file and start tracing session

'start' reads a symbol file (like a .noi or .sym file) and creates breakpoints and traces for each entry found.

Syntax: symboltracer start [<sym-file>]
}}
		"add" { return {Creates breakpoint and trace for a user-defined name and address

'add' inserts a user-defined breakpoint and trace. No previously defined symbol is necessary. This is most useful for quick tracing sessions.

Syntax: symboltracer add <name> <address>
}}
		"stop" { return {Interrupts symboltracer session

'stop' removes all breakpoints and traces associated with symboltracer and all files included with the 'start' command.

Syntax: symboltracer stop
}}
		"list" { return {Verifies if a user trace exists

'list' returns all user traces created by symboltracer when executed without parameters or the address of the user trace if a user trace name is specified.

Syntax: symboltracer list my_function
        symboltracer list
}}
		"remove" { return {Removes user trace from the trace viewer

'remove' deletes the user specified user trace and associated breakpoint.

Syntax: symboltracer remove my_function
}}
		default { error "Unknown subcommand. Must be one of 'add', 'list', 'remove', 'start' or 'stop'." }
	}
}

proc _enter_function {name} {
	# find function return address
	set retaddr [peek16 [reg SP]]
	# check if we create caller breakpoint for CALL or RST
	if {[peek [expr {$retaddr - 1}]] in "0 8 16 24 32 40 48 56" ||
	    [peek [expr {$retaddr - 3}]] in "196 204 205 212 220 228 236 244 252"} {
		variable names
		variable context
		if {[dict get $names $name expression] ne {}} {
			# evaluate expression and update user trace
			set result [uplevel #0 [dict get $names $name expression]]
			dict with names $name {
				debug trace add $name $result -type $type -format $format
			}
		} else {
			# update context value and update user trace
			set result [expr [lindex [dict get $context $name] end] + 1]
			debug trace add $name $result
		}
		dict lappend context $name $result
		debug breakpoint create -once true -address $retaddr -command [namespace code "_exit_function {$name}"]
	}
}

proc _exit_function {name} {
	variable context
	# ignore a probable dangling breakpoint (from a previous session?)
	if {![dict exists $context $name]} { return }
	variable names
	# remove last item from context
	dict set context $name [lreplace [dict get $context $name] end end]
	# update user trace with last value
	set old_value [lindex [dict get $context $name] end]
	debug trace add $name $old_value
}

proc add {name addr {expression {}} {type int} {format dec}} {
	# add user-defined name/addr pair and create function breakpoints and traces
	variable names
	# replace old entry
	if {[dict exists $names $name]} {
		debug breakpoint remove [dict get $names $name bp]
		debug trace drop $name
	}

	variable context
	dict lappend context $name 0
	set bp [debug breakpoint create -address $addr -command [namespace code "_enter_function {$name}"]]
	dict set names $name [dict create bp $bp expression $expression type $type format $format]
	debug trace add $name 0 -type bool
}

proc remove {name} {
	variable names
	if {[dict exists $names $name]} {
		debug trace drop $name
		debug breakpoint remove [dict get $names $name bp]
		dict unset names $name
	}
}

proc list {{name {}}} {
	variable names
	if {$name ne {}} {
		if {[dict exists $names $name]} {
			return [dict get $names $name]
		}
	} else {
		return [dict keys $names]
	}
}

proc add_symbol_set {symbols} {
	# run through collection of symbols
	foreach entry $symbols {
		dict with entry {
			add $name $value
		}
	}
}

proc start {{file ""}} {
	set symbols {}
	if {$file eq ""} {
		# use current symbols only
		set symbols [debug symbols lookup]
	} else {
		# load symbol file
		debug symbols load $file
		set symbols [debug symbols lookup -filename $file]
		variable symbolfiles
		lappend symbolfiles $file
	}
	if {[llength $symbols] == 0} {
		error "no symbols found"
	}
	add_symbol_set $symbols
}

proc stop {} {
	variable context
	set context {}

	variable names
	dict for {name _} $names {
		debug trace drop $name
	}
	set names {}

	dict for {bp entry} [debug breakpoint list] {
		if {[string match "* [namespace current] *" [dict get $entry "-command"]]} {
			catch {debug breakpoint remove $bp}
		}
	}

	variable symbolfiles
	foreach file $symbolfiles {
		debug symbols remove $file
	}
	set symbolfiles {}
}

set_tabcompletion_proc symboltracer [namespace code _tab_symboltracer]

proc _tab_symboltracer {args} {
	::list "add" "list" "remove" "start" "stop"
}

} ;# namespace symboltracer
