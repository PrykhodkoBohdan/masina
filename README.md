# FPV Drone over 4G (QuadroFleet + MasinaV3)

## Overview

This project provides a step-by-step guide to building an FPV drone capable of long-range operation over a 4G network.
It streams live video to a remote computer with latency typically under 100 ms.

The drone is controlled via a gamepad (Xbox or PlayStation) or a standard RC transmitter connected to the ground station (computer) via USB.
Key features include live video with on-screen telemetry, GPS tracking on an interactive map, and automated safety behaviors in the event of
connectivity loss.

## Features

* **Low Latency Video:** Real-time video feed with latency <100 ms.
* **Connection Loss Handling:** Automatic hover, land, or Return-To-Home (RTH) when a link is lost.
* **Live GPS Tracking:** Displays real-time drone location using OpenStreetMap.
* **Extended Range:** Significantly longer range compared to traditional 2.4 GHz radio systems by using cellular networks.
* **On-Screen Display (OSD):** Telemetry such as battery voltage, compass heading, GPS coordinates, speed, and altitude is overlaid on the video.

## System Architecture

1. **Video & Telemetry Uplink**
   An OpenIPC-based camera streams video and handles control communication over a secure WireGuard VPN using UDP sockets.
   It transmits and receives Crossfire (CRSF) telemetry data via UART to/from the flight controller.

2. **Ground Station — *QuadroFleet* (Operator Side)**

    * Establishes a secure VPN tunnel to the drone using WireGuard.
    * Reads joystick input (throttle, pitch, roll, yaw, etc.) from a connected gamepad or RC transmitter.
    * Encodes input into CRSF channel frames and sends them to the drone via UDP.
    * Receives and decodes telemetry data from the drone.
    * Overlays telemetry on the live video stream and updates the drone's position on an interactive map.

3. **Drone**

    * Connects to the ground station over WireGuard VPN.
    * Receives CRSF control frames via UDP and forwards them to the flight controller via UART.
    * Collects telemetry from the flight controller and sends it back to the ground station over UDP.
    * Streams the video feed in H.265 format to the ground station using UDP.

4. **Safety & Resilience**

    * If the connection is lost for more than 250 ms (configurable), the drone switches to hover mode.
    * If communication is not restored within 5 seconds (configurable), the drone performs an automatic landing or Return-To-Home (RTH), based on
      flight controller settings.

## Legal Compliance Notice:

**This device lets you control drones over 4G networks, making long-distance control possible. But before you jump into using this technology, it's
essential to know the rules and regulations in your country. In many places, like Europe and the US, there are strict laws about how drones can be
used. For example, flying a drone beyond your line of sight (VLOS) might be restricted unless you have special permissions. It's strongly recommended
to check the laws and regulations where you plan to use your drone. It's your responsibility to follow the rules and ensure you're flying legally.
Breaking these rules can lead to legal trouble, so stay safe and fly responsibly!**

## Images

![Front](images/front.png "Front")
![Left](images/left.png "Left")
![Right](images/right.png "Right")

## Prerequisites

Before getting started, ensure the following requirements are met:

