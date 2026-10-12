FQBN ?= esp32:esp32:esp32
SKETCHES := examples/esp32-mqtt examples/esp32-http

.PHONY: commit-check lint compile shellcheck micropython arduino

commit-check: lint compile

lint: shellcheck

compile: micropython arduino

shellcheck:
	shellcheck examples/curl/*.sh

micropython:
	mpy-cross -o /tmp/picow-main.mpy examples/picow-mqtt/main.py

arduino:
	@for s in $(SKETCHES); do \
		echo "compile $$s"; \
		arduino-cli compile --fqbn $(FQBN) $$s || exit 1; \
	done
