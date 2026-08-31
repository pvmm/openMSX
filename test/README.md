# openMSX `step_back` test suite

Automated tests for the block-repeat handling in `disasm::step_back`
(`share/scripts/_disasm.tcl`).  A *block repeat instruction* is one of
`LDIR LDDR CPIR CPDR INIR INDR OTIR OTDR`.

`step_back` normally rewinds by exactly one instruction.  For a block repeat
instruction it instead rewinds to the point **before the first iteration** of
the current block execution.  This must also hold when:

  * the block has *just completed* and the instruction about to run is the one
    immediately after it (e.g. a `RET`/`JR` right after an `LDIR`), and
  * the block was *interrupted by an IRQ* and resumed, and
  * the block sits inside a loop and the same PC is executed on many passes.

These tests guard all of the above.

## Layout

    test/
      common/
        tcltest.tcl              tiny dependency-free assertion framework
        openmsx_driver.tcl       headless openMSX driver (XML stdio control)
      mock/
        step_back_mock.tcl       logic-level mock of the reverse timeline
        step_back_tests.tcl      the mock test cases (run the REAL algorithm)
        run_mock.tcl             entry point
      integration/
        step_back_integration_test.tcl  real-hardware test cases
        run_integration.tcl             entry point
      roms/
        *.asm  *.rom             MSX cartridge fixtures (16 KiB, page 1)
        build.sh                 rebuild the .rom files with SjASMPlus

## Running

Run from the repository root.

Mock suite (no openMSX needed, only `tclsh`):

    tclsh test/mock/run_mock.tcl

Integration suite (needs an openMSX binary and the `C-BIOS_MSX1` machine):

    tclsh test/integration/run_integration.tcl

Both print one `ok`/`FAIL` line per test and a final summary, and exit non-zero
if any test failed.

### Choosing the openMSX binary

`openmsx_driver.tcl` locates the binary in this order:

  1. `$OPENMSX_BIN` if set;
  2. the in-tree build `./derived/openmsx` (so you test what you just built);
  3. the first `openmsx` on `$PATH`;
  4. common install prefixes (`/opt/openmsx/bin`, `/usr/local/bin`, ...).

### Which Tcl scripts are tested

openMSX normally loads its console scripts from its installed *system data
dir*, so testing a freshly edited `share/scripts/_disasm.tcl` would silently
exercise the installed copy instead.  The driver prevents that: before
spawning openMSX it points `OPENMSX_USER_DATA` at a scratch directory
(`test/.userdata`, gitignored) that contains a `scripts` symlink to the repo's
`share/scripts`.  openMSX's `data_file` helper looks in `OPENMSX_USER_DATA`
first, so `scripts/_disasm.tcl` (and the rest of `share/scripts/`) is loaded
from the working tree.  Using a scratch dir (rather than pointing
`OPENMSX_USER_DATA` at the repo's `share/` itself) keeps openMSX's runtime
writes — `settings.xml`, `.filecache` — out of the repository.
`OPENMSX_SYSTEM_DATA` is deliberately left unset so that data the source tree
does not ship — machine definitions such as `C-BIOS_MSX1.xml` and the system
ROMs — still resolves from the installed system data dir.

## The fixtures

Each fixture is a 16 KiB MSX1 cartridge (`AB` header, `INIT` at the standard
offset) that, on reset, sets up registers and runs one block repeat
instruction, then spins in a `JR` loop.  They are built from the `.asm` sources
with SjASMPlus (`sh test/roms/build.sh`); the `.rom` files are checked in so
the tests run without an assembler.

  * `msx_60hz_16kb_ldir.rom` — `LDIR` at `0x401B`, `BC=$2000`, source `HL=$4000`,
    dest `DE=$C000`; a 60 Hz video IRQ is arranged to hit mid-`LDIR`.
  * `msx_60hz_16kb_otir.rom` — `OTIR` at `0x4019`, `B=0` (256 iterations),
    `C=$A0`; exercises the B-only counter branch.
  * `msx_60hz_16kb_ldir_loop.rom` — `LDIR` at `0x4019` inside a `DJNZ` outer
    loop (4 passes, `DI` so no IRQ); same block PC on every pass.

## What the tests assert

After arming a breakpoint and running to it, each test calls `step_back` and
asserts that the CPU lands on the block instruction with the registers restored
to their **pre-block** values (the initial, maximum counter).  For example, the
`LDIR` after-block test asserts `PC=0x401B`, `BC=$2000`, `HL=$4000`, `DE=$C000`.