* Access to a configured **WireGuard VPN** server (used for secure communication between drone and ground station).
* A stable **internet connection** on the ground station (QuadroFleet application).
* **Reliable 4G coverage** at the drone’s location with at least **200 kB/s** sustained upload speed (higher recommended).
* A **data-enabled SIM card** with a sufficient monthly quota.
* A flight controller running a properly configured version of **BetaFlight** or **iNAV** (with UART CRSF enabled).
* **GStreamer** installed on the ground station:
  Download the [MSVC 64-bit runtime and development packages](https://gstreamer.freedesktop.org/download) if using Windows.

## Hardware

1. Drone base
    1. [Mark4 7-inch frame](https://aliexpress.com/item/1005007050005578.html) - 16 €
    2. [iFlight XING E Pro 2207 1800KV 6S](https://aliexpress.com/item/1005006356256645.html) - 50 €
    3. [SpeedyBee F405 V4 FC 55A ESC Stack](https://aliexpress.com/item/1005006809684035.html) - 80 €
    4. [Gemfan Hurrikan MCK v2](https://aliexpress.com/item/1005006741863428.html) - 3 € for 2 pairs
    5. (optional although highly recommended) [GEP-M10Q](https://aliexpress.com/item/1005004752373178.html) - 25 €
2. [SSC30KQ + 1.7mm](https://de.aliexpress.com/item/1005006835439125.html) - 27 €
3. [Quectel EC25](https://de.aliexpress.com/item/1005002330780040.html) - 40 €
6. [DC-DC 12V-5V 3A Buck Converter](https://de.aliexpress.com/item/1005002163078645.html) - 2 €
7. [4G FPC-Antenna Signal Booster](https://de.aliexpress.com/item/1005004592746304.html) - 2 €
8. XBox/Playstation/FPV remote controller
9. 3S/6S Battery pack - 20-90 € or DIY
10. SIM Card with enough mobile data
11. Wires and connectors TODO

| Category               | Components Included                   | Approx. Cost (€)       |
|------------------------|---------------------------------------|------------------------|
| **Drone Frame & Core** | Frame, Motors, FC + ESC, Props, GPS   | 174 €                  |
| **Video System**       | Camera module, 4G modem, antenna      | 69 €                   |
| **Power System**       | Buck converter, battery (3S–6S)       | 22–92 €                |
| **Miscellaneous**      | Wiring, connectors, mounting hardware | \~10 €                 |
|                        |                                       | **Total: \~275–345 €** |

## Wiring

![Wiring](images/wiring.png "Wiring")

## Setup

### Preparing the Client firmware for OpenIPC

1. You need some Linux-based system to compile the OpenIPC firmware, Ubuntu is recommended.
2. Download the OpenIPC firmware from [OpenIPC official repository](https://github.com/OpenIPC/firmware.git)
3. Install compilers:
   ```
   sudo apt install g++-arm-linux-gnueabihf
   ```
4. Download QuadroFleet-Masina project (`opt` branch) from [GitHub](https://github.com/beep-systems/quadrofleet-masina.git)
5. Build the client by running the following commands:
   ```
   cd quadrofleet-masina/client
   make clean
   make
   ```
6. Copy and replace the `quadrofleet-masina/client/drop` files from the QuadroFleet-Masina project to the OpenIPC firmware root folder.
7. Build the OpenIPC firmware by running the following commands:
   ```
   cd firmware
   make
   ```
8. Select `SSC30KQ_4G` or `SSC338Q_4G` as the target device and wait till the end of building.
9. The firmware will be generated in the `firmware/output/images` folder. The file names will be something like `rootfs.squashfs.ssc30kq` and
   `uImage.ssc30kq`.
10. How to flash these files, you can find in the [OpenIPC wiki](https://github.com/OpenIPC/wiki/blob/master/en/installation.md)
11. Since the image files are slightly non-standard in size, you must manually specify the block sizes for kernel and rootfs. Size of images (
    `0x1fdd68` or `0x8ea000`) ay be different (`192.168.178.66` is local tftp server).
   ```
   setenv serverip 192.168.178.66
   setenv kernsize 0x300000
   setenv rootaddr 0x350000
   setenv rootsize 0xA00000
   setenv rootmtd 10240k
   
   setenv bootargs 'console=ttyS0,115200 panic=20 root=/dev/mtdblock3 init=/init mtdparts=NOR_FLASH:256k(boot),64k(env),3072k(kernel),${rootmtd}(rootfs),-(rootfs_data) LX_MEM=${memlx} mma_heap=mma_heap_name0,miu=0,sz=${memsz}'
   saveenv
   tftp 0x21000000 uImage.ssc30kq
   sf probe 0; sf erase 0x50000 0x300000; sf write 0x21000000 0x50000 0x1fdd68
   
   tftp 0x21000000 rootfs.squashfs.ssc30kq
   sf probe 0; sf erase 0x350000 0xA00000; sf write 0x21000000 0x350000 0x8ea000
   saveenv
   reset
   ```
12. Alternatively, you can download the precompiled firmware from
    the [QuadroFleet-Masina repository](https://quadrofleet.com/downloads/firmware/ssc30kq.bin) and flash it to the camera
    using [CH341A](https://de.aliexpress.com/item/1005006530290946.html) and [NeoProgrammer 2.2.0.10](https://quadrofleet.com/downloads/np.zip).
   ```
   Device: GD25Q128x [3.3V]
   Type: SPI NOR 25xx
   BitSize: 128 Mbits
   Manuf: GIGADEVICE
   Size: 16777216 Bytes
   Page: 256 Bytes
   ```
13. If you later want to update firmware, you can do it like that (`192.168.178.66` is a local webserver):
   ```
   cd /tmp
   curl -O http://192.168.178.66/rootfs.squashfs.ssc30kq
   curl -O http://192.168.178.66/uImage.ssc30kq

   soc=$(fw_printenv -n soc)
   sysupgrade --kernel=/tmp/uImage.${soc} --rootfs=/tmp/rootfs.squashfs.${soc} -z --force_ver -n
   ```
14. After flashing, the camera will boot into OpenIPC firmware. You can access the web interface by the IP address given by DHCP.
15. If you need to make some changes to the camera settings, you can do it directly in the web interface.

### Preparing the EC25-EU Modem (activating Ethernet (RNDIS) interface)

1. Plug the modem to the USB port of PC.
2. Run Putty or any other terminal program and connect to the modem using COM-port and the following settings:
   ```
   Baud rate: 115200
   Data bits: 8
   Stop bits: 1
   Parity: None
   Flow control: None
   ```
3. Execute the following commands:
   ```
   AT+QCFG="usbnet",1
   AT+CFUN=1,1
   ```
4. Wait for the modem to reboot.
5. After reboot, the modem should be recognized as a network device.

### OpenIPC settings

* Insert the SIM card into the modem.
* Connect the 4G modem to the raspberry pi using microUSB -> USB OTG cable.

#### Client configuration
1. Open the web interface of the camera.
2. Go to the **Extension** >> ***Wireguard* menu and set the following settings:
   ```
   host=10.253.0.3
   LOCAL_TIMEOUT=300000
   FAILSAFE_TIMEOUT=5000
   STABILIZE_TIMEOUT=250
   ELRS_SWITCH_PIN=0
   CONTROL_PORT=2223
   CAM_INFO_PORT=2224
   ```

#### WireGuard

1. Install WireGuard on the VPS. Config file `/etc/wireguard/wg0.conf`:
   ```
   [Interface]
   Address = 10.253.0.1/24
   ListenPort = 51820
   PrivateKey = sHT9ILg72IVOt+GSNE5+qPJ5Pl5FVOmt9pDMv5MPSFM=
   
   [Peer]
   # Client Desktop
   PublicKey = VGk8qCxEZeolwLZgoYl0AY+mb27pmQDURcmxUPzVtnk=
   AllowedIPs = 10.253.0.3/32
   
   [Peer]
   # Client OpenIPC
   PublicKey = TmT7JJVDwzjQ23Tzu72wTCWHMjmr6m9lwa1BQAyW/Ec=
   AllowedIPs = 10.253.0.2/32
   ```
2. Install WireGuard on the PC. Config file:
   ```
   [Interface]
   PrivateKey = yIXhUB6z7/Wiz48bTarzAlNqgnUCtn2xNjOIbcPcjG4=
   Address = 10.253.0.3/24
   
   [Peer]
   PublicKey = lY2vOP917/9qPZxDwkSHdcHivaidsDEsE02pxmVV708=
   AllowedIPs = 10.253.0.0/24
   Endpoint = 217.154.192.211:51820
   PersistentKeepalive = 25
   ```
3. Open the web interface of the camera.
4. Go to the **Extension** >> ***Wireguard* menu and set the following settings:
5. Update WireGuard config:
   ```
   [Interface]
   PrivateKey = QEslU7u45Q66Ax203kr19qM9x9DgpMi+yPWSY0nhmXY=
   
   [Peer]
   PublicKey = lY2vOP917/9qPZxDwkSHdcHivaidsDEsE02pxmVV708=
   AllowedIPs = 10.253.0.0/24
   Endpoint = 217.154.192.211:51820
   PersistentKeepalive = 25
   ```
6. Update WireNetwork interface config:
   ```
   auto wg0
   iface wg0 inet static
      address 10.253.0.2
      netmask 255.255.255.0
      pre-up modprobe wireguard
      pre-up ip link add dev wg0 type wireguard
      pre-up wg setconf wg0 /etc/wireguard.conf
      post-down ip link del dev wg0
   ```

#### Video stream settings
1. Open the web interface of the camera.
2. Go to the **Majestic** >> ***Settings* menu and set the following settings:
3. Video0 settings:
   ```
   Video codec: h265
   Video resolution: 960x720
   Video frame rate: 60
   Video bitrate: 1024
   ```
4. Or (optional) you can use **Camera runtime settings**:
   ```
   FPS: 60
   Bitrate: 1024 kbps
   Video resolution: 960x720 4:3
   ```

### Flight controller

#### Channel mapping
   ```
   CH1		Axis Roll:                          AXIS_X
   CH2		Axis Pitch:                         AXIS_Y
   CH3		Axis Throttle:                      AXIS_Z
   CH4		Axis Yaw:                           AXIS_RX
   
   CH5		Switch LT (Armed):                  AXIS_THROTTLE
   CH6		Switch LB (Acro, Horizon, Angle):   AXIS_RY
   CH7		Switch RT (Alt hold):               AXIS_RUDDER
   CH8		Switch RB (Fail safe):              AXIS_RZ
   ```

### QuadroFleet Android application

## Some random stuff
```
gst-launch-1.0 udpsrc port=10900 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! avdec_h264 ! d3dvideosink sync=false
---
gst-launch-1.0 udpsrc port=10900 ! application/x-rtp, encoding-name=H265, payload=96 ! rtph265depay ! h265parse ! avdec_h265 ! d3dvideosink sync=false
---
Here im testing EG25 to EG25 stream using wireguard server
Right (good) : gst-launch-1.0 udpsrc port=2222 ! application/x-rtp,encoding-name=H265,payload=96  ! rtph265depay ! avdec_h265 ! fpsdisplaysink sync=false
Left: gst-launch-1.0 udpsrc port=2222   ! application/x-rtp,encoding-name=H265,payload=96   ! rtpjitterbuffer ! rtph265depay   ! avdec_h265   ! fpsdisplaysink sync=false
---

```

## Troubleshooting
[Discord](https://discord.gg/3YTfeUXQ7p)
