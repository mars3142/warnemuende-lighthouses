# Thread Integration — ESP32-H2 Lighthouse

This document describes the Thread network protocol used by the ESP32-H2 lighthouse
firmware and what the ESP32-C6 coordinator must implement to discover and control it.

---

## Architecture

```
ESP32-C6 (Coordinator / Leader)
    │  Thread / IEEE 802.15.4
    └── ESP32-H2 (Lighthouse / Child or Router)
            CoAP server on port 5683
            Resources: /.well-known/core  /beacon  /outdoor  /flicker  /group
```

The H2 is a **Full Thread Device (FTD)** with leader weight 1, so the C6 always wins
leader election. The H2 operates as Child or Router — never Leader.

---

## 1 — Pre-provisioning: device name

Before first flash, write a human-readable name into the H2's NVS. This name is
announced on the Thread network when the device attaches so the C6 can identify it.

```bash
# requires IDF environment: . $IDF_PATH/export.sh
./scripts/set_device_name.sh "Leuchtturm West"

# or via Make
make set-name NAME="Leuchtturm West"
make set-name NAME="Leuchtturm West" PORT=/dev/cu.usbmodem1101
```

The name is stored in NVS namespace `thread`, key `name`. If no name is set the
device uses `"Lighthouse"` as fallback.

---

## 2 — Thread network credentials

Both the H2 and the C6 use a **fixed, pre-provisioned dataset** — there is no runtime
commissioning (no Joiner, no Commissioner). Both sides must be flashed with identical
credentials.

| Parameter    | Value                              | sdkconfig key                       |
| ------------ | ---------------------------------- | ----------------------------------- |
| Network name | `MaerklinNet`                      | `CONFIG_OPENTHREAD_NETWORK_NAME`    |
| Channel      | `18`                               | `CONFIG_OPENTHREAD_NETWORK_CHANNEL` |
| PAN ID       | `0xBEEF`                           | `CONFIG_OPENTHREAD_NETWORK_PANID`   |
| ExtPAN ID    | `dead00beef00cafe`                 | `CONFIG_OPENTHREAD_NETWORK_EXTPANID`|
| Network key  | `00112233445566778899aabbccddeeff` | `CONFIG_OPENTHREAD_NETWORK_MASTERKEY`|

**First-time flash procedure:**

Both devices must start with clean NVS to avoid stale dataset conflicts:

```bash
idf.py erase-flash flash   # run on C6 first, then H2
```

**Boot order:** Flash and start the C6 first so it forms the network and wins leader
election. The H2 can then boot at any time — simultaneous boot also works because the
H2's leader weight (1) is always lower than the C6's.

**Dataset application logic (H2):** On boot, `thread_task` calls
`otDatasetIsCommissioned()`. If the dataset is already stored (subsequent boots) it
is used as-is. If not (first boot after erase-flash), `apply_fixed_dataset()` writes
the credentials from sdkconfig into the active dataset.

---

## 3 — Device discovery

Once the H2 attaches to the network (`OT_DEVICE_ROLE_CHILD` or
`OT_DEVICE_ROLE_ROUTER`), it sends a single **NON-CONFIRMABLE CoAP POST** to the
mesh-local all-nodes multicast address:

```
Destination : coap://[ff03::1]:5683/announce
Payload     : <device-name>           e.g. "Leuchtturm West"
```

This is sent exactly once per network attachment (`s_announced` flag). If the C6
restarts it must wait for the next H2 reboot to receive the announce again.

**What the C6 must implement:**

```c
otCoapAddResource(instance, &(otCoapResource){
    .mUriPath = "announce",
    .mHandler = on_announce,   // store sender IPv6 + name
    .mContext = instance,
});
otCoapStart(instance, OT_DEFAULT_COAP_PORT);
```

Record the sender's IPv6 address from `msg_info->mPeerAddr` — this is the unicast
address used for all subsequent CoAP communication with that device.

---

## 4 — Capability discovery

After discovery, query the H2 for its available resources:

```
GET coap://[<H2-addr>]:5683/.well-known/core
```

Response (Content-Format 40 — `application/link-format`):

