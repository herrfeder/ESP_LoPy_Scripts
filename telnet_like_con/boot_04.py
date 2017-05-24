
from network import WLAN
from machine import UART
import os
import pycom
import machine

uart = UART(0, 115200)
os.dupterm(uart) 

wlan = WLAN(mode=WLAN.STA)

pycom.heartbeat(False)
pycom.rgbled(0xff00)

if machine.reset_cause() != machine.SOFT_RESET:
        wlan.init(mode=WLAN.STA)
        # configuration below MUST match your home router settings!!
        wlan.ifconfig(config=('10.0.0.4', '255.255.255.0', '10.0.0.1', '8.8.8.8'))

nets = wlan.scan()
for net in nets:
    if net.ssid == 'wpa_00':
        print('Network found!')
        wlan.connect(net.ssid, auth=(net.sec, 'horst123'), timeout=5000)
        while not wlan.isconnected():
            machine.idle() # save power while waiting
        print('WLAN connection succeeded!')
        break
    else:

        wlan = WLAN(mode=WLAN.AP)
        wlan.init(mode=WLAN.AP,ssid="lopy_04",auth=(WLAN.WPA2,"horst123"), channel=1,antenna=WLAN.INT_ANT)
