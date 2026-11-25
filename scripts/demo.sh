#!/usr/bin/env bash
# Interactive walkthrough demonstrating MyGit core workflows in an isolated sandbox.
# Author: Rajath Gadagkar <rajathgadagkar@gmail.com>

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
MYGIT="${PROJECT_ROOT}/mygit"

if [ ! -x "${MYGIT}" ]; then
    echo "Building mygit..."
    (cd "${PROJECT_ROOT}" && make)
fi

SANDBOX="$(mktemp -d)"
trap 'rm -rf "${SANDBOX}"' EXIT
cd "${SANDBOX}"

echo "=== MyGit Walkthrough (sandbox: ${SANDBOX}) ==="
echo ""

echo "$ mygit init"
"${MYGIT}" init

echo ""
echo "$ echo 'Hello, MyGit!' > README.md && mkdir src && echo 'int main() { return 0; }' > src/app.cpp"
printf 'Hello, MyGit!\n' > README.md
mkdir -p src
printf 'int main() { return 0; }\n' > src/app.cpp

echo ""
echo "$ mygit status"
"${MYGIT}" status

echo ""
echo "$ mygit add ."
"${MYGIT}" add .

echo ""
echo "$ mygit commit -m 'Initial commit'"
COMMIT1="$("${MYGIT}" commit -m "Initial commit" | tail -n 1)"
echo "Created commit: ${COMMIT1}"

echo ""
echo "$ printf 'Hello, MyGit!\nAdded unified diff support.\n' > README.md"
printf 'Hello, MyGit!\nAdded unified diff support.\n' > README.md

echo ""
echo "$ mygit diff"
"${MYGIT}" diff

echo ""
echo "$ mygit add README.md && mygit commit -m 'Document diff support'"
"${MYGIT}" add README.md
COMMIT2="$("${MYGIT}" commit -m "Document diff support" | tail -n 1)"
echo "Created commit: ${COMMIT2}"

echo ""
echo "$ mygit log"
"${MYGIT}" log

echo ""
echo "$ mygit checkout ${COMMIT1:0:7}"
"${MYGIT}" checkout "${COMMIT1:0:7}"
echo "README.md is now:"
cat README.md
