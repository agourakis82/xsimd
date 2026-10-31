#!/usr/bin/env bash
set -uo pipefail
root="$PWD"
failed=0
mkdir -p evidence/runtime
{
    printf 'MSYSTEM=%s\nPATH=%s\n' "${MSYSTEM-}" "$PATH"
    pacman -Q
    for tool in c++ cmake ninja; do type -a "$tool"; done
    c++ --version
    uname -a
    env | LC_ALL=C sort | sed -n '/^MSYS/p;/^MSYSTEM/p'
} > evidence/environment.txt 2>&1
ps=/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
cat > evidence/runtime/smoke.cpp <<'CPP'
#include <iostream>
#include <string>
#include <vector>
int main() {
    std::vector<std::string> messages{"CXX23_RUNTIME_MAIN_REACHED"};
    std::cout << messages.front() << std::endl;
    return 0;
}
CPP
if c++ -std=c++23 evidence/runtime/smoke.cpp -o evidence/runtime/smoke.exe > evidence/runtime/build.log 2>&1; then
    "$ps" -NoProfile -File "$(cygpath -w "$root/diagnostic/native-status.ps1")" \
        -Exe "$(cygpath -w "$root/evidence/runtime/smoke.exe")" \
        -OutputDir "$(cygpath -w "$root/evidence/runtime")" -Smoke || failed=1
else
    failed=1
fi
for name in baseline candidate; do
    out="$root/evidence/$name"
    mkdir -p "$out"
    if ! git -C "$name" rev-parse HEAD > "$out/source-sha.txt" ||
       ! git -C "$name" status --porcelain > "$out/source-status.txt"; then
        failed=1
        continue
    fi
    if [ "$name" = baseline ]; then
        expected=ebe9991fb0e8ed2dfb7c1a9425aaf6b104adab86
    else
        expected=d46d757429cd2fd3abad7250445cec9d493b3f59
    fi
    if [ "$(cat "$out/source-sha.txt")" != "$expected" ] || [ -s "$out/source-status.txt" ]; then
        failed=1
        continue
    fi
    # C++23 is explicit under ADR009. This is a fresh paired diagnostic, not an
    # exact replay of the old runner image or package snapshot.
    if ! cmake -S "$name" -B "$name/_build" -DBUILD_TESTS=ON \
        -DBUILD_BENCHMARK=ON -DBUILD_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Release \
        -DDOWNLOAD_DOCTEST=ON -DCMAKE_CXX_STANDARD=23 \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja > "$out/configure.log" 2>&1; then
        failed=1
        continue
    fi
    if ! cp "$name/_build/CMakeCache.txt" "$out/" ||
       ! cp "$name/_build/compile_commands.json" "$out/"; then
        failed=1
    fi
    if ! cmake --build "$name/_build" --parallel 2 > "$out/build.log" 2>&1; then
        failed=1
        continue
    fi
    exe="$root/$name/_build/test/test_xsimd.exe"
    if ! cp "$exe" "$out/" || ! sha256sum "$exe" > "$out/executable.sha256"; then
        failed=1
    fi
    if command -v llvm-readobj >/dev/null; then
        llvm-readobj --coff-imports "$exe" > "$out/imports.txt" 2>&1
        printf '%s\n' "$?" > "$out/imports.exit"
    else
        printf 'unavailable\n' > "$out/imports.exit"
    fi
    if command -v ldd >/dev/null; then
        ldd "$exe" > "$out/ldd.txt" 2>&1
        printf '%s\n' "$?" > "$out/ldd.exit"
    else
        printf 'unavailable\n' > "$out/ldd.exit"
    fi
    require_sqrt=()
    if [ "$name" = candidate ]; then require_sqrt=(-RequireSqrt); fi
    ( cd "$name" && "$ps" -NoProfile -File "$(cygpath -w "$root/diagnostic/native-status.ps1")" \
        -Exe "$(cygpath -w "$exe")" -OutputDir "$(cygpath -w "$out")" "${require_sqrt[@]}" ) \
        > "$out/native-launch.log" 2>&1 || failed=1
    # Preserve the original MSYS2 invocation and its shell-visible exit code.
    ( cd "$name" && timeout --verbose --kill-after=10s 180s ./_build/test/test_xsimd ) > "$out/msys.stdout" 2> "$out/msys.stderr"
    rc=$?
    printf '%s\n' "$rc" > "$out/msys.exit"
    if [ "$rc" = 124 ] || [ "$rc" = 137 ]; then
        printf 'timeout-wrapper status; inspect stderr before interpreting\n' > "$out/msys.timeout-status"
    fi
    if [ "$rc" -ne 0 ]; then failed=1; fi
done
printf '%s\n' "$failed" > evidence/diagnostic.exit
exit "$failed"
