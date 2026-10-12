#!/bin/sh
# Link check. Falls back to local links only when there is no network.
set -eu
if curl -fsS -m 5 -o /dev/null https://github.com; then
  lychee --config lychee.toml --no-progress "$@"
else
  echo "no network, checking local links only"
  lychee --config lychee.toml --no-progress --offline "$@"
fi
