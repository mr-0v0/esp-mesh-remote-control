# ESP-NOW Mesh Doorbell Relay

Remote doorbell system built on ESP32 + relays using an **ESP-NOW mesh-style broadcast**.  
Any node that detects a button press sends an encrypted ESP-NOW broadcast;  
every node that receives it activates its relay (doorbell, buzzer, light, etc).

One button can drive **one or many** doorbells with **no Wi-Fi router required**.

---

## Features

- **Mesh-style broadcast** using ESP-NOW (no central hub, no Wi-Fi AP)
- **Encrypted communication** – all nodes share the same key
- **One-to-many control** – one switch can trigger multiple doorbell nodes
- Relay output for **doorbells / buzzers / chimes / lights**
- Low latency – near-instant reaction from button press to relay click
- Configurable relay ON time, debounce, and node role (button / relay / both)

---

## How It Works

1. **All ESP32 nodes share the same ESP-NOW encryption key.**
2. A node with a **button input** detects a press.
3. It sends a **broadcast ESP-NOW message**.
4. Any node that:
   - is in range
   - has the **same encryption key**
5. …will receive the message and **turn on its relay** for a configured duration.

There’s no routing or complex mesh protocol here—just simple encrypted broadcast.
However, because any node can both **send** and **receive**, it behaves like a simple mesh of peers.

---

