# FPV Drone over 4G (QuadroFleet + Masina V3)

## Overview

This project provides a step-by-step guide to building an FPV drone capable of long-range operation over a 4G network.
It streams live video to a remote computer with latency typically under 100 ms.

The drone is controlled via a gamepad (Xbox or PlayStation) or a standard RC transmitter connected to the ground station (computer) via USB.
Key features include live video with on-screen telemetry, GPS tracking on an interactive map, and automated safety behaviors in the event of connectivity loss.

## Features

* **Low Latency Video:** Real-time video feed with latency <100 ms.
* **Connection Loss Handling:** Automatic hover, land, or Return-To-Home (RTH) when link is lost.
* **Live GPS Tracking:** Displays real-time drone location using OpenStreetMap.
* **Extended Range:** Significantly longer range compared to traditional 2.4GHz radio systems by using cellular networks.
* **On-Screen Display (OSD):** Telemetry such as battery voltage, compass heading, GPS coordinates, speed, and altitude is overlaid on the video.

## System Architecture

1. **Video & Telemetry Uplink**
   An OpenIPC-based camera streams video and handles control communication over a secure WireGuard VPN using UDP sockets.
   It transmits and receives Crossfire (CRSF) telemetry data via UART to/from the flight controller.

2. **Ground Station — *QuadroFleet* (Operator Side)**

   * Establishes a secure VPN tunnel to the drone using WireGuard.
   * Reads joystick input (throttle, pitch, roll, yaw) from a connected gamepad or RC transmitter.
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
   * If communication is not restored within 5 seconds (configurable), the drone performs an automatic landing or Return-To-Home (RTH), based on flight controller settings.

## Legal Compliance Notice:
**This device lets you control drones over 4G networks, making long-distance control possible. But before you jump into using this technology, it's essential to know the rules and regulations in your country. In many places, like Europe and the US, there are strict laws about how drones can be used. For example, flying a drone beyond your line of sight (VLOS) might be restricted unless you have special permissions. It's strongly recommended to check the laws and regulations where you plan to use your drone. It's your responsibility to follow the rules and ensure you're flying legally. Breaking these rules can lead to legal trouble, so stay safe and fly responsibly!**

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
| ---------------------- | ------------------------------------- |------------------------|
| **Drone Frame & Core** | Frame, Motors, FC + ESC, Props, GPS   | 174 €                  |
| **Video System**       | Camera module, 4G modem, antenna      | 69 €                   |
| **Power System**       | Buck converter, battery (3S–6S)       | 22–92 €                |
| **Miscellaneous**      | Wiring, connectors, mounting hardware | \~10 €                 |
|                        |                                       | **Total: \~275–345 €** |

## Wiring
![Wiring](images/wiring.png "Wiring")

