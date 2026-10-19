#!/usr/bin/env bash
# QEMU smoke test: build, stage the test ESP, boot in AArch64 QEMU.
#   1. no input: the countdown boots the default entry, which checks every
#      hand-over (DTB patches + overlay, /chosen, injected kit driver, initrd
#      media, variable) and powers off;
#   2. keyboard: Down + Enter picks the second entry;
#   3. degraded: no display device and no config file, so the built-in
#      configuration runs on the text console and its default (Return to
#      firmware) fires when the timer ends;
#   4. block slice: UniXNU on a bare ESP finds its config on a FAT volume
#      at the start of a GPT partition named "userdata" and publishes the
#      rest of that partition as its own block device (needs mtools, or a
#      python with pyfatfs in FAT_PYTHON; skipped otherwise).
# Logs and screenshots go to Build/Smoke/.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TARGET=${TARGET:-DEBUG}
OUT=$ROOT/Build/Out/AARCH64-$TARGET
SMOKE=$ROOT/Build/Smoke
ESP=$SMOKE/esp
KEXT_TIMEOUT=${KEXT_TIMEOUT:-60}

"$ROOT/build.sh" -b "$TARGET" >/dev/null
mkdir -p "$SMOKE"

python3 "$ROOT/Scripts/stage-esp.py" -o "$ESP" "$ROOT/CrabApplePkg/Test/Qemu/esp" \
  --put "$OUT/CrabApple.efi=EFI/BOOT/BOOTAA64.EFI" \
  --put "$OUT/HelloPayload.efi=EFI/CrabApple/Payloads/HelloPayload.efi" \
  --put "$OUT/SampleDriver.efi=EFI/CrabApple/Kits/SampleDriver.kit/SampleDriver.efi"

echo "== scenario 1: countdown boots the default entry"
python3 "$ROOT/Scripts/qemu-run.py" "$ESP" \
  --log "$SMOKE/default.log" --screenshot "$SMOKE/menu.png" \
  --expect 'CRABAPPLE: UniXNU [0-9.]+ starting as application' \
  --expect 'CRABAPPLE: config: loaded' \
  --expect 'CRABAPPLE: kit SampleDriver 1\.0\.0: loaded' \
  --expect 'CRABAPPLE: kit NeverMatch 0\.0\.1: not loaded' \
  --expect 'CRABAPPLE: driver .*SampleDriver\.efi \(kit SampleDriver\): started' \
  --expect 'CRABAPPLE: ui: graphics' \
  --expect 'CRABAPPLE: menu: shown 4 entries, default 0' \
  --expect 'CRABAPPLE: menu: timeout, booting default 0' \
  --expect 'CRABAPPLE: dt: installed' \
  --expect "CRABAPPLE-HELLO: check dtb PASS \(bootargs='console=ttyAMA0 crabapple.test=1' marker='patched'\)" \
  --expect 'CRABAPPLE-HELLO: check overlay PASS' \
  --expect "CRABAPPLE-HELLO: check driver PASS \(sample driver args='from-kit'\)" \
  --expect 'CRABAPPLE-HELLO: check initrd PASS .*CRABAPPLE-INITRD-OK' \
  --expect "CRABAPPLE-HELLO: check var PASS \(CrabAppleTest='hello-variable'" \
  --expect 'CRABAPPLE-HELLO: RESULT PASS 5 passed 0 failed' \
  --reject 'CRABAPPLE: config: .*(error|needs|unknown)' \
  --reject 'ASSERT'

echo "== scenario 2: keyboard selects the second entry"
python3 "$ROOT/Scripts/qemu-run.py" "$ESP" \
  --log "$SMOKE/keys.log" --keys down,ret \
  --expect 'CRABAPPLE: menu: enter, booting 1' \
  --expect "CRABAPPLE-HELLO: load options 'plain shutdown'" \
  --expect 'CRABAPPLE-HELLO: RESULT PASS 0 passed 0 failed' \
  --reject 'CRABAPPLE: menu: timeout' \
  --reject 'ASSERT'

echo "== scenario 3: no GOP, no config file"
python3 "$ROOT/Scripts/stage-esp.py" -o "$SMOKE/esp-bare" "$ROOT/CrabApplePkg/Test/Qemu/empty" \
  --put "$OUT/CrabApple.efi=EFI/BOOT/BOOTAA64.EFI"
