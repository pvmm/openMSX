ext programmabledevice

variable f2status COLD

proc _reset {} {
	puts stderr "RESET detected"
	variable f2status COLD
}

proc _read {port} {
	variable f2status 
	set port [expr $port & 0xff]
	puts stderr "Data requested from 0x[format %02X $port]"
	switch -- $port {
		242 {
			switch -- $f2status {
				COLD {  ;# cold: first response
					puts stderr "/COLD/ send first response FF"
					return 255
				}
				WARM {  ;# warm: bla
					puts stderr "/WARM/ send next message: FF"
					return 255
				}
			}
		}
		default {
			puts stderr "read port=$port"
		}
	}
	return 0
}

proc _write {port value} {
	variable f2status 
	set port [expr $port & 0xff]
	puts stderr "Received byte 0x[format %02X $value] from port 0x[format %02X $port]"
	switch -- $port {
		6 {
			switch -- $value {
				20 { ;# clear UART buffer
					puts stderr "Received command to clear UART buffer"
				}
			}
		}
		7 {
			switch -- $value {
				99 { ;# CMD_QUERY_ACLK_SETTINGS
					puts stderr "Received CMD_QUERY_ACLK_SETTINGS"
				}
			}
		}
		242 {
			switch -- $f2status {
				COLD {  ;# cold: receive confirmation message
					if {$value eq 0} {
						puts stderr "/COLD/ got confirmation message 00"
						set f2status WARM
						puts stderr "/COLD/ setting status=WARM"
					}
				}
				WARM {  ;# warm:
					if {$value eq 255} {
						set f2status NEXT
						puts stderr "/WARM/ setting status=NEXT"
					}
				}
				NEXT { ;# bla
					if {$value eq 254} {
						set f2status NEX2
						puts stderr "/NEXT/ setting status=NEX2"
					}
				}
			}
		}
		default {
			puts stderr "write port=$port, value=$value"
		}
	}
}

proc start {} {
	variable {Programmable Device port layout} {6 7 242}
	variable {Programmable Device reset callback} "_reset"
	variable {Programmable Device read callback}  "_read"
	variable {Programmable Device write callback} "_write"
	puts "Start testing..."
	#debug set_watchpoint read_io {6 7}
	#debug set_watchpoint write_io {6 7}
}

start