## Setup
### Preparing the SD Card
* Download Raspberry Pi Imager from the [official website](https://www.raspberrypi.com/software/).
* Insert the MicroSD card into your computer's card reader.
* Open Raspberry Pi Imager and select the OS: **Raspberry Pi OS (Legacy 32-bit) Lite**.
* Set your username and password.
* Configure Wi-Fi settings.
* Save your settings and click Write to flash the SD card.
* Eject the SD card and insert it into your Raspberry Pi.

### Preparing the Raspberry Pi
1. Plug the camera to the CSI port.
2. Power it on.
3. SSH into the raspberry pi.
4. Enable the legacy camera interface and serial port using: `sudo raspi-config` and navigate to **Interface Options**
    1. Legacy Camera > Yes
    2. Serial Port > No > Yes
    3. Back > Finish > Yes
5. Update the OS: 
```
sudo apt update && sudo apt upgrade -y
sudo apt-get install git proftpd python3-pip libboost-all-dev build-essential libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-pulseaudio autoconf automake libtool pkg-config libraspberrypi-dev -y
```
6. Reboot: `sudo reboot` and ssh again
7. Install libraries
```
git clone https://github.com/thaytan/gst-rpicamsrc.git
cd gst-rpicamsrc/
./autogen.sh --prefix=/usr --libdir=/usr/lib/arm-linux-gnueabihf/
make
sudo make install
cd

wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.75.tar.gz
tar -xzvf bcm2835-1.75.tar.gz
cd bcm2835-1.75
./configure
make
sudo make install
cd
```

### Modem Configuration
* Insert the SIM card into the modem.
* Connect the 4G modem to the raspberry pi using microUSB -> USB OTG cable.

#### Method 1: ModemManager
This is the simpler way of connecting to the 4G network, but it doesnt have any reconnect protection.

`sudo crontab -e` -> choose 1 (nano) and to the bottom add: 

`@reboot sleep 30 && sudo mmcli -m 0 --enable && sudo mmcli -m 0 --simple-connect="apn=your_apn,ip-type=ipv4"`
replacing the "your_apn" with your mobile service provider´s access point name.

Save the file by holding `Ctrl+X`, then press `Y`
>To find your provider's access point name insert the SIM card to your phone. And navigate to mobile network settings, here you should find it.

#### Method 2: NetworkManager (recommended)
* If you have usb ethernet adapter and usb hub you can use that to connect the raspberry pi to your network and skip to step: *Edit the NetworkManager config: sudo nano /etc/NetworkManager/NetworkManager.conf*
* If not make a nmconnection file for your wifi, I highly recommend creating separate wifi network just for the drone so you can disable it when not debugging/setting it up, otherwise it might use wifi instead of the 4g modem and you will lose connection when trying to fly somewhere.


Create a new connection file: `sudo nano /etc/NetworkManager/system-connections/debug` and paste, replacing the `ssid` and `psk` with your own:
```
[connection]
id=debug
uuid=f1c34425-a556-4b27-9ed1-0f9dc6bdec74
type=wifi

[wifi]
mode=infrastructure
ssid=debug

[wifi-security]
key-mgmt=wpa-psk
psk=debugpassword

[ipv4]
method=auto

[ipv6]
ip6-privacy=2
method=auto

[proxy]
```
Save the file by holding `Ctrl+X`, then press `Y`

Modify the permissions: `sudo chmod 600 /etc/NetworkManager/system-connections/debug`

Edit the NetworkManager config: `sudo nano /etc/NetworkManager/NetworkManager.conf`

Remove everything and paste:
```
[main]
plugins=ifupdown,keyfile
dhcp=internal

[ifupdown]
managed=true
```
Save the file by holding `Ctrl+X`, then press `Y`

Disable wpa_supplicant and enable NetworkManager:
```
sudo systemctl disable dhcpcd && sudo systemctl disable wpa_supplicant && sudo systemctl enable NetworkManager && sudo reboot &
```
The system will reboot. So ssh back into the raspberry pi.

Now create the connection file for the modem: `sudo nmcli c add type gsm ifname '*' con-name MOBILE apn your_apn connection.autoconnect-priority 20` replacing the `your_apn` with your mobile service provider´s access point name.

>To find your provider's access point name insert the SIM card to your phone. And navigate to mobile network settings, here you should find it.

Check if the modem is active by typing `nmcli` and you should get something like this:
```
ttyUSB0: connected to MOBILE
    "Huawei Mobile Broadband"
wlan0: connected to your_wifi
    "Broadcom BCM43438 combo and Bluetooth Low Energy"
```
### Connecting to home
#### Method 1: PiTunnel (recommended)
* Open https://www.pitunnel.com create an account and follow instrucions.
* Ensure UDP ports 2222-2224 are open on your router.

#### Method 2: WireGuard VPN (use if you have linux server/pc at home)
* Setup WireGuard server on your home network: https://www.pivpn.io
* Open udp port on your router (default: 51820)
* add a client using `pivpn -a drone`
* copy the file to /etc/wireguard/wg0.conf on the raspberry pi using `sudo nano /etc/wireguard/wg0.conf` and add `PersistentKeepalive = 25` at the bottom
Enable the service on raspberry pi:
```
sudo systemctl enable wg-quick@wg0.service
sudo systemctl daemon-reload
sudo systemctl start wg-quick@wg0.service
```

### Setting up Dynamic DNS
Setting up Dynamic DNS (DDNS) allows you to avoid constantly changing the IP address whenever your network restarts. If your router supports DDNS, you can set it up directly. For example, you can create a free account on [No-IP](www.noip.com) and add your login credentials to your router's DDNS settings.

>If your router doesn't support DDNS, look up alternative methods and tutorials online to achieve this.

### Testing

At this point we should have working video stream over 4G.
So let's test it. First make sure we are connected to the 4G network by turning off the wifi network we setup just for this or if you used usb ethernet adapter unplug it.

Open [PiTunnel](https://www.pitunnel.com/devices) devices tab to verify your Raspberry Pi is online.

Click on Open Remote Terminal, enter your Pi's credentials, and run the command sudo nmcli. You should see:
```
ttyUSB0: connected to MOBILE
    "Huawei Mobile Broadband"
```
Enter the following GStreamer command, replacing your_ddns with your DDNS or [public IP address](http://checkip.amazonaws.com): 
`gst-launch-1.0 rpicamsrc bitrate=500000 preview=false ! video/x-h264,width=480, height=360,framerate=30/1 ! h264parse ! rtph264pay config-interval=1 pt=96 ! udpsink host=your_ddns port=2222`


On your computer navigate to your gstreamer folder, the default should be: `C:\gstreamer\1.0\msvc_x86_64\bin` open a terminal here by holding `Shift` and `Right click` and click on Open in Terminal/Open powershell window here.
Paste this command to the terminal: `gst-launch-1.0.exe udpsrc port=2222 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! avdec_h264 ! fpsdisplaysink sync=false`
And video window should appear and everything should be working.
>  If nothing opens, check for errors on the Raspberry Pi. Otherwise ensure your firewall isn't blocking the port 2222. If using a third-party antivirus, disable its firewall. Then go to **Windows Defender Firewall with Advanced Security** settings. Now on top left click on Inbound Rules and in the top right click New Rule > Port > UDP, Specific remote ports: 2222-2224 > Allow the connection > name it whatever you like > Finish. Repeat the same in Outbound Rules.

### Autostart video
Here you can choose whether you also want to record the video to a SD card.

#### Stream only
Create the stream service: `sudo nano /etc/systemd/system/cam_stream.service`
```
[Unit]
Description=Camera Stream
After=NetworkManager.service

[Service]
ExecStart=gst-launch-1.0 rpicamsrc rotation=180 bitrate=0 quantisation-parameter=32 preview=false ! video/x-h264,width=480,height=360,framerate=40/1 ! h264parse ! rtph264pay config-interval=1 pt=96 ! udpsink host=your_ddns port=2222
Restart=always
RestartSec=5

[Install]
WantedBy=default.target
```
Save the file by holding `Ctrl+X`, then press `Y`
Enable the service: `sudo systemctl enable cam_stream.service`

#### Stream and record
Create bash script that record the video with time as the filename: `sudo nano stream_and_save.sh`
```
#!/bin/bash

# Generate a timestamp
timestamp=$(date +"%Y%m%d_%H%M%S")

# Define the filename with the timestamp
filename="/home/pi/videos/video_$timestamp.mkv"

# Run the GStreamer pipeline
gst-launch-1.0 rpicamsrc rotation=180 bitrate=0 quantisation-parameter=32 preview=false ! \
    video/x-h264,width=480,height=360,framerate=40/1 ! h264parse ! \
    tee name=t ! \
    queue ! rtph264pay config-interval=1 pt=96 ! udpsink host=your_ddns port=2222 \
    t. ! \
    queue ! matroskamux ! filesink location=$filename

```
Modify the script permissions: `sudo chmod +x stream_and_save.sh`

Create a folder for videos: `sudo mkdir videos`

Create the stream service: `sudo nano /etc/systemd/system/cam_stream.service`
```
[Unit]
Description=Camera Stream
After=NetworkManager.service

[Service]
ExecStart=sh /home/pi/stream_and_save.sh
Restart=always
RestartSec=5

[Install]
WantedBy=default.target
```
Enable the service: `sudo systemctl enable cam_stream.service`

### Controls and telemetry (Pi)
Download the source: `git clone https://github.com/danielbanar/MasinaV3.git`

Enter the folder: `cd MasinaV3/client`
Edit the DDNS to yours: `sudo nano client.cpp` by changing the string on this line `#define HOSTNAME "your_ddns"`

Compile it by running: `make`
Move it to home folder: `cd` and then
`cp MasinaV3/client client -r`

Create a new service: `sudo nano /etc/systemd/system/client.service`
```
[Unit]
Description=UDP Client
After=NetworkManager.service

[Service]
ExecStart=sudo /home/pi/client/client
Restart=always
RestartSec=5

[Install]
WantedBy=default.target
```

### Controls and telemetry (PC)
Download the repository on your PC and open the solution file in Visual Studio: `pc\UDP_Server\UDP_Server.sln`.

Compile the project, preferably in **Release, x64**.

### Video
Add the gstreamer folder to your PATH environment variable by running this command as an administrator, or wherever you installed it.

`setx PATH "%PATH%;C:\gstreamer\1.0\mingw_x86_64\bin"`

#### Now you have two choices:
1. Run one of these gstreamer commands: 
```
-No frame info
gst-launch-1.0.exe udpsrc port=2222 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! avdec_h264 ! autovideosink sync=false

-With frame info
gst-launch-1.0.exe udpsrc port=2222 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! avdec_h264 ! fpsdisplaysink sync=false

-Frame info with scaled font (change the resolution to your own)
gst-launch-1.0.exe udpsrc port=2222 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! avdec_h264 ! videoscale ! video/x-raw,width=1280,height=960 ! fpsdisplaysink sync=false
```
2. Create a executable by compiling the solution: `pc\UDP_Server\UDP_Video.sln`.


## Troubleshooting
[Discord](https://discord.gg/3YTfeUXQ7p)
