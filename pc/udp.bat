@echo off
gst-launch-1.0.exe udpsrc port=2222 ! application/x-rtp,encoding-name=H265,payload=96 ! rtph265depay ! avdec_h265 ! fpsdisplaysink sync=false
pause
