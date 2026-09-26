namespace eval disasm {

# very common debug functions

proc peek {addr {m memory}} {
	debug read $m $addr
}
proc peek8 {addr {m memory}} {
	peek $addr $m
}
proc peek_u8 {addr {m memory}} {
	peek $addr $m
}
proc peek_s8 {addr {m memory}} {
	set b [peek $addr $m]
	expr {($b < 128) ? $b : ($b - 256)}
}
proc peek16 {addr {m memory}} {
	expr {[peek $addr $m] + 256 * [peek [expr {$addr + 1}] $m]}
}
proc peek16_LE {addr {m memory}} {
	peek16 $addr $m
}
proc peek16_BE {addr {m memory}} {
	expr {256 * [peek $addr $m] + [peek [expr {$addr + 1}] $m]}
}
proc peek_u16 {addr {m memory}} {
	peek16 $addr $m
}
proc peek_u16LE {addr {m memory}} {
	peek16 $addr $m
}
proc peek_u16BE {addr {m memory}} {
	peek16_BE $addr $m
}
proc peek_s16 {addr {m memory}} {
	set w [peek16 $addr $m]
	expr {($w < 32768) ? $w : ($w - 65536)}
}
proc peek_s16LE {addr {m memory}} {
	peek_s16 $addr $m
}
proc peek_s16BE {addr {m memory}} {
	set w [peek16_BE $addr $m]
	expr {($w < 32768) ? $w : ($w - 65536)}
}

set help_text_peek \
{Read a byte or word from the given memory location.
Optionally allows to specify a different 'debuggable' (default is 'memory').

usage:
  peek        <addr> [<mem>] Read unsigned 8-bit value from address
  peek8       <addr> [<mem>]      unsigned 8-bit
  peek_u8     <addr> [<mem>]      unsigned 8-bit
  peek_s8     <addr> [<mem>]        signed 8-bit
  peek16      <addr> [<mem>]      unsigned 16-bit little endian
  peek16_LE   <addr> [<mem>]      unsigned 16-bit little endian
  peek16_BE   <addr> [<mem>]      unsigned 16-bit big    endian
  peek_u16    <addr> [<mem>]      unsigned 16-bit little endian
  peek_u16_LE <addr> [<mem>]      unsigned 16-bit little endian
  peek_u16_BE <addr> [<mem>]      unsigned 16-bit big    endian
  peek_s16    <addr> [<mem>]        signed 16-bit little endian
  peek_s16_LE <addr> [<mem>]        signed 16-bit little endian
  peek_s16_BE <addr> [<mem>]        signed 16-bit big    endian
}
set_help_text peek        $help_text_peek
set_help_text peek8       $help_text_peek
set_help_text peek_u8     $help_text_peek
set_help_text peek_s8     $help_text_peek
set_help_text peek16      $help_text_peek
set_help_text peek16_LE   $help_text_peek
set_help_text peek16_BE   $help_text_peek
set_help_text peek_u16    $help_text_peek
set_help_text peek_u16_LE $help_text_peek
set_help_text peek_u16_BE $help_text_peek
set_help_text peek_s16    $help_text_peek
set_help_text peek_s16_LE $help_text_peek
set_help_text peek_s16_BE $help_text_peek


proc poke {addr val {m memory}} {
	debug write $m $addr $val
}
proc poke8 {addr val {m memory}} {
	poke $addr $val $m
}
proc poke16 {addr val {m memory}} {
	poke        $addr       [expr { $val       & 255}] $m
	poke [expr {$addr + 1}] [expr {($val >> 8) & 255}] $m
}
proc poke16_LE {addr val {m memory}} {
	poke16 $addr $val $m
}
proc poke16_BE {addr val {m memory}} {
	 poke        $addr       [expr {($val >> 8) & 255}] $m
	 poke [expr {$addr + 1}] [expr { $val       & 255}] $m
}
set help_text_poke \
{Write a byte or word to the given memory location.
Optionally allows to specify a different 'debuggable' (default is 'memory').

usage:
  poke      <addr> <val> [<mem>]   Write 8-bit value
  poke8     <addr> <val> [<mem>]         8-bit
  poke16    <addr> <val> [<mem>]        16-bit little endian
  poke16_LE <addr> <val> [<mem>]        16-bit little endian
  poke16_BE <addr> <val> [<mem>]        16-bit big    endian
}
set_help_text poke      $help_text_poke
set_help_text poke8     $help_text_poke
set_help_text poke16    $help_text_poke
set_help_text poke16_LE $help_text_poke
set_help_text poke16_BE $help_text_poke


# because of reverse we can now save replays to a file,
# poke-ing adds an entry into the replay file and therefore
# the file size can grow significantly. Therefor dpoke (poke
# if different or diffpoke) is introduced.
proc dpoke {addr val {m memory}} {
	if {[peek $addr $m] != $val} {poke $addr $val $m}
}


#
# disasm
#
set_help_text disasm \
{Disassemble z80 instructions

Usage:
  disasm                Disassemble 8 instr starting at the currect PC
  disasm <addr>         Disassemble 8 instr starting at address <adr>
  disasm <addr> <num>   Disassemble <num> instr starting at address <addr>
}
proc disasm {{address -1} {num 8}} {
	if {$address == -1} {set address [reg PC]}
	for {set i 0} {$i < int($num)} {incr i} {
		set l [debug disasm $address]
		append result [format "%04X  %s\n" $address [join $l]]
		set address [expr {($address + [llength $l] - 1) & 0xFFFF}]
	}
	return $result
}


#
# run_to
#
set_help_text run_to \
{Run to the specified address, if a breakpoint is reached earlier we stop
at that breakpoint.}
proc run_to {address} {
	debug set_bp -once $address
	debug cont
}


#
# toggle_breaked
#
set_help_text toggle_breaked \
{Toggles breaked status.}
proc toggle_breaked {} {
	if ([debug breaked]) {
		debug cont
	} else {
		debug break
	}
}


#
# step_in
#
set_help_text step_in \
{Step in. Execute the next instruction, also go into subroutines.}
proc step_in {} {
	debug step
}
set_help_text step \
{Same as step_in.}
proc step {} {
	debug step
}


#
# step_out
#
set_help_text step_out \
{Step out of the current subroutine. In other words, execute till right after
the next 'ret' instruction (more if there were also extra 'call' instructions).
Note: simulation can be slow during execution of 'step_out', though for not
extremely large subroutines this is not a problem.}

variable step_out_bp1
variable step_out_bp2

proc step_out_is_ret {} {
	# ret        0xC9
	# ret <cc>   0xC0,0xC8,0xD0,..,0xF8
	# reti retn  0xED + 0x45,0x4D,0x55,..,0x7D
	set instr [peek16 [reg pc]]
	expr {(($instr & 0x00FF) == 0x00C9) ||
	      (($instr & 0x00C7) == 0x00C0) ||
	      (($instr & 0xC7FF) == 0x45ED)}
}
proc step_out_after_break {} {
	variable step_out_bp1
	variable step_out_bp2

	# also clean up when breaked, but not because of step_out
	catch {debug remove_condition $step_out_bp1}
	catch {debug remove_condition $step_out_bp2}
}
proc step_out_after_next {} {
	variable step_out_bp1
	variable step_out_bp2
	variable step_out_sp

	catch {debug remove_condition $step_out_bp2}
	if {[reg sp] > $step_out_sp} {
		catch {debug remove_condition $step_out_bp1}
		debug break
	}
}
proc step_out_after_ret {} {
	variable step_out_bp2

	catch {debug remove_condition $step_out_bp2}
	set step_out_bp2 [debug set_condition 1 [namespace code step_out_after_next]]
}
proc step_out {} {
	variable step_out_bp1
	variable step_out_bp2
	variable step_out_sp

	catch {debug remove_condition $step_out_bp1}
	catch {debug remove_condition $step_out_bp2}
	set step_out_sp [reg sp]
	set step_out_bp1 [debug set_condition {[disasm::step_out_is_ret]} [namespace code step_out_after_ret]]
	after break [namespace code step_out_after_break]
	debug cont
}


#
# step_over
#
set_help_text step_over \
{Step over. Execute the next instruction but don't step into subroutines.
Only 'call' or 'rst' instructions are stepped over. Note: 'push xx / jp nn'
sequences can in theory also be used as calls but these are not skipped
by this command.}
proc step_over {} {
	set address [reg PC]
	set l [debug disasm $address]
	set instr [lindex $l 0]
	if {[string match "call*" $instr] ||
	    [string match "rst*"  $instr] ||
	    [string match "ldir*" $instr] ||
	    [string match "cpir*" $instr] ||
	    [string match "inir*" $instr] ||
	    [string match "otir*" $instr] ||
	    [string match "lddr*" $instr] ||
	    [string match "cpdr*" $instr] ||
	    [string match "indr*" $instr] ||
	    [string match "otdr*" $instr] ||
	    [string match "halt*" $instr]} {
		run_to [expr {$address + [llength $l] - 1}]
	} else {
		debug step
	}
}


#
# is_block_repeat
#
# Predicate: is this disassembled mnemonic one of the 8 Z80 block-repeat
# instructions (LDIR/LDDR/CPIR/CPDR/INIR/INDR/OTIR/OTDR)?
#
# 'instr' is the first word of 'debug disasm <addr>' output, e.g. "ldir".
# The trailing "*" allows optional operand suffixes / flag annotations, so
# "ldir", "ldir (extra info)" etc. all match. Matching is intentionally
# lowercase-only because 'debug disasm' always returns lowercase mnemonics;
# plain single-step opcodes (ldi, ldd, cpi, cpd, ini, ind, outi, outd, ...)
# must NOT match -- only the repeating forms listed above.
proc is_block_repeat {instr} {
	expr {[string match "ldir*" $instr] || [string match "lddr*" $instr] ||
	      [string match "cpir*" $instr] || [string match "cpdr*" $instr] ||
	      [string match "inir*" $instr] || [string match "indr*" $instr] ||
	      [string match "otir*" $instr] || [string match "otdr*" $instr]}
}


#
# step_back
#
# Reverse-step one instruction: rewind the replay timeline to the instruction
# boundary just before the instruction that brought us to the current moment.
#
# Mental model: the reverse system records a totally ordered timeline of
# instruction boundaries, each with a timestamp and full machine state (PC,
# BC, ...). "Current time" always sits exactly ON such a boundary, meaning
# "[reg PC] / debug disasm [reg PC]" describes the instruction ABOUT TO
# execute. 'reverse goto T' lands on the first boundary with time >= T
# (rounding forward to a complete instruction); 'reverse goback D' lands on
# the last boundary with time <= now-D. Forward re-execution ('goto' to a
# slightly later time) is orders of magnitude faster than re-executing
# backwards, hence the "big step back, then small steps forward" pattern.
#
# Historical note: an older implementation stepped backwards repeatedly until
# PC changed. On R800 that could take 80+ slow backwards steps. The current
# design uses exactly 2 backwards moves (one big jump + one final placement)
# plus many cheap forward steps, which is much faster in the worst case.
set_help_text step_back \
{Step back. Go back in time till right before the last instruction was
executed. Note that this operation is relatively slow (compared to the other
step functions). Also the reverse feature must be enabled for this to work
(normally it's enabled by default).

When the current instruction is a block repeat instruction (LDIR, LDDR,
CPIR, CPDR, INIR, INDR, OTIR, OTDR), step_back will rewind to before the
entire block instruction started (i.e. to the point before the first
iteration), rather than just going back one iteration. This is also the
case when the block sequence was interrupted by an IRQ: step_back still
rewinds to the first iteration of the block execution.}
proc step_back {} {
	# Phase 0 -- timing constants.
	# 'z80' or 'r800'
	set cpu [get_active_cpu]

	# Duration of one CPU cycle in emulated seconds. Used as the smallest
	# useful forward step: advancing by one cycle guarantees we cannot skip
	# over any instruction boundary, because every instruction takes at
	# least one cycle. So repeated 'reverse goto [curr + cycle_period]'
	# visits every boundary in order, one (or occasionally more, see below)
	# at a time.
	set cycle_period [expr {1.0 / [machine_info ${cpu}_freq]}]

	# (Overestimation) for the maximum instruction length.
	#  On Z80 the slowest instruction is probably 'EX (SP),IX' (25 cycles).
	#  On R800 it's probably some I/O instruction to the VDP, followed by
	#  a memory refresh (up to 87(!) cycles). I added some extra cycles as
	#  a safety margin in case I forgot some extra penalty cycles (e.g.
	#  access to a device that inserts extra wait cycles).
	# Going back by this amount from any boundary is therefore guaranteed
	# to land strictly before the instruction that ends at the start time.
	set max_instr_len [expr {(($cpu eq "z80") ? 35 : 100) * $cycle_period}]

	# Phase 1 -- plain single-step-back.
	# Timestamp of the boundary we start from ("start instruction" = the
	# instruction that was about to execute when step_back was called).
	set start [dict get [reverse status] "current"]

	# Go back till a moment that's certainly before the start instruction.
	# By construction of max_instr_len, 'curr' now predates the previous
	# instruction boundary. If not, our overestimation was wrong: abort
	# rather than silently landing in the wrong place.
	reverse goback -novideo $max_instr_len
	set curr [dict get [reverse status] "current"]
	if {$curr >= $start} {
		# The user probably activated the reverse feature after the
		# instruction of interest was accounted for.
		error "Internal error: initial step-back was not big enough"
	}

	# Take small steps (forward) till we again reach the start instruction.
	# Times are compared so we know for sure that the start instruction is
	# exactly the instruction were we previously stopped.
	while {1} {
		# Note that 'reverse goto' for a small forward step is
		# orders of magnitudes faster than a backwards 'reverse goto'.
		# The '-novideo' flag is required to not (temporarily
		# internally) step back a few video frames (so that immediately
		# after 'reverse goto' we have the correct video output).
		# Also note that this may take a bigger step forward than
		# requested: it will only stop after a complete instruction is
		# emulated. So one 'goto curr+cycle' advances exactly one
		# instruction boundary in the common case.
		reverse goto -novideo [expr {$curr + $cycle_period}]
		set next [dict get [reverse status] "current"]
		if {$next >= $start} {
			# Check for '$next >= $start' (instead of $next == $start).
			# I/O is emulated with sub-instruction precision, it's
			# possible to call this script from a watchpoint-callback
			# (which triggers in the middle of an instruction). However
			# 'reverse goto' always stops at instruction boundaries. So
			# the combination of this may cause this algorithm to
			# overshoot the destination timestamp, hence the '>='.
			break
		}
		set curr $next
	}

	# The previous step was the correct one, so go back there.
	# At this point: 'curr' = boundary just before the start instruction,
	# 'next' = start boundary (or the first boundary past it on overshoot).
	# Note that (only here) we don't pass the '-novideo' flag, so the
	# visible screen is refreshed to the final destination state instead
	# of being left at a fast-path stale frame.
	reverse goto $curr

	# Phase 2 -- did we just step over (part of) a block-repeat?
	# Check if the instruction that is about to execute at this boundary is
	# a block repeat instruction. This covers two cases:
	#  * the current instruction IS the block: we just went back one
	#    iteration, so [reg PC] still points to the block (e.g. stopped
	#    mid-LDIR, one iteration back is still LDIR);
	#  * the current instruction immediately follows a block (e.g. a RET
	#    right after an LDIR): this boundary is the end of the block
	#    execution, so [reg PC] points to the block that just finished.
	#    Without this second case, stopping on the RET after a finished
	#    LDIR would step back onto the tail of the LDIR instead of before
	#    its first iteration (the original bug fixed in 9255c286a).
	# In both cases we rewind to before the first iteration of the block
	# instruction instead of stopping at the end of the block. A single
	# LDIR can execute thousands of iterations (e.g. BC=$2000), so going
	# back just one iteration would cause surprise; users expect to undo
	# the whole block at once, symmetric with step_over which skips it.
	if {[is_block_repeat [lindex [debug disasm [reg PC]] 0]]} {
		# Phase 3 -- jump to somewhere before the whole block run.
		# Measure the time per iteration of this block instruction:
		# 'start' is the time we came from, 'time_after_first' is where
		# Phase 1 left us (exactly one iteration earlier, or -- in the
		# after-block case -- the block's end boundary). Their difference
		# is therefore one iteration's duration.
		set current_addr [reg PC]
		set time_after_first [dict get [reverse status] "current"]
		set time_per_iter [expr {$start - $time_after_first}]

		# Safety: ensure we always make at least a tiny progress.
		# Normally time_per_iter > 0, but clamp against a zero/negative
		# measurement (e.g. mid-instruction watchpoint overshoot) so the
		# exponential search below cannot stall with a zero stride.
		if {$time_per_iter < $cycle_period} {
			set time_per_iter $cycle_period
		}

		# Go back past all iterations of the block instruction using
		# exponential backoff (O(log N) reverse goback calls instead
		# of O(N)). We double the goback amount each time until PC
		# no longer points to the block instruction address.
		# Starting at 8 iterations and doubling means an N-iteration
		# block needs only ~log2(N/8)+1 slow backwards jumps however
		# large N is. The loop provably terminates: each jump goes
		# further back than the last, and the block run is finite, so
		# eventually PC != current_addr. (Interrupts inside the block
		# also satisfy PC != current_addr, which terminates the loop
		# early -- harmless, Phase 4 scans forward past them anyway.)
		set goback [expr {$time_per_iter * 8}]
		while {[reg PC] == $current_addr} {
			# Guard against running off the available replay history:
			# if 'reverse goback' cannot move (emulator time frozen),
			# there is no earlier state to examine, so fail loudly
			# instead of looping forever (fix from 710889eae for
			# "reverse start called mid-block", where no pre-block
			# history exists yet).
			set check_time_lock [machine_info time]
			reverse goback -novideo $goback
			if {$check_time_lock == [machine_info time]} {
				error "Internal error: reverse system record unavailable for the time frame"
			}
			set goback [expr {$goback * 2}]
		}

		# Phase 4 -- scan forward to the first iteration of THIS block run.
		# We are now guaranteed somewhere strictly before the block run
		# (Phase 3 exited only when PC left current_addr). Nudge one more
		# instruction back so the forward scan starts before -- not inside
		# -- the earliest candidate boundary, then walk forward boundary by
		# boundary until the original 'start' time is reached.
		#
		# Why PC-matching alone is insufficient: the same block address can
		# appear many times in the scanned window, but only one occurrence
		# is the start of the CURRENT execution:
		#  * Loop: 'LD BC,n / LDIR / DJNZ' re-executes the same LDIR on
		#    every outer-loop pass. All passes share current_addr.
		#  * IRQ: the CPU suspends the LDIR, runs the handler at 0x0038
		#    (PC != current_addr), then resumes the LDIR with a SMALLER
		#    counter. The timeline therefore holds several disjoint "runs"
		#    of consecutive current_addr boundaries for one logical block.
		#
		# Disambiguation key: the block counter strictly decreases as the
		# block runs, so a FRESH block entry always carries the initial
		# (maximum) counter, while resume/continuation boundaries carry
		# lower values. The counter is the BC pair for LDIR/LDDR/CPIR/CPDR
		# and the B register alone (C = fixed I/O port) for
		# INIR/INDR/OTIR/OTDR; either way B is decremented per iteration,
		# so comparing the whole [reg BC] works uniformly (C is constant
		# for I/O blocks, hence BC ordering == B ordering). Therefore: a
		# run start whose BC >= every BC seen before in this scan is the
		# start of a new block execution, and the LAST such qualifying run
		# before 'start' is the first iteration of the current execution
		# (earlier passes are superseded, IRQ-resumed runs are rejected
		# for their lower counter).
		reverse goback -novideo $max_instr_len
		set curr [dict get [reverse status] "current"]
		set cand -1
		set max_bc -1
		set inrun 0
		while {1} {
			reverse goto -novideo [expr {$curr + $cycle_period}]
			set next [dict get [reverse status] "current"]
			# 'match' = this boundary is about to execute the block.
			# 'inrun' remembers whether the PREVIOUS boundary also
			# matched, so '!inrun && match' fires exactly once per
			# contiguous run -- i.e. on run starts only, not on every
			# iteration inside a run.
			set match [expr {[reg PC] == $current_addr}]
			if {$match && !$inrun} {
				# New run of block iterations starts here; keep it as
				# a candidate only if the block counter is fresh, i.e.
				# if this is the start of a (new) block execution.
				#
				# Note: record 'next' (the current boundary), not 'curr'
				# (the boundary we advanced from). The block counter is
				# loaded by the instruction immediately preceding the
				# block (e.g. 'LD BC,<max>' just before an LDIR), so the
				# first boundary where PC matches the block already has
				# the initial counter value; that is the correct landing
				# point. Recording 'curr' would land one instruction too
				# early (before the counter is loaded). This was the
				# second half of the 9255c286a fix.
				set bc [reg BC]
				if {$bc >= $max_bc} {
					set cand $next
					set max_bc $bc
				}
			}
			set inrun $match
			if {$next >= $start} {
				# Time check: the original start time is reached.
				# A PC match at an earlier time is not sufficient: the
				# loop ends only when the scan window covers 'start',
				# which guarantees 'cand' (if any) belongs to the
				# current execution rather than an older pass.
				if {!$match && $cand < 0} {
					# Defensive fallback: start time hit in the middle
					# of a non-block instruction (e.g. when called from
					# a watchpoint callback, where 'start' is
					# sub-instruction) and no block run was ever seen.
					# Land on the last boundary before the start time,
					# i.e. plain single-step-back behaviour.
					set cand $curr
				}
				break
			}
			set curr $next
		}
		# Ultimate fallback (should be unreachable when a block run was
		# found, but keeps the proc total): stay where Phase 1 left us.
		if {$cand < 0} { set cand $curr }
		# Note that (only here) we don't pass the '-novideo' flag, so the
		# final landing refreshes the display like Phase 1 does.
		reverse goto $cand
	}
}

#
# skip one instruction
#
set_help_text skip_instruction \
{Skip the current instruction. In other words increase the program counter with the length of the current instruction.}
proc skip_instruction {} {
	set pc [reg pc]
	reg pc [expr {$pc + [llength [debug disasm $pc]] - 1}]
}

namespace export peek
namespace export peek8
namespace export peek_u8
namespace export peek_s8
namespace export peek16
namespace export peek16_LE
namespace export peek16_BE
namespace export peek_u16
namespace export peek_u16_LE
namespace export peek_u16_BE
namespace export peek_s16
namespace export peek_s16_LE
namespace export peek_s16_BE
namespace export poke
namespace export poke8
namespace export poke16
namespace export poke16_LE
namespace export poke16_BE
namespace export dpoke
namespace export disasm
namespace export run_to
namespace export toggle_breaked
namespace export step_over
namespace export step_back
namespace export step_out
namespace export step_in
namespace export step
namespace export skip_instruction

} ;# namespace disasm

namespace import disasm::*
