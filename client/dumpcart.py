#!/usr/bin/python3

# apt install python3-serial

# apt install python3-xmodem
#   or
# pip3 install xmodem

import serial, sys, logging, argparse, datetime
import serial.tools.list_ports
from xmodem import XMODEM

verbose_mode = False
rxbytes = 0
rxbytes2 = 0
last_exch_start_time_start = 0
last_exch_duration = 0

programmer_caps = [ ];
# firmware 1.0 did not have the 'version' command
programmer_version_str = "1.0"
# major major * 100 + minor
programmer_version = 100

def sendCommand(command):
    if command and verbose_mode:
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

def readProgrammerInfo():
    exchangeCommand("")
    exchangeCommand("")
    tmp = exchangeCommand("version")
    if "version" in tmp:
        lines = tmp.split("\r\n")
        tmpv = [v.split(": ")[1] for v in lines if "Version:" in v][0]
        programmer_version_str = tmpv
        major = int(tmpv.split(".")[0])
        minor = int(tmpv.split(".")[0])
        programmer_version = major * 100 + minor

        # Command introduced in version 1.2
        if programmer_version >= 102:
            programmer_caps.append("blankcheck")
            programmer_caps.append("setromsize")


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
parser.add_argument("-i", '--info', help='Provide information about the programmer and cartridge', action='store_true')
parser.add_argument("-b", '--blankcheck', help='Check if a FLASH cartridge is blank', action='store_true')
parser.add_argument("-r", '--read', help='Read the cartridge contents to a file.', type=argparse.FileType('wb'), dest='outfile')
parser.add_argument("-p", '--prog', help='(Re)program the cartridge with contents of file', type=argparse.FileType('rb'), dest='infile')
parser.add_argument("-d", "--device", help='Use specified character device.', action='store', default='/dev/ttyACM0')
parser.add_argument("-l", '--listports', help='List serial ports', action='store_true')
parser.add_argument("-v", '--verbose', help='Enable verbose output', action='store_true')
parser.add_argument('--bootloader', help='Restart programmer in bootloader for FW update', action='store_true')

args = parser.parse_args()
verbose_mode = args.verbose

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


if (args.bootloader):
    try:
        exchangeCommand("bootloader")
    except Exception as e:
        print("Programmer disconnected.")
        print("")

    print("Just sent the 'bootloader' command to the programmer, which causes the firmware to")
    print("jump into the built-in Atmel DFU bootloader.")
    print("")
    print("The programmer will not appear as a serial port until one of the conditions below occur:")
    print("")
    print("  - It is diconnected and reconnected to USB")
    print("  - The firmware is started through dfu-programmer 'start' command.")
    print("")
    print("You may now use dfu-programmer to manually program a new firmware.")
    print("")
    print("Typically the next steps are:")
    print("")
    print("  dfu-programmer atmega32u2 erase")
    print("  dfu-programmer atmega32u2 flash firmware.hex")
    print("  dfu-programmer atmega32u2 start")
    print("")
    exit()


if (args.blankcheck):
    readProgrammerInfo()
    if "blankcheck" not in programmer_caps:
        print("Error: Programmer firmware does not support blank check")
        exit()
    tmp = exchangeCommand("bc")
    print(tmp)



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


if (args.info):
    readProgrammerInfo()
    print("Programmer version:", programmer_version)
    tmp = exchangeCommand("")
    tmp = exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)
    tmp = exchangeCommand("")




ser.close()
print("Done.")
exit()
