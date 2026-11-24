# Wiring

All boards: ESP32 DevKit v1 (30/38-pin). Sensors run on 3.3 V unless noted.
Fan and servo use their own supply; **connect all grounds together**.

## Sensor node (outdoor)

| Part | Part pin | ESP32 pin | Note |
|---|---|---|---|
| PMS5003 | VCC | 5V (VIN) | needs 5 V for the fan inside |
| | GND | GND | |
| | TX | GPIO16 (RX2) | 3.3 V logic, direct |
| | RX | GPIO17 (TX2) | |
| DHT22 | VCC / GND | 3V3 / GND | use a 3.3 V compatible module |
| | DATA | GPIO18 | bare sensor needs a 4.7–10 kΩ pull-up from DATA to 3V3 |
| Rain module | VCC / GND | 3V3 / GND | |
| | AO | GPIO35 | ADC1 (ADC2 does not work with radio on) |
| MQ-2 smoke | VCC / GND | 5V (VIN) / GND | heater needs 5 V, warm-up ~1 min |
| | AO | GPIO34 **via divider** | AO -> 10 k -> GPIO34 -> 20 k -> GND (5 V -> 3.3 V max). **Never connect AO directly** |

Box: waterproof, sensor holes facing down; rain plate outside the box.

## Gateway (indoor)

| Part | Part pin | ESP32 pin | Note |
|---|---|---|---|
| SCD40 | VDD / GND | 3V3 / GND | I2C addr 0x62 |
| SHT31 | VIN / GND | 3V3 / GND | I2C addr 0x44 |
| BH1750 | VCC / GND | 3V3 / GND | I2C addr 0x23 (ADDR pin to GND) |
| OLED SSD1306 | VCC / GND | 3V3 / GND | I2C addr 0x3C |
| all I2C | SDA / SCL | GPIO21 / GPIO22 | one shared bus |
| PIR or LD2410 | OUT | GPIO27 | HIGH = person |
| Button AC | one leg | GPIO32 | other leg to GND (internal pull-up) |
| Button window | one leg | GPIO33 | other leg to GND |
| Button E-stop | one leg | GPIO25 | other leg to GND |
| Limit switch OPEN | COM / NO | GPIO26 / GND | pressed when window fully open |
| Limit switch CLOSED | COM / NO | GPIO14 / GND | pressed when window fully closed |
| Servo MG996R | signal | GPIO19 | power from 5 V buck, **not** from the ESP32 |
| Fan relay module | IN | GPIO18 | use a 3.3 V logic-compatible module; default input is active-low |
| Fan relay module | VCC / GND | supply per module / common GND | do not power the relay coil from an ESP32 GPIO |
| Relay contacts | COM / NO | in series with fan supply | use a contact rating above the fan's startup current |

## Power

```
12 V 2 A adapter ──┬── relay COM/NO ── 12 V fan
                   └── LM2596 buck (set to 5.0 V) ──┬── servo V+
                                                     └── ESP32 VIN
GND of adapter, buck, relay module, servo and ESP32 all tied together
```

- Put a 470-1000 uF capacitor across the servo supply (servo start current).
- Set `FAN_RELAY_ACTIVE_LOW` in `gateway/include/config.h` to match the relay module.
- The relay contacts must be rated for the fan's startup current; a DC motor fan can
  draw several times its running current when it starts.

## Receiver/web-server configuration

The current gateway role has no locally connected sensor/window tasks; the fan relay
is controlled from the NETPIE-fed model.
Connect MQ-135 AO to **node GPIO34** with a divider that keeps the ESP32 input at
or below 3.3 V, plus a common GND and the module's required heater supply.
The node's existing analog `smoke_raw` field carries this reading over ESP-NOW.
The gateway selects `MQ135_NODE_ID` (default `out-01`) and publishes to NETPIE.
The web server reads NETPIE Shadow back through HTTPS REST and exposes those
cloud values, their age and source status at `/api/status` and `/`.
