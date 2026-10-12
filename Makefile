FQBN ?= esp32:esp32:esp32
SKETCHES := examples/esp32-mqtt examples/esp32-http
CSOURCES := $(wildcard examples/*/*.ino examples/*/*.c examples/*/*.h)

.PHONY: commit-check lint compile shellcheck markdownlint links clang-format actionlint yamllint gitleaks micropython arduino

commit-check: lint compile

lint: shellcheck markdownlint links clang-format actionlint yamllint gitleaks
	@echo "lint passed."

compile: micropython arduino

shellcheck:
	shellcheck examples/curl/*.sh scripts/*.sh

markdownlint:
	markdownlint-cli2 "**/*.md"

links:
	sh scripts/lychee.sh '**/*.md' examples

clang-format:
	clang-format --dry-run --Werror $(CSOURCES)

actionlint:
	actionlint

yamllint:
	yamllint --strict .

gitleaks:
	gitleaks dir . --no-banner --redact

micropython:
	mpy-cross -o /tmp/picow-main.mpy examples/picow-mqtt/main.py

arduino:
	@for s in $(SKETCHES); do \
		echo "compile $$s"; \
		arduino-cli compile --fqbn $(FQBN) $$s || exit 1; \
	done
