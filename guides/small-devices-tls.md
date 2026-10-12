# TLS on small devices

Some boards, like the ESP8266 and small Arduino boards, cannot finish a TLS handshake before the connection times out. Even WolfSSL can be too large for them. This guide explains why, and what to do about it, from the cheapest fix to the last resort.

Short version:

1. Use an EC P-256 (ECDSA) key for the device certificate, not RSA.
2. Use an ECDSA server certificate with a short chain, and TLS 1.2 with `ECDHE-ECDSA-AES128-GCM-SHA256`.
3. Give the handshake enough time, on the device and on the server.
4. On the ESP8266, use BearSSL with smaller buffers.
5. If the board still cannot do it, use a hardware crypto chip or a local gateway.

## Why the handshake is slow

### Public key math on a slow CPU

The handshake does a few public key operations. On a desktop they take microseconds. On a small MCU they take seconds.

BearSSL publishes numbers for an ARM Cortex M0+ at 48 MHz ([BearSSL speed](https://bearssl.org/speed.html)):

| Operation | Speed on M0+ 48 MHz | Time per operation |
| --- | --- | --- |
| RSA 2048 private key (sign) | 0.128 per second | about 8 seconds |
| RSA 2048 public key (verify) | 2.2 per second | about 0.45 seconds |
| P-256 point multiply (generic point) | 1.7 per second | about 0.6 seconds |
| P-256 point multiply (fixed point, used when signing) | 2.5 per second | about 0.4 seconds |

The ESP8266 is in the same league. Its Arduino core docs say it has no crypto hardware and no 32x32 to 64 bit multiplier, that TLS should run at 160 MHz, and that some key exchanges take several seconds even then ([ESP8266 BearSSL docs](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/bearssl-client-secure-class.html)).

What this means for infinimesh: the MQTT bridge on port 8883 requires a client certificate. With a client certificate the device must sign the handshake with its own private key. If that key is RSA 2048, that one step alone is around 8 seconds on an M0+ class chip. With a P-256 key it is well under a second.

### Certificate chain size

The device has to receive and check every certificate the server sends. A typical Let's Encrypt RSA chain is two certificates and several kilobytes. Each RSA signature in the chain costs a verify. A longer chain means more bytes to buffer, more hashing, and more verify operations.

### RAM

TLS records can be up to 16 KB, so a client needs a 16 KB receive buffer unless the server agrees to smaller records. The ESP8266 has about 40 KB of free heap. BearSSL on the ESP8266 needs about 6 KB of stack plus about 22 KB per connection by default ([ESP8266 BearSSL docs](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/bearssl-client-secure-class.html)). A large certificate chain adds to that.

## Fixes

### 1. Device certificate: use EC P-256

Make the device key with P-256. infinimesh accepts any certificate you register, it only checks that the fingerprint matches.

```bash
openssl ecparam -name prime256v1 -genkey -noout -out device.key
openssl req -new -x509 -sha256 -key device.key -out device.crt -days 365 -subj "/CN=my-sensor"
```

On the ESP8266 load it with `setClientECCert` instead of `setClientRSACert`.

### 2. Server certificate: ECDSA and a short chain

With an ECDSA server certificate the device verifies P-256 signatures instead of RSA, and the cipher suite becomes `ECDHE-ECDSA-AES128-GCM-SHA256` (TLS 1.2). That suite is supported by BearSSL, mbedTLS (ESP32) and Go.

- Let's Encrypt issues ECDSA certificates when you ask for an EC key, and the ECDSA chain to ISRG Root X2 is short ([Let's Encrypt chains](https://letsencrypt.org/certificates/)).
- For a private deployment, a private CA with one P-256 root and the server certificate signed directly by it gives a chain of one certificate.
- Pin the CA on the device (one trust anchor) instead of loading a big CA bundle.

The MQTT bridge can serve an ECDSA certificate next to the primary one. Set `TLS_EC_CERT_FILE` and `TLS_EC_KEY_FILE` (both or neither). Each client gets the certificate its cipher suites allow, so a TLS 1.2 client that offers only ECDSA suites gets the ECDSA certificate. The bridge requires TLS 1.2 or newer.

```bash
TLS_EC_CERT_FILE=/cert/tls-ec.crt
TLS_EC_KEY_FILE=/cert/tls-ec.key
TLS_HANDSHAKE_TIMEOUT=45s
```

For local tests, core ships a P-256 pair in `hack/server-ec.crt` and `hack/server-ec.key`, with subject alternative names for `localhost` and `127.0.0.1`. Never serve that pair next to a public certificate, or a client that prefers ECDSA gets a certificate for `localhost`. The details are in core's [small-device TLS guide](https://github.com/infinimesh/core/blob/main/docs/guides/small-devices-tls.md).

### 3. Give the handshake enough time

The timeout can come from three places:

- The device. On the ESP32, `WiFiClientSecure::setHandshakeTimeout(seconds)` and ESP-MQTT `network.timeout_ms`. On the ESP8266, `client.setTimeout(ms)`.
- The MQTT bridge. `TLS_HANDSHAKE_TIMEOUT` sets how long the bridge waits for the handshake. It is a Go duration such as `30s` or `1m`, and the default is `30s`. It covers the handshake only. After the handshake the bridge waits for the CONNECT packet.
- A proxy in front of the bridge. In the core compose files port 8883 goes straight to the bridge, Traefik and Caddy do not sit in front of it, so their timeouts do not apply. If you put a TCP proxy in front yourself (for example a Traefik TCP router or HAProxy), check its read and idle timeouts. In Traefik these are the entry point `transport.respondingTimeouts` settings ([Traefik entry points](https://doc.traefik.io/traefik/routing/entrypoints/#respondingtimeouts)).

If the handshake takes more than 30 seconds even with P-256 keys, raising the timeout only hides the problem. Fix the keys first.

### 4. Session resumption

A full handshake is only needed once. After that the device can resume the TLS session and skip the public key math. The ESP8266 docs say this saves 3 to 4 seconds with EC keys ([ESP8266 BearSSL docs](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/bearssl-client-secure-class.html), see `BearSSL::Session`).

TLS 1.2 has two ways to resume: session IDs (server keeps a cache) and session tickets ([RFC 5077](https://www.rfc-editor.org/rfc/rfc5077)). Go servers resume only with tickets. A resumed session still identifies the device by its client certificate, and core has a test for this on TLS 1.2 and TLS 1.3. Tickets last 7 days. The ticket keys live in the bridge process, so a bridge restart, or a connection that lands on another replica, falls back to a full handshake. Whether a given client library sends tickets or only session IDs decides if resumption works. We have not tested the ESP8266 session API against the bridge yet.

Keep the MQTT connection open with a keepalive when you can. One long connection beats many short handshakes.

### 5. ESP8266: BearSSL with smaller buffers

On the ESP8266 use `BearSSL::WiFiClientSecure`:

```cpp
BearSSL::WiFiClientSecure net;
BearSSL::X509List ca(CA_PEM);
BearSSL::X509List cert(DEVICE_CERT_PEM);
BearSSL::PrivateKey key(DEVICE_KEY_PEM);

net.setTrustAnchors(&ca);
net.setClientECCert(&cert, &key, BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN, BR_KEYTYPE_EC);
net.setSSLVersion(BR_TLS12, BR_TLS12);
if (net.probeMaxFragmentLength(host, 8883, 1024)) {
  net.setBufferSizes(1024, 1024);
} else {
  net.setBufferSizes(16384, 512);
}
```

Run the CPU at 160 MHz. Keep the objects global, BearSSL does not copy them.

Maximum Fragment Length Negotiation (MFLN, [RFC 6066 section 4](https://www.rfc-editor.org/rfc/rfc6066#section-4)) lets the client ask for records of 512 to 4096 bytes. **The infinimesh MQTT bridge does not support MFLN**: Go's TLS stack does not implement it, and core's guide confirms the bridge ignores the extension. So on the ESP8266 the receive buffer must stay at 16 KB, unless you are sure the bridge never sends a record bigger than your buffer. During the handshake the bridge sends its certificate chain, so a short chain matters here too.

### 6. TLS-PSK

TLS with a pre-shared key ([RFC 4279](https://www.rfc-editor.org/rfc/rfc4279), [RFC 8442](https://www.rfc-editor.org/rfc/rfc8442) for `ECDHE-PSK-AES128-GCM-SHA256`) skips certificates completely. Pure PSK needs no public key math at all, so it fits the smallest chips.

**The infinimesh MQTT bridge itself does not support PSK.** Go's `crypto/tls` has no PSK cipher suites ([golang/go#6379](https://github.com/golang/go/issues/6379)).

You can run stunnel in front of the bridge. Core ships a sample in `hack/stunnel/psk.conf`. The device talks TLS-PSK to stunnel on `127.0.0.1:8884`, and stunnel opens a TLS connection to the bridge on 8883 with the client certificate registered for that device. The bridge still requires and checks the certificate. One stunnel service is one device, so this does not scale to a large fleet.

Generate your own key and never commit the secrets file:

```bash
openssl rand -hex 16
```

The secrets file has one line, `identity:hexkey`. See core's [small-device TLS guide](https://github.com/infinimesh/core/blob/main/docs/guides/small-devices-tls.md) for the full setup.

Note that BearSSL does not support PSK suites either, so on the ESP8266 this would need mbedTLS or WolfSSL.

### 7. Hardware crypto chip

A secure element such as the Microchip ATECC608 does P-256 ECDSA and ECDH in hardware and keeps the private key inside the chip, so it cannot be read out ([Microchip ATECC608B](https://www.microchip.com/en-us/product/atecc608b)). It connects over I2C. Libraries like ArduinoECCX08 and Microchip CryptoAuthLib work with it. This makes the device side of the handshake fast and protects the key, but the board still needs RAM for TLS records.

### 8. Local gateway

If a board cannot do TLS well, do not force it. Put a small gateway next to it:

- A Raspberry Pi or an ESP32 runs the TLS connection to infinimesh with the device certificate.
- The small board talks to the gateway over a local link: serial, I2C, RS-485, BLE, or plain MQTT on the local network.
- The gateway publishes to `devices/<id>/state/reported` for each board.

This is also the answer for boards with no TLS at all, like the ATmega based Arduinos.

## Quick checklist

- [ ] Device key is EC P-256.
- [ ] Server certificate is ECDSA with a short chain, and the device pins one CA.
- [ ] TLS 1.2, `ECDHE-ECDSA-AES128-GCM-SHA256`.
- [ ] Clock is set with NTP before connecting.
- [ ] Device handshake timeout is 20 to 30 seconds.
- [ ] Bridge timeout raised with `TLS_HANDSHAKE_TIMEOUT` only if keys are already P-256.
- [ ] Bridge has `TLS_EC_CERT_FILE` and `TLS_EC_KEY_FILE` set.
- [ ] ESP8266 at 160 MHz with BearSSL.
- [ ] Keepalive instead of reconnecting for every message.
- [ ] Still failing: secure element, or a local gateway.

## Server side status

Core added the configurable handshake timeout, the ECDSA certificate pair, TLS 1.2 as the minimum, the P-256 test certificate, the session resumption test and the stunnel PSK sample in [core #52](https://github.com/infinimesh/core/pull/52).

Still open in core:

1. Port 1883 is plaintext and published by default in the compose file. Make it opt-in or add a clear warning.
2. `TLS_HANDSHAKE_TIMEOUT` has no upper limit. A cap would limit slow handshake abuse.
3. Ignore `hack/stunnel/psk-secrets.txt` in `.gitignore` so a real key is not committed by accident.
