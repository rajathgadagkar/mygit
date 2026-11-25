#!/usr/bin/env bash
# End-to-end CLI test suite for MyGit (runs in an isolated temporary directory).
# Author: Rajath Gadagkar <rajathgadagkar@gmail.com>

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
MYGIT="${PROJECT_ROOT}/mygit"

if [ ! -x "${MYGIT}" ]; then
    echo "Error: ${MYGIT} binary not found. Run 'make' first." >&2
    exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

PASS=0
FAIL=0

assert_eq() {
    local actual="$1"
    local expected="$2"
    local desc="$3"
    if [ "${actual}" = "${expected}" ]; then
        echo "[PASS] ${desc}"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] ${desc} (expected='${expected}', actual='${actual}')" >&2
        FAIL=$((FAIL + 1))
    fi
}

assert_contains() {
    local haystack="$1"
    local needle="$2"
    local desc="$3"
    if printf '%s' "${haystack}" | grep -Fq "${needle}"; then
        echo "[PASS] ${desc}"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] ${desc} (missing '${needle}')" >&2
        FAIL=$((FAIL + 1))
    fi
}

assert_fails() {
    local desc="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        echo "[FAIL] ${desc} (expected non-zero exit)" >&2
        FAIL=$((FAIL + 1))
    else
        echo "[PASS] ${desc}"
        PASS=$((PASS + 1))
    fi
}

cd "${TMP_DIR}"
echo "=== MyGit End-to-End CLI Tests (sandbox: ${TMP_DIR}) ==="

# 1. Version & help
assert_contains "$("${MYGIT}" --version)" "mygit version" "version output"
assert_contains "$("${MYGIT}" --help)" "PORCELAIN COMMANDS" "help output"

# 2. Init
"${MYGIT}" init >/dev/null
assert_eq "$([ -d .mygit/objects ] && echo yes || echo no)" "yes" "init creates .mygit/objects"
assert_eq "$([ -f .mygit/HEAD ] && echo yes || echo no)" "yes" "init creates .mygit/HEAD"

# 3. hash-object & cat-file (matches real git SHA-1!)
printf 'Hello World\n' > hello.txt
HASH="$("${MYGIT}" hash-object -w hello.txt)"
assert_eq "${HASH}" "557db03de997c86a4a028e1ebd3a1ceb225be238" "hash-object matches Git SHA-1"
assert_eq "$("${MYGIT}" cat-file -t "${HASH}")" "blob" "cat-file -t returns blob"
assert_eq "$("${MYGIT}" cat-file -s "${HASH}")" "12" "cat-file -s returns 12"
assert_eq "$("${MYGIT}" cat-file -p "${HASH:0:7}")" "Hello World" "cat-file -p works with 7-char short hash"

# 4. write-tree & ls-tree
mkdir -p subdir
printf 'nested data\n' > subdir/file.txt
TREE_HASH="$("${MYGIT}" write-tree)"
assert_eq "${#TREE_HASH}" "40" "write-tree returns 40-char SHA-1"
LS_OUT="$("${MYGIT}" ls-tree --name-only "${TREE_HASH}")"
assert_contains "${LS_OUT}" "hello.txt" "ls-tree lists hello.txt"
assert_contains "${LS_OUT}" "subdir" "ls-tree lists subdir"

# 5. add, status, commit, diff, log, show, checkout
"${MYGIT}" add . >/dev/null
STATUS_STAGED="$("${MYGIT}" status)"
assert_contains "${STATUS_STAGED}" "Changes to be committed" "status shows staged files"

COMMIT1="$("${MYGIT}" commit -m "Initial commit" | tail -n 1)"
assert_eq "${#COMMIT1}" "40" "commit 1 returns 40-char SHA-1"
assert_contains "$("${MYGIT}" status)" "nothing to commit, working tree clean" "working tree clean after commit"

printf 'Hello World v2\nAdded line\n' > hello.txt
DIFF_OUT="$("${MYGIT}" diff)"
assert_contains "${DIFF_OUT}" "-Hello World" "diff shows removed line"
assert_contains "${DIFF_OUT}" "+Hello World v2" "diff shows modified line"

"${MYGIT}" add hello.txt >/dev/null
COMMIT2="$("${MYGIT}" commit -m "Update hello.txt" | tail -n 1)"
assert_eq "${#COMMIT2}" "40" "commit 2 returns 40-char SHA-1"

LOG_OUT="$("${MYGIT}" log)"
assert_contains "${LOG_OUT}" "${COMMIT1}" "log contains commit 1"
assert_contains "${LOG_OUT}" "${COMMIT2}" "log contains commit 2"

SHOW_OUT="$("${MYGIT}" show "${COMMIT2:0:7}")"
assert_contains "${SHOW_OUT}" "Update hello.txt" "show works with short hash"

"${MYGIT}" checkout "${COMMIT1:0:7}" >/dev/null
assert_eq "$(cat hello.txt)" "Hello World" "checkout restores commit 1 content"

"${MYGIT}" checkout "${COMMIT2}" >/dev/null
assert_contains "$(cat hello.txt)" "Hello World v2" "checkout restores commit 2 content"

# 6. Error handling
assert_fails "hash-object missing file fails" "${MYGIT}" hash-object does_not_exist.txt
assert_fails "cat-file invalid hash fails" "${MYGIT}" cat-file -p deadbeef
assert_fails "checkout invalid hash fails" "${MYGIT}" checkout 1234567
assert_fails "unknown command fails" "${MYGIT}" nonexistent-cmd

echo ""
echo "Summary: ${PASS} passed, ${FAIL} failed."
[ "${FAIL}" -eq 0 ]
