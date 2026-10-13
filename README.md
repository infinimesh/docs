# infinimesh docs

infinimesh is an open source (Apache-2.0) device layer: devices connect over MQTT or HTTP, and infinimesh keeps their identity, permissions and state, then hands the data to the tools you already run.

The server lives in [infinimesh/core](https://github.com/infinimesh/core). This repo holds small device examples and practical guides.

## Examples

Each example is one sketch or script with a short README.

| Example | Board | Protocol |
| --- | --- | --- |
| [examples/esp32-mqtt](examples/esp32-mqtt) | ESP32 (Arduino) | MQTT over TLS on 8883 with a device certificate |
| [examples/esp32-http](examples/esp32-http) | ESP32 (Arduino) | HTTPS with a device token |
| [examples/picow-mqtt](examples/picow-mqtt) | Raspberry Pi Pico W (MicroPython) | MQTT over TLS on 8883 with a device certificate |
| [examples/curl](examples/curl) | Any computer | HTTPS with a device token |

## Guides

- [guides/cli-getting-started.md](guides/cli-getting-started.md): the `inf` CLI on 3.4, from login on `:443` to quotas, rules, OTA addon registration and bulk assign.
- [guides/small-devices-tls.md](guides/small-devices-tls.md): why TLS fails on small chips like the ESP8266, and how to fix it.
- [guides/node-red.md](guides/node-red.md): send device data to Node-RED with the MQTT bridge or webhook addon.

## Good to know

- Devices bring their own MQTT or HTTP client. infinimesh has no device SDK.
- MQTT topics: a device publishes to `devices/<id>/state/reported` and subscribes to `devices/<id>/state/desired`. `<id>` is the device uuid.
- Boards without TLS (for example ATmega Arduinos) should talk to a local gateway, such as a Raspberry Pi or an ESP32, which then connects to infinimesh over TLS. See the guide above.
- HTTP devices send their own JSON with `POST /v1/ingest` and a device token, and read desired state with `GET /v1/ingest/desired`. The v1 API is stable, see [API versioning](https://github.com/infinimesh/core/blob/main/docs/api-versioning.md).
- The boards themselves have not been tested yet. What was tested is listed in each README.

## Check before you commit

```bash
make commit-check
```

This runs `make lint` and then compiles the examples. `make lint` runs shellcheck, markdownlint, a lychee link check, clang-format on the sketches, actionlint, yamllint and gitleaks. The compile step runs a MicroPython compile check and `arduino-cli compile` for both ESP32 sketches. On Linux, `sh scripts/install-lint-tools.sh` installs the lint tools; on macOS use `brew install shellcheck markdownlint-cli2 lychee clang-format actionlint yamllint gitleaks`.

## License

Apache-2.0. See [LICENSE](LICENSE).
