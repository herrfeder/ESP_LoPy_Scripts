import socket
import struct
from network import LoRa
import os
import time
# A basic package header, B: 1 byte for the deviceId, B: 1 byte for the pkg size, %ds: Formated string for string
_LORA_PKG_FORMAT = "!BB%ds"
# A basic ack package, B: 1 byte for the deviceId, B: 1 bytes for the pkg size, B: 1 byte for the Ok (200) or error messages
_LORA_PKG_ACK_FORMAT = "BBB"
class NanoNode(object):

    def __init__(self,device_id, cmd="",mode="rx"):

        if mode == "rx":
            self.mode = "rx"
        if mode == "tx":
            self.mode = "tx"
                
        self.last_recv_buffer =  []
        self.last_device_id = ""

        self.to_send_buffer = []

        self.device_id = device_id
        self.cmd_chain = []
        if cmd:
            self.cmd_chain.append(cmd)

    def set_up_rx(self):
        self.lora = LoRa(mode=LoRa.LORA)
        self.loso = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.loso.setblocking(False)
        print("Setted up RX")
        time.sleep(3)
    def set_up_tx(self):
        self.lora = LoRa(mode=LoRa.LORA)
        self.loso = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.loso.setblocking(True)
        print("Setted up TX")
        time.sleep(3)

    def pdeb(self,message):
        print("%s DEBUG: %s"%(self.mode,message))

    def run(self):


        while(True):
            if self.mode == "rx":
                self.receive_mode()
            if self.mode == "tx":
                self.transmit_mode()

    def receive_mode(self):
                self.set_up_rx()
                while(True):
                    result = self.receive()
                    if result:
                        self.send("ack",self.cb_output_payload)
                        time.sleep(1)
                        self.mode = "tx"
                        time.sleep(1)
                        break

    def transmit_mode(self):
                self.set_up_tx()
                cmd = []
                cmd.append(input("Give cmd to execute:"))
                while(True): 
                    to_send = ""
                    if len(self.to_send_buffer)>0:
                        to_send = self.to_send_buffer.pop()
                        self.pdeb("There is something in the send_buffer")
                        print(to_send)
                    elif cmd != "":
                        to_send = cmd.pop()
                    elif len(self.cmd_chain)>0:
                        to_send = self.cmd_chain.pop()
                        self.pdeb("There is something in the commandchain")

                    else:
                        pass
                    if to_send:
                        self.send(to_send,self.cb_output_payload)
                        print("sent:")
                        print(to_send)
                        waiting_answ = True
                        while(waiting_answ):
                            recv_msg = self.loso.recv(256)
                            if (len(recv_msg) > 0):

                                device_id, pkg_len, ack = struct.unpack(_LORA_PKG_ACK_FORMAT, recv_msg) 
                                
                                #last_recv_len = recv_msg[1]
                                #device_id, pkg_len, msg = struct.unpack(_LORA_PKG_FORMAT % last_recv_len, recv_msg)

                                if (device_id == self.device_id):
                                    if (ack == 200):
                                        waiting_answ = False
                                        # If the uart = machine.UART(0, 115200) and os.dupterm(uart) are set in the boot.py this print should appear in the serial port
                                        print("got:")
                                        print("ACK")
                                    else:
                                        waiting_answ = False
                                        # If the uart = machine.UART(0, 115200) and os.dupterm(uart) are set in the boot.py this print should appear in the serial port
                                        print("Message Failed")
                                    
                        self.mode = "rx"
                        break


    def interpret_cmd(self,msg,cb=""):
        output = ""
        if msg == b'ls':
            dirs = os.listdir()
            dirs = '\n'.join(dirs)
            output = dirs[0:50]
            self.to_send_buffer.append(output)
        if cb != "":
            cb(output,self.cb_output_payload)

    def send(self, msg="", cb=""):
        if cb != "":
            self.loso.send(cb(msg))
        else:
            self.loso.send(msg)
        self.pdeb("finished sending")

    def cb_output_payload(self,msg):

        if msg == "ack":
            pkg = struct.pack(_LORA_PKG_ACK_FORMAT, self.last_device_id, 1, 200)
        else:
            pkg = struct.pack(_LORA_PKG_FORMAT % len(msg), self.device_id,len(msg),msg)
        return pkg

    def cb_send_i2c(self, cb=""):
        pass

    def receive(self):
        last_recv = self.loso.recv(200)
        if (len(last_recv) > 2):
            self.pdeb("finished receiving")
            last_recv_len = last_recv[1]
            self.last_device_id, pkg_len, msg = struct.unpack(_LORA_PKG_FORMAT % last_recv_len, last_recv)

            print('Device: %d - Pkg:  %s' % (self.last_device_id, msg))
            self.interpret_cmd(msg)
            return 1


