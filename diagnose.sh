#!/usr/bin/env bash
set -uo pipefail
root="$PWD"
mkdir -p evidence || exit 1
failed=0
pacman -Q > evidence/packages.txt || failed=1
c++ --version > evidence/compiler.txt || failed=1
uname -a > evidence/uname.txt || failed=1
ps=/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
git_exe='/c/Program Files/Git/bin/git.exe'
"$ps" -NoProfile -Command 'Get-CimInstance Win32_Processor | Select-Object Name, Manufacturer, Architecture | Format-List' > evidence/cpu.txt || failed=1
for name in baseline head proposed; do
    if [ "$name" = proposed ]; then
        src="$root/head"
        "$git_exe" -C "$src" apply --check "$root/diagnostic/review.patch" || { failed=1; break; }
        "$git_exe" -C "$src" apply "$root/diagnostic/review.patch" || { failed=1; break; }
    else src="$root/$name"; fi
    printf "DIAGNOSTIC_PHASE %s\n" "$name"
    dest="$root/evidence/$name"
    mkdir -p "$dest" || { failed=1; continue; }
    "$git_exe" -C "$src" rev-parse HEAD > "$dest/source-sha.txt" || { failed=1; continue; }
    "$git_exe" -C "$src" diff > "$dest/source.patch" || { failed=1; continue; }
    if ! cmake -S "$src" -B "$src/_build" -DBUILD_TESTS=ON -DBUILD_BENCHMARK=ON -DBUILD_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Release -DDOWNLOAD_DOCTEST=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja > "$dest/configure.log" 2>&1; then failed=1; continue; fi
    cp "$src/_build/compile_commands.json" "$src/_build/CMakeCache.txt" "$dest/" || failed=1
    if ! cmake --build "$src/_build" --parallel 2 > "$dest/build.log" 2>&1; then failed=1; continue; fi
    exe="$src/_build/test/test_xsimd.exe"
    cp "$exe" "$dest/test_xsimd.exe" || failed=1
    sha256sum "$exe" > "$dest/executable.sha256" || failed=1
    llvm-objdump -p "$exe" > "$dest/pe-headers.txt" 2>&1 || true
    if "$ps" -NoProfile -File "$(cygpath -w "$root/diagnostic/native-status.ps1")" -Exe "$(cygpath -w "$exe")" -OutputDir "$(cygpath -w "$dest/native")" -WorkingDirectory "$(cygpath -w "$src")" > "$dest/native-launch.log" 2>&1; then native_rc=0; else native_rc=$?; failed=1; fi
    printf '%s\n' "$native_rc" > "$dest/native-wrapper.exit" || failed=1
    if (cd "$src" && timeout --kill-after=10s 90s ./_build/test/test_xsimd) > "$dest/msys.stdout" 2> "$dest/msys.stderr"; then rc=0; else rc=$?; failed=1; fi
    printf '%s\n' "$rc" > "$dest/msys.exit" || failed=1
    if [ "$native_rc" != 0 ] || [ "$rc" != 0 ]; then
        # Debug the original binary before adding any instrumentation.
        (cd "$src" && timeout --kill-after=10s 90s gdb -q -batch -ex 'set pagination off' -ex 'set confirm off' -ex 'set print thread-events off' -ex 'run --no-colors=true' -ex 'thread apply all bt' -ex 'info registers' -ex 'x/12i $pc-16' --args "$exe") > "$dest/gdb-original.txt" 2>&1 || true
    fi
 done
# Trace only after all three original-source runs. Never use its result to replace theirs.
for name in baseline head proposed; do
    if [ "$name" = baseline ]; then src="$root/baseline"; else src="$root/head"; fi
    dest="$root/evidence/$name"
    [ -f "$dest/native-wrapper.exit" ] || continue
    if [ "$(cat "$dest/native-wrapper.exit")" = 0 ] && [ "$(cat "$dest/msys.exit")" = 0 ]; then continue; fi
    # Restore only our disposable checkout before each traced variant.
    "$git_exe" -C "$src" checkout -- test/main.cpp test/test_complex_power.cpp || { failed=1; continue; }
    if [ "$name" = proposed ]; then
        "$git_exe" -C "$src" apply "$root/diagnostic/review.patch" || { failed=1; continue; }
    fi
    cp "$src/test/main.cpp" "$dest/main-original.cpp" || { failed=1; continue; }
    cat "$root/diagnostic/trace-listener.hpp" >> "$src/test/main.cpp" || { failed=1; continue; }
    "$git_exe" -C "$src" diff > "$dest/instrumented-source.patch" || { failed=1; continue; }
    if cmake -S "$src" -B "$src/_build" -DCMAKE_CXX_FLAGS=-g > "$dest/trace-configure.log" 2>&1 && cmake --build "$src/_build" --parallel 2 > "$dest/trace-build.log" 2>&1; then
        exe="$src/_build/test/test_xsimd.exe"
        mkdir -p "$dest/trace-binary" || { failed=1; continue; }
        cp "$exe" "$src/_build/compile_commands.json" "$src/_build/CMakeCache.txt" "$dest/trace-binary/" || failed=1
        sha256sum "$exe" > "$dest/trace-binary/executable.sha256" || failed=1
        "$ps" -NoProfile -File "$(cygpath -w "$root/diagnostic/native-status.ps1")" -Exe "$(cygpath -w "$exe")" -OutputDir "$(cygpath -w "$dest/trace")" -WorkingDirectory "$(cygpath -w "$src")" -Trace > "$dest/trace-launch.log" 2>&1 || true
        (cd "$src" && timeout --kill-after=10s 90s gdb -q -batch -ex 'set pagination off' -ex 'set confirm off' -ex 'set print thread-events off' -ex 'run --no-colors=true' -ex 'thread apply all bt full' -ex 'info registers' -ex 'x/12i $pc-16' --args "$exe") > "$dest/gdb-trace.txt" 2>&1 || true
    fi
 done
exit "$failed"
