#!/bin/bash
# check_short_enums.sh -- which structs in the libnx headers change layout with
# the size of an enum. devkitARM (AArch32) makes enums as small as their
# values (-fshort-enums); the system's data (IPC, applet storage, shared
# memory) has the 64-bit layout, where they are 4 bytes. A struct listed here
# must be client-side only; one the system reads or writes needs its enum
# fields as u32. Every struct's size and top-level field offsets are compiled
# both ways and compared. Run inside the toolchain container from the repo root:
#   docker run --rm --platform linux/amd64 -v "$PWD:/w" -w /w \
#     ghcr.io/vita2hos/devcontainer/vita2hos:latest bash -lc tools32/check_short_enums.sh
set -euo pipefail
cd "$(dirname "$0")/.."
T=$(mktemp -d)
python3 - "$T/audit.c" <<'PY'
import re, glob, sys
def structs(s):
    for m in re.finditer(r'typedef\s+struct\s*(\w+)?\s*\{', s):
        i=m.end(); d=1; j=i
        while d and j<len(s):
            d += (s[j]=='{') - (s[j]=='}'); j+=1
        body=s[i:j-1]
        nm=re.match(r'\s*(?:NX_PACKED\s*)?(\w+)\s*;', s[j:j+200])
        if not nm: continue
        top=''; d=0
        for ch in body:
            if ch=='{': d+=1
            elif ch=='}': d-=1
            elif d==0: top+=ch
        yield nm.group(1), re.findall(r'^\s*(?:const\s+|volatile\s+)?\w+\s*\**\s*(\w+)\s*(?:\[[^\]]*\])*\s*;', top, re.M)
out=['#include <switch.h>','#include <stddef.h>']; seen=set()
for h in sorted(glob.glob('nx/include/**/*.h', recursive=True)):
    for name, fields in structs(open(h, errors='ignore').read()):
        if name in seen: continue
        seen.add(name)
        out.append(f'char SZ__{name}[sizeof({name})];')
        out += [f'char OF__{name}__{f}[offsetof({name}, {f})+1];' for f in fields]
open(sys.argv[1],'w').write('\n'.join(out)+'\n')
PY
CC="$DEVKITPRO/devkitARM/bin/arm-none-eabi-gcc -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -D__SWITCH__ -fcommon -Inx/include"
for i in $(seq 1 20); do  # drop the generated lines the simple parser got wrong
  $CC -fshort-enums -c "$T/audit.c" -o "$T/a.o" 2>"$T/err" && break
  for n in $(grep -o 'audit.c:[0-9]*' "$T/err" | cut -d: -f2 | sort -rnu); do [ "$n" -gt 2 ] && sed -i "${n}s/.*//" "$T/audit.c"; done
done
for e in short no-short; do
  $CC -f$e-enums -c "$T/audit.c" -o "$T/$e.o"
  "$DEVKITPRO/devkitARM/bin/arm-none-eabi-nm" -S "$T/$e.o" | awk '{print $4, $2}' | sort > "$T/$e.txt"
done
{ diff "$T/short.txt" "$T/no-short.txt" || true; } | grep '^<' | sed -E 's/^< (SZ|OF)__//; s/__.* / /; s/ .*//' | sort | uniq -c | sort -rn
echo "(client-side only, as of the thirtytwo fixes: SwkbdInline Framebuffer SfOutHandleAttrs NvMap TipcDispatchParams SfDispatchParams RingConUserCal RingCon AppletHolder)"
rm -rf "$T"
