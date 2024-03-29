# ESB Multilink
<p align="center">
  <img src="https://github.com/droidecahedron/esb_multi/assets/63935881/3cf10ea0-b65f-4e2e-9ea2-f58c3166ffaa">
</p>

# Overview
Primary transmitter (PTX) will go round-robin and send a packet to each primary receiver (PRX). PRXs peripherals will send data back to central PTX via ACK payloads.
> NOTE: The present version of the application only supports 1 central PTX 2 peripheral PRX. This was my choice due to the # of buttons on the DK where you choose which PRX you want to be. At some point, I may add CLI as the user input to make it more flexible.

# Usage
- Press button 1 or 2 on a PRX to be on channel/address selection 1 or 2.
- Press button 1 on the PTX to start an ESB transmit loop. Make sure to start it after you assing the PRXs to each channel you want them on.
- Press button 3 on the PRX to swap to be a BLE LBS application. If the PRX is in the process of being spammed by the PTX in this application, you will not be able to swap from ESB to BLE due to the priorities. The intention of BLE is a fall-back communication method, so remove the PTX from the network in order to use the RF Swap button.

# Testing/running application
Probe Pin29 for the PPI Toggle (RADIO ACTIVITY). Pin 31 is the application ESB callback toggle in software.

> For calculating theoretical best ESB performance, visit this [blog](https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/intro-to-shockburstenhanced-shockburst).

Logging is in deferred mode to avoid slogging down the ESB callback in its default state.

You can search for esb_ble as a name filter with the [nRF Connect for Mobile App](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-mobile) when you RF Swap.

<p align="center">
  <img src="https://github.com/droidecahedron/esb_multi/assets/63935881/b66ecdc6-a054-44c3-990f-8c63356a7170" width=75% height=25%>
</p>
<p align="center"><i>Img 1. PPI Toggle waveform for radio activity</i></p>
<p align="center">
  <img src="https://github.com/droidecahedron/esb_multi/assets/63935881/1c215178-7ab8-4a4a-8c41-fad8457cdf4c" width=25% height=25% class="center">
</p>
<p align="center"><i>Img 2. BLE connectable device when you RF swap in nRF Connect for iOS</i></p>



# Requirements
## HARDWARE
`nRF52840DK`
>  Will work on any nRF52 series device. 53 there is some work to be done. Due to using short radio ramp up time, it will not work on older devices.
<p align="center">
  <img src="https://github.com/droidecahedron/nrf-blueberry/assets/63935881/12612a0e-9f81-4431-8b22-f69704248f89" width=25% height=25%>
</p>

## SOFTWARE
`nRF Connect SDK v2.6.0`

### notes
Association: The transmitter and receiver need the same addr, channel, and packet config (e.g. num address bytes, CRC bytes, etc).
> Note: The on-air addresses are composed of a 2-4 byte long base address in addition to a 1 byte prefix address. Note that the nRF5 radio uses an alternating sequence of 0 and 1 as the preamble of the packet. Therefore, for packets to be received correctly, the most significant byte of the base address must not be an alternating sequence of 0 and 1, that is, it must not be 0x55 or 0xAA.

Channel selection: 2Mbps PHY wants a 1MHz channel separating each device. So start from channel 2, and have a channel in between. (1M PHY no lower than ch1, 2M ch2 or higher, ensure enough margin on higher end of channel spectrum close to 2483.5 MHz ISM band edge.)
independent channels are not necessary for this application, but I included it anyway.

changing channel: The module must be in an idle state to call this function. As a PTX, the application must wait for an idle state and as a PRX, the application must stop RX before changing the channel. After changing the channel, operation can be resumed.

Round-trip latency: Realistically you should probably double-ping from the PTX if your response depends on input from the PTX. A data packet, then a second exchange to pick up the ACK data from the PRX.

> TODO:
> Clean up, abstract ESB and IO portion to their own files.
> CRITICAL: Debug BLE Connection process
