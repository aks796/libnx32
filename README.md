# libnx32

**libnx for 32-bit (AArch32) Nintendo Switch programs**

libnx built for AArch32, with the fixes 32-bit programs need to run on a real
Switch.

This is [vita2hos](https://github.com/xerpi/vita2hos)'s AArch32 port of libnx
(`vita2hos/libnx` at `721c977`), brought up to switchbrew libnx 4.12.0. On top
of it are the fixes found while running 32-bit Android games on the Switch.
Each one was tested on hardware in at least one of those ports.

The work is on the `thirtytwo` branch.

---

## What you get

* `libnx.a` and `libnxd.a` (debug) for AArch32: ARMv8-A, softfp, NEON
* The libnx headers
* `switch32.specs`, `switch32.ld` and `switch32_rules` for linking 32-bit
  programs
* `tools32/check_short_enums.sh`, which checks the headers for structs whose
  layout depends on the size of an enum

---

## How this differs from vita2hos/libnx

**Short enums.** devkitARM makes enums as small as their values
(`-fshort-enums`). The system reads the 64-bit layout, where enums are 4 bytes.
Wherever an enum reached the system, the data went out wrong:

* `hidSetSupportedNpadIdType` sent the controller list one byte per id, so
  wireless controllers could not connect. Only the handheld Joy-Cons worked.
* Enums sent as raw request data went out as 1 byte where the system reads a
  u32.
* Structs the system reads or writes had short enum fields: the software
  keyboard arguments (every field after the type was 2 bytes early), the web,
  parental control and error applets, the audio renderer, hid, btdrv, nifm, ns
  and psm.

Those fields are u32 now, and `serviceDispatchIn`/`InOut` refuse enum request
data smaller than a u32 at compile time.

**32-bit fixes:**

* `svcSetThreadCoreMask` takes a u64 mask. The kernel reads r2:r3, and r3 held
  whatever the caller left there, so every thread stayed on its first core.
* `svcGetThreadCoreMask` no longer unbalances the stack.
* New `svcWaitForAddress`, `svcSignalToAddress` and `svcGetThreadContext3`
  stubs. `svcWaitForAddress` passes the value in r2 and the timeout in r3:r4,
  the layout that worked on hardware and on Ryujinx.
* virtmem: region ends are u64, since a 32-bit ASLR region ends at
  `0x1_0000_0000`. 32-bit processes look for mapping space in the code region
  `[0x200000, 0x40000000)`, the only place the kernel accepts shared memory,
  transfer memory and code mappings.
* The heap is clamped to the 1 GiB heap region. It used to ask for about 3 GB
  and abort before `main`.
* audout and audin send their buffer descriptors in the 64-bit layout (0x28
  bytes, u64 pointers) and read released tags as u64.
* A working exception entry. It uses its own stack, saves r8-r12, d0-d31 and
  FPSCR, and calls an optional `__libnx_exception_handler32(type, info, frame)`
  that can resume the thread. See `switch/arm/exception32.h`.
* `armICacheInvalidate` works. AArch32 user code has no cache instructions, so
  it cleans the data cache and flips a spare code page's permission, which
  makes the kernel invalidate every core's instruction cache.
* `envAcquireOwnProcessHandle()` gives a real handle to the running process,
  made over a session to itself when no loader passed one.
* `nwindowGetDefaultDisplay()` returns the default window's display. vi allows
  one OpenDisplay per process, and the display gives the vsync event.
* SHA-1, SHA-256 and HMAC build for AArch32. AES and CMAC are still AArch64
  only.
* fsdev maps "file open for writing elsewhere" (FS 2-0007) to `EBUSY`.
* A weak C11 `timespec_get`. newlib declares it but never implements it, and
  Mesa's threads need it.
* `switch32.ld` places `.rel.dyn` and `.rel.plt`. ARM relocations are
  `SHT_REL`, not `SHT_RELA`.

It also has switchbrew libnx master merged in, up to 4.12.0.

---

## Requirements

* Docker
* The AArch32 toolchain image `ghcr.io/vita2hos/devcontainer/vita2hos`
  (devkitARM with the Switch multilibs, and a stock libnx32)

---

## Building

```bash
./build.sh
```

This builds in the toolchain image and installs into `prefix/`:

```text
prefix/
├── include/
├── lib/
│   ├── libnx.a
│   └── libnxd.a
├── switch32.ld
├── switch32.specs
└── switch32_rules
```

To build inside the container yourself:

```bash
docker run --rm --platform linux/amd64 -v "$PWD:/work" -w /work \
  ghcr.io/vita2hos/devcontainer/vita2hos:latest bash -lc ./build_libnx32.sh
```

To check the struct layouts after changing a header:

```bash
docker run --rm --platform linux/amd64 -v "$PWD:/w" -w /w \
  ghcr.io/vita2hos/devcontainer/vita2hos:latest bash -lc tools32/check_short_enums.sh
```

---

## Using it

The toolchain image already has a stock libnx32 in `/opt/devkitpro/libnx32`,
next to other libraries such as miniz and deko3d. Mount this build's files
over the stock ones rather than replacing the whole folder:

```bash
NX=/opt/devkitpro/libnx32
docker run --rm --platform linux/amd64 \
  -v "$PWD:/work" \
  -v "/path/to/libnx32/prefix/include/switch:$NX/include/switch:ro" \
  -v "/path/to/libnx32/prefix/include/switch.h:$NX/include/switch.h:ro" \
  -v "/path/to/libnx32/prefix/lib/libnx.a:$NX/lib/libnx.a:ro" \
  -v "/path/to/libnx32/prefix/lib/libnxd.a:$NX/lib/libnxd.a:ro" \
  -v "/path/to/libnx32/prefix/switch32.ld:$NX/switch32.ld:ro" \
  -w /work ghcr.io/vita2hos/devcontainer/vita2hos:latest bash -lc "make"
```

Compile everything that links against it with:

```text
-march=armv8-a+crc+crypto -mtune=cortex-a57 -mfloat-abi=softfp
-mfpu=neon-fp-armv8 -mtp=soft -fPIE -ftls-model=local-exec
```

and link with `-specs=$DEVKITPRO/libnx32/switch32.specs`.

* **softfp**: floats and doubles are passed in integer registers, and VFP and
  NEON are still used. This matches Android's armeabi-v7a, so the libraries
  can call and be called by 32-bit Android `.so` files directly.
* **Enums are short.** devkitARM uses `-fshort-enums`, and this libnx and the
  32-bit Mesa are built that way. Keep your code the same, except code that
  includes FFmpeg's headers, which needs `-fno-short-enums`.

### Running a 32-bit program

A 32-bit program is an NSO with a `main.npdm` whose `is_64_bit` is `false` and
`address_space_type` is `0` (32-bit), packed into an ExeFS NSP (`main` and
`main.npdm`). Two ways to start one with Atmosphère:

* **An ExeFS override for a title.** Put the NSP at
  `sd:/atmosphere/contents/<title id>/exefs.nsp`. The NPDM's program id must
  match the title. A sphaira forwarder is a good title to use: it gets its own
  icon on the HOME menu, and deleting `exefs.nsp` undoes it.
* **An hbl override.** In `sd:/atmosphere/config/override_config.ini`:

  ```ini
  [hbl_config]
  override_any_app=true
  override_any_app_key=R
  override_any_app_address_space=32_bit
  path=your_program.nsp
  ```

  Then hold R while starting any game. This replaces the Homebrew Menu for
  every title override, so remove it when you are done.

Both run with full application memory. A 32-bit address space has a 1 GiB
heap region.

---

## Known issues

* **Text relocations.** devkitARM's target libraries (newlib's libc and libm,
  libsysbase, libstdc++) are not built `-fPIC`. Their literal pools hold
  absolute addresses, so a PIE link has `R_ARM_RELATIVE` relocations in
  `.text` and `.rodata`, and crt0's `__nx_dynamic` cannot apply them. Link with
  `-z notext` and apply them through a writable alias of each code block. Do not
  make the code writable: on Atmosphère a code page made writable can never
  become executable again. The 32-bit ports carry a relocator for this
  (`crt0_reloc.c` with their own linker script and specs). Building the target
  libraries `-fPIC` would remove the need for it.
