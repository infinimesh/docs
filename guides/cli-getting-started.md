# Getting started with the inf CLI

This guide takes you from a fresh Docker Compose stack to a namespace with a quota, a device, a rule, and an OTA assignment. Every command here was run against infinimesh 3.4.

## Install

```bash
brew tap infinimesh/inf-cli https://github.com/infinimesh/inf-cli
brew install inf
```

If Homebrew says the tap is untrusted, run `brew trust infinimesh/inf-cli` and install again. Binaries for Linux, macOS and Windows are on the [inf-cli releases page](https://github.com/infinimesh/inf-cli/releases). The 3.2 CLI does not work against a 3.4 core, so check `inf version` if login fails with `Unimplemented` or `404`.

## Log in

The API on Compose is `api.localhost:443` (or `api.<BASE_DOMAIN>:443`). Port 8000 is not the CLI port and returns 404.

```bash
inf login api.localhost:443 infinimesh infinimesh --insecure-skip-verify
inf whoami
```

The default Traefik certificate does not cover `api.localhost`, so a local login needs `--insecure-skip-verify`. On a public domain with a real certificate, drop the flag. Use `--ca-file` only when your own CA signed a certificate that matches the host.

Login stores the host, the token and the TLS settings in `$HOME/.default.infinimesh.yaml`. If you switch from a local stack to a real one, log in again with the TLS flags you want, and check the first command after login works.

## Namespaces and quotas

There is no `quotas` command. Quotas are flags on `inf ns create` and `inf ns update`. Zero means unlimited.

```bash
inf ns create --title plant-a --max-devices 50 --device-mps 5
inf ns update <namespace> --max-devices 100
```

`--device-mps` limits messages per second per device. `--namespace-mps` limits the whole namespace.

## Devices

```bash
inf devices create pump.yaml --crt pump.pem --namespace <namespace>
inf devices token <device> --allow-post > device.token
inf devices state <device> --token "$(cat device.token)" --report '{"temp":21.5}'
```

The device template is JSON or YAML with `title`, `enabled` and `tags`. Real devices talk MQTT or HTTP. See the [examples](../examples).

## Rules and alarms

A rule is a CEL expression on device state. Test it before you save it.

```bash
inf rules dry-run --when 'reported.temp > 70' --clear 'reported.temp < 60'
inf rules create --namespace <namespace> --name overheat \
  --when 'reported.temp > 70' --clear 'reported.temp < 60' --cooldown 10m
inf alarms list --namespace <namespace>
inf alarms ack --rule <rule> --device <device>
inf alarms clear --rule <rule> --device <device>
```

`ack` and `clear` take `--rule` and `--device` flags, not positional arguments. `inf alarms list --state any` shows acked and cleared alarms too.

## OTA: register an addon first

Core does not ship firmware. It stores the target version per device (`desired.ota.version`) and what the device reports back (`reported.ota`). An OTA addon such as hawkBit or Mender does the delivery. OTA is the only addon kind that registers with core. Every other addon just uses the API.

The order matters:

1. Log in with a root token. Only root may create or delete plugins.

   ```bash
   inf login --root api.localhost:443 infinimesh infinimesh --insecure-skip-verify
   ```

2. Register the addon. The token is printed once and never shown again, so store it in your secret manager.

   ```bash
   inf plugins create --title hawkbit --kind ota --provider hawkbit --namespace <namespace>
   ```

3. Switch OTA on for the namespace. Before step 2 this fails with "ota cannot be enabled until an OTA addon is registered".

   ```bash
   inf ns update <namespace> --ota --provider hawkbit
   ```

4. Assign a version as the addon. `--token` applies to one command and does not replace your stored login.

   ```bash
   inf --token "$ADDON_TOKEN" ota assign --namespace <namespace> --version 1.2.3
   inf --token "$ADDON_TOKEN" ota assign --device <device> --version 1.2.3
   ```

With provider `none`, an account with manage access can assign. With hawkbit, mender or webhook, only the addon token can. An admin token is refused per device.

### Bulk assign returns 200 even when devices fail

`ota assign` is a bulk call. It needs `--version` and one of `--namespace`, `--tag` or `--device`. The call succeeds as a whole and returns a result per device. A refused device shows up as an error in its own row, with `updated 0`, not as a failed command. Read the per-device results. A device token is rejected outright with "bulk assign needs an account or addon token".

## Audit

```bash
inf audit --namespace <namespace>
```

Alarm acks and clears, plugin create and delete, and OTA assigns are recorded. An assign made with an addon token is recorded under the plugin id.

## Metrics

```bash
inf metrics --addr 127.0.0.1:2112
```

Each core service serves Prometheus on `:2112` inside its own container. Compose publishes host port 2112 for the MQTT bridge only, so `--addr` is required. Look for the `infinimesh_` metrics.

## Next

Server side reference: [rules](https://github.com/infinimesh/core/blob/main/docs/rules.md), [OTA](https://github.com/infinimesh/core/blob/main/docs/ota.md), [CoAP](https://github.com/infinimesh/core/blob/main/docs/coap.md), [fleet](https://github.com/infinimesh/core/blob/main/docs/fleet.md). Run `inf <command> --help` for every flag.