python3 "$ROOT/Scripts/qemu-run.py" "$SMOKE/esp-bare" --no-gop --timeout 40 \
  --log "$SMOKE/degraded.log" \
  --expect 'CRABAPPLE: config: none found, using built-in configuration' \
  --expect 'CRABAPPLE: config: no boot entry found' \
  --expect 'CRABAPPLE: ui: text console' \
  --expect "CRABAPPLE: menu: shown 3 entries, default 0 'exit'" \
  --expect 'CRABAPPLE: menu: timeout, booting default 0' \
  --expect 'CRABAPPLE: action: exit' \
  --reject 'ASSERT'

echo "== scenario 4: config on a partition head, tail as a block slice"
FAT_PYTHON=${FAT_PYTHON:-python3}
if command -v mcopy >/dev/null || "$FAT_PYTHON" -W ignore -c 'import pyfatfs.PyFatFS' 2>/dev/null; then
  python3 "$ROOT/Scripts/stage-esp.py" -o "$SMOKE/userdata-tree" "$ROOT/CrabApplePkg/Test/Qemu/userdata" \
    --put "$OUT/HelloPayload.efi=EFI/CrabApple/Payloads/HelloPayload.efi" >/dev/null
  "$FAT_PYTHON" "$ROOT/Scripts/mk-gpt-disk.py" --out "$SMOKE/gpt-disk.img" --tree "$SMOKE/userdata-tree" \
    --size 256 --fat 64 --name userdata
  python3 "$ROOT/Scripts/qemu-run.py" "$SMOKE/esp-bare" --disk "$SMOKE/gpt-disk.img" \
    --log "$SMOKE/slice.log" \
    --expect 'CRABAPPLE: config: loaded' \
    --expect "CRABAPPLE: slice passthrough: partition 'userdata' blocks 131072\\+[0-9]+ \\(512-byte\\), read-only" \
    --expect "CRABAPPLE-HELLO: check slice PASS \\('passthrough' [0-9]+ blocks ro, block 0 'CRABAPPLE-TAIL-MARKER'" \
    --expect 'CRABAPPLE-HELLO: RESULT PASS 1 passed 0 failed' \
    --reject 'ASSERT'
else
  echo "   skipped: needs mtools or FAT_PYTHON with pyfatfs"
fi

echo "== scenario 5: synthetic kext staging and XNU protocol compatibility"
python3 "$ROOT/Scripts/kext-fixtures.py" "$SMOKE/kext" "$OUT"
for variant in v2 v1 v1-empty missing-loader unknown nullboot missing-file bad-image truncated-command empty-plist duplicate bad-path bad-type wrong-entry; do
  expected='CRABAPPLE: kext: invalid entry configuration; boot refused'
  reject='CRABAPPLE-XNU-TEST: .* handoff'
  case "$variant" in
    v2) expected='CRABAPPLE-XNU-TEST: v2 handoff PASS call 1'; reject='CRABAPPLE-XNU-TEST: .* FAIL' ;;
    v1) expected='CRABAPPLE: xnu: loader revision 1 cannot accept kexts' ;;
    v1-empty) expected='CRABAPPLE-XNU-TEST: v1 handoff PASS call 1'; reject='CRABAPPLE-XNU-TEST: .* FAIL' ;;
    missing-loader) expected='CRABAPPLE: xnu: no loader protocol installed' ;;
    unknown|nullboot) expected='CRABAPPLE: xnu: unsupported loader revision or missing Boot callback' ;;
    missing-file) expected='CRABAPPLE: kext .*Code.kext: cannot read executable' ;;
    bad-image|truncated-command) expected='CRABAPPLE: kext .*Code.kext: invalid AArch64 MH_KEXT_BUNDLE; boot refused' ;;
    empty-plist) expected='CRABAPPLE: kext .*Code.kext: empty plist; boot refused' ;;
    duplicate) expected='CRABAPPLE: kext .*code.kext: duplicate bundle; boot refused' ;;
  esac
  python3 "$ROOT/Scripts/qemu-run.py" "$SMOKE/kext/$variant" --timeout "$KEXT_TIMEOUT" \
    --cpus 1 --finish-on-expect \
    --log "$SMOKE/kext-$variant.log" --expect "$expected" --reject "$reject" --reject 'ASSERT'
done

echo "== smoke test passed; logs and menu.png in $SMOKE"
