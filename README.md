# Overview

![image](https://github.com/droidecahedron/esb_multi/assets/63935881/17b1016d-957e-4745-af8d-fea1c5574c49)

Primary transmitter (PTX) will go round-robin and send a packet to each primary receiver (PRX). PRXs peripherals will send data back to central PTX via ACK payloads.

Press button 1 or 2 on a PRX to be on channel/address selection 1 or 2.
Press button 1 on the PTX to start the test. Make sure to start it after you assing the PRXs to each channel you want them on.

Pin29 is the PPI Toggle. Pin 31 is the application ESB callback toggle.

![image](https://github.com/droidecahedron/esb_multi/assets/63935881/b66ecdc6-a054-44c3-990f-8c63356a7170)

*PPI Toggle waveform*

For calculating theoretical best performance, visit https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/intro-to-shockburstenhanced-shockburst

### notes
Association: The transmitter and receiver need the same addr, channel, and packet config (e.g. num address bytes, CRC bytes, etc).
> Note: The on-air addresses are composed of a 2-4 byte long base address in addition to a 1 byte prefix address. Note that the nRF5 radio uses an alternating sequence of 0 and 1 as the preamble of the packet. Therefore, for packets to be received correctly, the most significant byte of the base address must not be an alternating sequence of 0 and 1, that is, it must not be 0x55 or 0xAA.

Channel selection: 2Mbps PHY wants a 1MHz channel separating each device. So start from channel 2, and have a channel in between. (1M PHY no lower than ch1, 2M ch2 or higher, ensure enough margin on higher end of channel spectrum close to 2483.5 MHz ISM band edge.)
independent channels are not necessary for this application, but I included it anyway.

changing channel: The module must be in an idle state to call this function. As a PTX, the application must wait for an idle state and as a PRX, the application must stop RX before changing the channel. After changing the channel, operation can be resumed.

## todo
BLE Fall back on that prx. Maybe 4th button can be for "BLE Fallback mode" for ease of access. Disable ESB, enable BLE.
