#!/bin/sh
# Installs the lint tools on a Linux x86_64 CI runner.
set -eu
BIN="${BIN:-/usr/local/bin}"
ACTIONLINT=1.7.7
GITLEAKS=8.28.0
LYCHEE=0.20.1

pip install --quiet yamllint==1.37.1 clang-format==23.1.3
npm install --global --silent markdownlint-cli2@0.23.3

curl -fsSL "https://github.com/rhysd/actionlint/releases/download/v${ACTIONLINT}/actionlint_${ACTIONLINT}_linux_amd64.tar.gz" | sudo tar xz -C "$BIN" actionlint
curl -fsSL "https://github.com/gitleaks/gitleaks/releases/download/v${GITLEAKS}/gitleaks_${GITLEAKS}_linux_x64.tar.gz" | sudo tar xz -C "$BIN" gitleaks
curl -fsSL "https://github.com/lycheeverse/lychee/releases/download/lychee-v${LYCHEE}/lychee-x86_64-unknown-linux-gnu.tar.gz" | sudo tar xz -C "$BIN" lychee