* **newlib is soft-float.** libm does every `double` operation through libgcc
  calls, which is correct but slow. `setjmp` does not save d8-d15, which the
  ARM calling convention makes callee-saved for VFP code.
* **miniz's `inflate` is greedy.** The image's miniz consumes all of its input,
  including the Adler-32 trailer, while output is still owed. libpng then fails
  with "Not enough image data". Give one consumed byte back when the output is
  full and the input is empty, or use a real zlib.
* **`__appInit` aborts on any service failure.** Under Ryujinx the time
  service's shared memory fails to map for 32-bit processes while everything
  else works. Override `__appInit` to report failures if you test on an
  emulator.
* **`exit()` shuts libnx's services down while other threads still run.** A
  thread that polls hid after that makes libnx abort.

---

## Upstream

* vita2hos libnx: [github.com/vita2hos/libnx](https://github.com/vita2hos/libnx)
* switchbrew libnx: [github.com/switchbrew/libnx](https://github.com/switchbrew/libnx)

Problems that are not specific to AArch32 belong upstream. For setting up
devkitPro and libnx in general, see
[Switchbrew](https://switchbrew.org/wiki/Setting_up_Development_Environment).

---

## Credits

**libnx32 fixes**: aks796

The AArch32 port of libnx and the toolchain image are by
[xerpi](https://github.com/xerpi), from
[vita2hos](https://github.com/xerpi/vita2hos).

libnx is by the switchbrew libnx authors, and is based on libctru.

---

## License

ISC, like libnx. See [LICENSE.md](LICENSE.md).
