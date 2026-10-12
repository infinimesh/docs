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

Today the infinimesh MQTT bridge loads one certificate from `TLS_CERT_FILE`, and the local test certificate in core (`hack/server.crt`) is RSA 2048. The bridge accepts an ECDSA certificate if you give it one, because Go's TLS stack supports ECDSA. Pointing `TLS_CERT_FILE` and `TLS_KEY_FILE` at a P-256 certificate is enough. Offering RSA and ECDSA side by side needs a core change, see the list at the end.

Checked against core main on 12 October 2026: a TLS 1.2 client gets `ECDHE-RSA-AES128-GCM-SHA256` with the default RSA certificate.

### 3. Give the handshake enough time

The timeout can come from three places:

- The device. On the ESP32, `WiFiClientSecure::setHandshakeTimeout(seconds)` and ESP-MQTT `network.timeout_ms`. On the ESP8266, `client.setTimeout(ms)`.
- The MQTT bridge. In core, `cmd/mqtt-bridge/mqtt-bridge.go` gives each TLS handshake a fixed 30 seconds. It cannot be changed without a code change. After the handshake the bridge waits for the CONNECT packet.
- A proxy in front of the bridge. In the core compose files port 8883 goes straight to the bridge, Traefik and Caddy do not sit in front of it, so their timeouts do not apply. If you put a TCP proxy in front yourself (for example a Traefik TCP router or HAProxy), check its read and idle timeouts. In Traefik these are the entry point `transport.respondingTimeouts` settings ([Traefik entry points](https://doc.traefik.io/traefik/routing/entrypoints/#respondingtimeouts)).

If the handshake takes more than 30 seconds even with P-256 keys, raising the timeout only hides the problem. Fix the keys first.

### 4. Session resumption

A full handshake is only needed once. After that the device can resume the TLS session and skip the public key math. The ESP8266 docs say this saves 3 to 4 seconds with EC keys ([ESP8266 BearSSL docs](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/bearssl-client-secure-class.html), see `BearSSL::Session`).

TLS 1.2 has two ways to resume: session IDs (server keeps a cache) and session tickets ([RFC 5077](https://www.rfc-editor.org/rfc/rfc5077)). Go servers resume only with tickets. We checked core main: OpenSSL resumes a ticket session against port 8883. Whether a given client library sends tickets or only session IDs decides if resumption works. We have not tested the ESP8266 session API against the bridge yet.

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

Maximum Fragment Length Negotiation (MFLN, [RFC 6066 section 4](https://www.rfc-editor.org/rfc/rfc6066#section-4)) lets the client ask for records of 512 to 4096 bytes. **The infinimesh MQTT bridge does not support MFLN today**: Go's TLS stack does not implement it, and our test against core main got no MFLN reply. So on the ESP8266 the receive buffer must stay at 16 KB, unless you are sure the bridge never sends a record bigger than your buffer. During the handshake the bridge sends its certificate chain, so a short chain matters here too.

### 6. TLS-PSK

TLS with a pre-shared key ([RFC 4279](https://www.rfc-editor.org/rfc/rfc4279), [RFC 8442](https://www.rfc-editor.org/rfc/rfc8442) for `ECDHE-PSK-AES128-GCM-SHA256`) skips certificates completely. Pure PSK needs no public key math at all, so it fits the smallest chips.

**The infinimesh MQTT bridge does not support PSK.** Go's `crypto/tls` has no PSK cipher suites ([golang/go#6379](https://github.com/golang/go/issues/6379)), and a PSK handshake against core main fails with a handshake failure alert. Supporting it needs a TLS terminator that speaks PSK in front of the bridge, see the list at the end.

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
- [ ] ESP8266 at 160 MHz with BearSSL.
- [ ] Keepalive instead of reconnecting for every message.
- [ ] Still failing: secure element, or a local gateway.

## For Grok Code in core

Server side changes that would help small devices (checked on core main, 12 October 2026):

1. `cmd/mqtt-bridge/mqtt-bridge.go`: the TLS handshake timeout is hard coded to 30 seconds. Make it configurable with an env var (for example `TLS_HANDSHAKE_TIMEOUT`), keep 30s as the default, and document it.
2. Same file, `tls.Config`: allow an ECDSA certificate next to the RSA one (two entries in `Certificates`, Go picks by the client's signature algorithms), for example `TLS_EC_CERT_FILE` and `TLS_EC_KEY_FILE`. Set `MinVersion: tls.VersionTLS12` explicitly. Ship a P-256 test certificate in `hack/` next to the RSA one, with a SAN for localhost (the current `hack/server.crt` has only a CN, so strict clients reject it).
3. Session resumption: confirm that resumed TLS sessions still identify the device (the bridge reads `PeerCertificates` after the handshake), add a test for it, and document ticket lifetime. Consider a server side session ID cache for clients that cannot use tickets. Go has none, so this would need a terminator.
4. TLS-PSK: Go cannot do it. Option: an optional TLS-PSK terminator (for example stunnel) in front of the bridge that maps a PSK identity to a device and passes the device id to the bridge. Treat as a design task, not a quick fix.
5. MFLN: Go does not implement it. Document that the bridge does not negotiate it, and keep the server chain short so a small receive buffer has the best chance.
6. Docs: add a "small devices" section to `docs/device-auth.md` that links to this guide.
