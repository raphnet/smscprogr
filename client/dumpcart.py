#!/usr/bin/python3

# apt install python3-serial

# apt install python3-xmodem
#   or
# pip3 install xmodem

import serial, sys, logging, argparse, datetime
import serial.tools.list_ports
from xmodem import XMODEM

rxbytes = 0
rxbytes2 = 0
last_exch_start_time_start = 0
last_exch_duration = 0

def sendCommand(command):
    print("Sending command: " + command)
    ser.write(bytes(command + "\r\n", "ASCII"))
    ser.flush()

def sendAbort():
    ser.write(b'\x18')
    ser.flush()
    ser.write(b'\x03')
    ser.flush()


def exchangeCommand(command, ender="\r\n> ", atEnd=True):
    global last_exch_duration
    global last_exch_start_time_start
    last_exch_start_time_start = datetime.datetime.now()
    ser.reset_input_buffer()    # Drop any unread characters
    sendCommand(command)        # Send the command
    # Now wait to the answer
    answer = ""
    ser.timeout = 0.1
    while True:
        b = ser.read(100)
        if len(b) > 0:
            #print(b.decode("ascii"))
            answer = answer + b.decode("ascii", "ignore")
            if atEnd:
                if answer[-len(ender):] == ender:
                    break
            else:
                if ender in answer:
                    break
    last_exch_duration = (datetime.datetime.now() - last_exch_start_time_start).total_seconds();
    return answer


def download(outfile):
    print("Starting download")
    # Initiate xmodem download
    exchangeCommand("dx", "CTRL+C to cancel.\r\n")

    xm = XMODEM(getc,  putc)
    print("Downloading", end="", flush=True)
    n = xm.recv(outfile, crc_mode=False, retry=102, quiet=False )
    print("") # newline

    print("Bytes received: " + str(n))
    return n


def upload(infile):
    print("Starting upload")
    time_start = datetime.datetime.now()
    # Initiate xmodem download
    exchangeCommand("ux", "READY. Please start uploading.\r\n", atEnd=False)

    xm = XMODEM(getc,  putc)
    print("Uploading", end="", flush=True)
    n = xm.send(infile, retry=102, quiet=False )
    print("") # newline
    duration = (datetime.datetime.now() - time_start).total_seconds();
    if n:
        print("Upload completed with success in ", duration, "seconds")
    else:
        print("Upload error")
    return n




# Glue functions for xmodem
def getc(size, timeout=1):
    global rxbytes, rxbytes2
    ser.timeout = timeout
    dat = ser.read(size)

    # Display a . each 1k
    rxbytes = rxbytes + len(dat)
    if rxbytes - rxbytes2 > 1024:
        print(".", end="", flush=True)
        rxbytes2 = rxbytes

    return dat

def putc(data, timeout=1):
    ser.timeout = timeout
    n = ser.write(data)
    ser.flush()
    return n


logging.basicConfig(filename='example.log', level=logging.DEBUG)



parser = argparse.ArgumentParser(description='Dump/Program tool for smscprogr')
parser.add_argument("-i", '--info', help='Provide information about the cartridge', action='store_true')
parser.add_argument("-r", '--read', help='Read the cartridge contents to a file.', type=argparse.FileType('wb'), dest='outfile')
parser.add_argument("-p", '--prog', help='(Re)program the cartridge with contents of file', type=argparse.FileType('rb'), dest='infile')
parser.add_argument("-d", "--device", help='Use specified character device.', action='store', default='/dev/ttyACM0')
parser.add_argument("-l", '--listports', help='List serial ports', action='store_true')
args = parser.parse_args()


if (args.listports):
    portlist = serial.tools.list_ports.comports()
    for port in portlist:
        print(port.name, "(", port.device, ")")
    exit()

try:
    ser = serial.Serial(args.device, 115200, 8)
except Exception as e:
        print(e)
        print("Could not open serial port")
        print("Try -l to list available ports?")
        exit()

if (args.info):
    sendAbort()
    tmp = exchangeCommand("")
    tmp = exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)
    tmp = exchangeCommand("")



if (args.outfile != None):
    sendAbort()
    tmp = exchangeCommand("")
    tmp = exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)
    download(args.outfile)
    tmp = exchangeCommand("")



if (args.infile != None):
    sendAbort()
    tmp = exchangeCommand("")
    tmp = exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)
    tmp = exchangeCommand("ce")
    print(tmp)
    print("Chip erase completed in", last_exch_duration, " seconds")
    upload(args.infile)
    tmp = exchangeCommand("")



ser.close()
print("Done.")
exit()