```
</beacon>;rt="maerklin.switch";title="Blinken";sim;obs,
</outdoor>;rt="maerklin.switch";title="Aussenlicht";sim;obs,
</flicker>;rt="maerklin.value";min=0;max=100;step=5;title="Flackern in %";obs,
</group>;rt="maerklin.group";title="Group"
```

| Resource              | Type               | Semantics                                     | Observable |
| --------------------- | ------------------ | --------------------------------------------- | ---------- |
| `maerklin.switch`     | Boolean `"0"`/`"1"` | On/off toggle                                | yes        |
| `maerklin.value`      | Integer string     | Numeric value within min/max/step constraints | yes        |
| `maerklin.group`      | —                  | Group membership management                   | no         |

The `sim` attribute on `/beacon` and `/outdoor` indicates that these resources
participate in simulated lighting (the `sim/on/...` and `sim/off/...` action
namespace on the C6 side).

---

## 5 — Controlling resources

All unicast control requests use **CONFIRMABLE CoAP** to the H2's unicast IPv6
address on port **5683**.

### `/beacon` — rotating lighthouse light

| Method | Observe option | Payload | Response code |
| ------ | -------------- | ------- | ------------- |
| GET    | —              | —       | 2.05 Content, payload `"1"` or `"0"` |
| GET    | `0` (register) | —       | 2.05 Content + Observe seq + current state |
| GET    | `1` (cancel)   | —       | 2.05 Content |
| PUT    | —              | `"1"`   | 2.04 Changed |
| PUT    | —              | `"0"`   | 2.04 Changed |

Starts **off** on every boot.

### `/outdoor` — outdoor lamp (flickering flame effect)

| Method | Observe option | Payload | Response code |
| ------ | -------------- | ------- | ------------- |
| GET    | —              | —       | 2.05 Content, payload `"1"` or `"0"` |
| GET    | `0` (register) | —       | 2.05 Content + Observe seq + current state |
| GET    | `1` (cancel)   | —       | 2.05 Content |
| PUT    | —              | `"1"`   | 2.04 Changed |
| PUT    | —              | `"0"`   | 2.04 Changed |

Starts **off** on every boot.

### `/flicker` — flicker intensity

Controls the random-flicker depth of the outdoor lamp as a percentage (0 = no
flicker / steady, 100 = maximum flicker).

| Method | Observe option | Payload             | Response code |
| ------ | -------------- | ------------------- | ------------- |
| GET    | —              | —                   | 2.05 Content, payload `"<0–100>"` |
| GET    | `0` (register) | —                   | 2.05 Content + Observe seq + current value |
| GET    | `1` (cancel)   | —                   | 2.05 Content |
| PUT    | —              | `"<0–100>"` (step 5) | 2.04 Changed |

Values above 100 are clamped to 100.

---

## 6 — Observing resources (RFC 7641)

`/beacon`, `/outdoor`, and `/flicker` support CoAP Observe. The C6 registers once
per device; from that point on the H2 pushes a **NON-CONFIRMABLE** notification to
every registered observer whenever the resource state changes.

### Capacity

Each resource holds up to **4 observers** (`MAX_OBSERVERS`). Registration beyond
that limit is silently ignored.

### Re-registration handling

If the same peer registers with a new token (e.g. after a C6 restart), the H2
updates the existing slot in place rather than consuming a new one. This means a
restarted C6 can re-register without exhausting observer slots.

### Registration

```
GET coap://[<H2-addr>]:5683/beacon
Observe: 0
Token: <client-chosen token>
```

The H2 responds with the current state and the current Observe sequence number.
Keep the token — all incoming notifications carry the same token.

### Cancellation

```
GET coap://[<H2-addr>]:5683/beacon
Observe: 1
Token: <same token as registration>
```

Alternatively send a **RST** in response to any incoming notification.

### Notification format

```
NON 2.05 Content
Observe: <monotonic 24-bit seq>
Content-Format: text/plain
Token: <registration token>
Payload: "1" or "0"   (for /flicker: "<integer>")
```

Notifications are NON-CONFIRMABLE. The C6 does not need to ACK them. Observers
are never automatically evicted — the C6 must cancel explicitly when a device goes
away.

