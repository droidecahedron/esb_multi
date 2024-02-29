# Overview
Primary transmitter (PTX) will go round-robin and send a packet to each primary receiver (PRX). PRXs peripherals will send data back to central PTX via ACK payloads.

## todo
PTX update addr to go between each PRX.
iterate and send a packet to each prx.

PRX - button to set up which addr he has. 4 buttons, 4 channels, easy enough.
end: BLE Fall back on that prx. Maybe 4th button can be for "BLE Fallback mode" for ease of access. Disable ESB, enable BLE.

maybe: add radio notifications for pintgl speed test.
