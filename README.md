[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) 

## Pinouts

### 6502

bits   | pins
-------|----------
a0..11 | 9..20
vss    | 21
d7..d0 | 26..33
R/W    | 34

### 2364 rom socket

bits    | pins
--------|----------
a7..a0  | 1..8
d0..d2  | 9..11
gnd     | 12
d3..d7  | 13..17
a11..a10| 18..19
CS      | 20
a12     | 21
a9..a8  | 22..23
vcc     | 24

```
[
  {"pin": 1, "name": "A7 (Address)"},
  {"pin": 2, "name": "A6 (Address)"},
  {"pin": 3, "name": "A5 (Address)"},
  {"pin": 4, "name": "A4 (Address)"},
  {"pin": 5, "name": "A3 (Address)"},
  {"pin": 6, "name": "A2 (Address)"},
  {"pin": 7, "name": "A1 (Address)"},
  {"pin": 8, "name": "A0 (Address)"},
  {"pin": 9, "name": "D0 (Data)"},
  {"pin": 10, "name": "D1 (Data)"},
  {"pin": 11, "name": "D2 (Data)"},
  {"pin": 12, "name": "GND (Ground)"},
  {"pin": 13, "name": "D3 (Data)"},
  {"pin": 14, "name": "D4 (Data)"},
  {"pin": 15, "name": "D5 (Data)"},
  {"pin": 16, "name": "D6 (Data)"},
  {"pin": 17, "name": "D7 (Data)"},
  {"pin": 18, "name": "A11 (Address)"},
  {"pin": 19, "name": "A10 (Address)"},
  {"pin": 20, "name": "/CS (Chip Select)"},
  {"pin": 21, "name": "A12 (Address)"},
  {"pin": 22, "name": "A9 (Address)"},
  {"pin": 23, "name": "A8 (Address)"},
  {"pin": 24, "name": "VCC (+5V Power)"}
]


[
  {
    "partNumber": "325302-01",
    "addressSpace": "$C000–$DFFF",
    "description": "Commonly known as the DOS Low ROM. This 8 KB chip contains the core Commodore DOS file management routines, command parsing logic, and LED status controls. Because it was highly stable from the early production days of the long-board drives, it was carried over unchanged into the short-board revisions."
  },
  {
    "partNumber": "901229-05",
    "addressSpace": "$E000–$FFFF",
    "description": "Known as the DOS High ROM or drive Kernal. This chip contains the critical serial bus communication handlers, low-level disk formatting controllers, and vector tables. It was engineered specifically for the short board to resolve timing bugs and sync issues inherent to the redesigned, cost-reduced motherboard layout."
  }
]
```


<img src="image/romulon.jpg" alt="romulon" width="66%"/>

# nitrologic romulon 0.01

## Catching the 8 bit bus the IC formerly known as UB3

<img src="image/desk1.jpg" alt="desk" width="66%"/>

## 1541 drive

<img src="image/schematic-1540-049.gif" alt="drawing" width="166%"/>