### C6 example

```c
void register_observe(otInstance *inst, otIp6Address *h2_addr, const char *resource)
{
    otMessage *msg = otCoapNewMessage(inst, NULL);
    otCoapMessageInit(msg, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_GET);
    otCoapMessageGenerateToken(msg, OT_COAP_DEFAULT_TOKEN_LENGTH);
    otCoapMessageAppendObserveOption(msg, 0);
    otCoapMessageAppendUriPathOptions(msg, resource);

    otMessageInfo info = {};
    info.mPeerAddr = *h2_addr;
    info.mPeerPort = OT_DEFAULT_COAP_PORT;

    otCoapSendRequest(inst, msg, &info, on_observe_notification, NULL);
}

void on_observe_notification(void *ctx, otMessage *msg,
                             const otMessageInfo *info, otError err)
{
    if (err != OT_ERROR_NONE) return;
    char buf[8] = {};
    otMessageRead(msg, otMessageGetOffset(msg), buf, sizeof(buf) - 1);
    // buf contains "1", "0", or a decimal integer (flicker)
}
```

---

## 7 — Group management

Groups use **IPv6 multicast**. The C6 assigns H2 devices to named groups by telling
each device which multicast address to subscribe to. A group command is then a single
CoAP request sent to the multicast address — all members receive it simultaneously.

### Joining a group

```
PUT coap://[<H2-addr>]:5683/group
Payload: "<multicast-addr>,1"

Example: "ff03::10,1"
```

### Leaving a group

```
PUT coap://[<H2-addr>]:5683/group
Payload: "<multicast-addr>,0"

Example: "ff03::10,0"
```

Response is `2.04 Changed` in both cases (no error response for invalid addresses —
the parse silently fails).

### Sending a group command

```
PUT coap://[ff03::10]:5683/outdoor
Payload: "1"
```

All H2 devices subscribed to `ff03::10` receive the packet and turn on their outdoor
lamp simultaneously.

### Persistence

Group memberships are persisted in NVS (namespace `thread`, key `groups`) as a
semicolon-separated list of IPv6 address strings. On every boot, after the device
attaches to the network, all previously stored memberships are restored via
`otIp6SubscribeMulticastAddress`. The C6 does not need to reassign memberships after
an H2 reboot.

### Recommended group address range

Use addresses in the **mesh-local multicast scope** `ff03::/16`:

| Address    | Suggested use         |
| ---------- | --------------------- |
| `ff03::10` | All outdoor lamps     |
| `ff03::11` | All beacons           |
| `ff03::20` | Scene: harbour lights |

---

## 8 — Status LED

The H2's status LED (driven by `beacon_set_status`) reflects the Thread network state:

| LED state              | `beacon_status_t`        | Meaning                          |
| ---------------------- | ------------------------ | -------------------------------- |
| Blue blinking (500 ms) | `BEACON_STATUS_SEARCHING` | Detached — waiting for network  |
| Off                    | `BEACON_STATUS_CONNECTED` | Child or Router — ready          |

`BEACON_STATUS_PAIRING` and `BEACON_STATUS_ERROR` are defined but not triggered by
the current Thread stack (they were used by the former Joiner flow).

---

## Summary — C6 implementation checklist

- [ ] Use identical Thread credentials in sdkconfig (`MaerklinNet`, ch 18, PAN `0xBEEF`, …)
- [ ] Erase flash on both devices before first use to clear stale NVS datasets
- [ ] Start C6 first so it wins leader election
- [ ] Listen for CoAP POST on `/announce` → store sender IPv6 + device name
- [ ] On new device: GET `/.well-known/core` → parse resource types and `obs` attribute
- [ ] Control via unicast PUT to `/beacon`, `/outdoor`, `/flicker`
- [ ] After discovery: register as observer (GET + `Observe: 0`) on `/beacon`, `/outdoor`, `/flicker`
- [ ] Handle incoming NON notifications → update device state in C6 model
- [ ] For group commands: PUT to multicast address + resource path
- [ ] Group membership is restored on H2 reboot — no reconciliation required
