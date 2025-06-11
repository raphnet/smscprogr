#!/usr/bin/python3

# apt install python3-serial

# apt install python3-xmodem
#   or
# pip3 install xmodem

import serial, sys, logging, argparse, datetime, io, os, subprocess, time
import serial.tools.list_ports
from xmodem import XMODEM

verbose_mode = False
trxbytes = 0
trxbytes2 = 0
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
            if verbose_mode:
                print(b.decode("ascii"))
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
    global programmer_version
    global programmer_version_str
    exchangeCommand("")
    exchangeCommand("")
    tmp = exchangeCommand("version")
    if "version" in tmp:
        lines = tmp.split("\r\n")

        versionLine = [v.split(": ")[1] for v in lines if "Version:" in v]

        # Version 1.0 will return an ERROR and no version information
        if versionLine:
            tmpv = versionLine[0]
            programmer_version_str = tmpv
            major = int(tmpv.split(".")[0])
            minor = int(tmpv.split(".")[1])
            programmer_version = major * 100 + minor

        # Command introduced in version 1.2
        if programmer_version >= 102:
            programmer_caps.append("blankcheck")
            # programmer_caps.append("setromsize") # broken - fixed in 1.3

        # Command introduced in version 1.2
        if programmer_version >= 103:
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


def downloadToBuffer():
    f = io.BytesIO()
    try:
        download(f)
        return f.getvalue();
    except BaseException as e:
        print(e)

    return None


def updateFirmware(filename):
    # Check if the update file exists, and keep it in
    # filename.
    print("Update file:", filename)

    if not os.path.isfile(filename):
        # This probably cannot happen through the GUI, but still...
        print("File not found:", filename)
        return False
    if not os.access(filename, os.R_OK):
        print("Cannot access file: ", filename)
        return False

    # Quick test for dfu-programmer presence
    print("Checking for dfu-programmer...")
    command = [ "dfu-programmer", "--help" ];
    result = subprocess.run(command, capture_output=True);
    #print(result)
    # stderr should contain help from --help, look for it
    if not "global-options" in ascii(result.stderr):
        print("Error: dfu-programmer not found")
        return False

    # Good, dfu-programmer is there.

    # Detect if already in boot loader (unprogrammed board, previously interrupted attempt, etc)
    print("Checking for device already in bootloader...")
    command = [ "dfu-programmer", "atmega32u2", "get" ]
    result = subprocess.run(command, capture_output=True);
    if not "Bootloader Version" in ascii(result.stdout):
        # If not already in bootloader, communicate with the programmer
        # to enter bootloader
        print("Entering bootloader...")

        try:
            exchangeCommand("bootloader")
        except Exception as e:
            # receiving an exception is normal
            print("Programmer disconnected.")
            print("")


    # If the programmer was not already in bootloader mode, it can take a few moments
    # for it to re-enumerate as a DFU device.

    attempts = 40 # Timeout of ~10 seconds with the .25 sleep in the loop
    while True:
        print("Waiting for bootloader...")

        # $ dfu-programmer atmega32u2 get
        # Bootloader Version: 0x00 (0)

        command = [ "dfu-programmer", "atmega32u2", "get" ]
        result = subprocess.run(command, capture_output=True);
        if "Bootloader Version" in ascii(result.stdout):
            break

        time.sleep(0.25)
        attempts -= 1

        if attempts == 0:
            g_errorMessage = "Timeout waiting for bootloader"
            return False


    print("Erasing old firmware...")
    command = [ "dfu-programmer", "atmega32u2", "erase" ]
    result = subprocess.run(command, capture_output=True);
    if result.returncode != 0:
        g_errorMessage = "Error erasing firmware"
        return False

    print("Programming new firmware...")
    command = [ "dfu-programmer", "atmega32u2", "flash", filename ]
    result = subprocess.run(command, capture_output=True);
    if result.returncode != 0:
        g_errorMessage = "Error programming firmware"
        return False

    print("Starting new firmware...")
    command = [ "dfu-programmer", "atmega32u2", "start" ]
    result = subprocess.run(command, capture_output=True);

    print("Firmware update completed.")
    return True


# Glue functions for xmodem
def getc(size, timeout=1):
    global trxbytes, trxbytes2
    ser.timeout = timeout
    dat = ser.read(size)

    # Display a . each 1k
    trxbytes = trxbytes + len(dat)
    if trxbytes - trxbytes2 > 1024:
        print(".", end="", flush=True)
        trxbytes2 = trxbytes

    return dat

def putc(data, timeout=1):
    global trxbytes, trxbytes2
    ser.timeout = timeout
    n = ser.write(data)
    ser.flush()

    # Display a . each 1k
    trxbytes = trxbytes + len(data)
    if trxbytes - trxbytes2 > 1024:
        print(".", end="", flush=True)
        trxbytes2 = trxbytes

    return n


logging.basicConfig(filename='example.log', level=logging.DEBUG)



parser = argparse.ArgumentParser(description='Control tool for smscprogr')
parser.add_argument("-i", '--info', help='Provide information about the programmer and cartridge', action='store_true')
parser.add_argument("-b", '--blankcheck', help='Check if a FLASH cartridge is blank', action='store_true')
parser.add_argument("-r", '--read', help='Read the cartridge contents to a file.', type=argparse.FileType('wb'), dest='outfile', metavar='outfile.sms')
parser.add_argument("-p", '--prog', help='(Re)program the cartridge with contents of file', type=argparse.FileType('rb'), dest='infile', metavar='rom.sms')
parser.add_argument("-d", "--device", help='Use specified serial port device.', action='store', default='/dev/ttyACM0')
parser.add_argument("-l", '--listports', help='List serial ports', action='store_true')
parser.add_argument("-v", '--verbose', help='Enable verbose output', action='store_true')
parser.add_argument('--bootloader', help='Restart programmer in bootloader for FW update', action='store_true')
parser.add_argument('--verify', help='Read back and compare after programming', default=False, action='store_true')
parser.add_argument('--update_firmware', help='Update programmer firmware with hexfile', action='store', metavar='firmware.hex')

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
 
        if args.update_firmware:
            print("Could not open port, but trying to use dfu-programmer to update firmware in case the programmer is already in bootloader mode.")
            exit(updateFirmware(args.update_firmware))

        print("Could not open serial port")
        print("Try -l to list available ports?")
        exit()


# Update firmware

if args.update_firmware:
    exit(updateFirmware(args.update_firmware))


# Enter bootloader?

if args.bootloader:
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

readProgrammerInfo()


# Blank check
if args.blankcheck:
    if "blankcheck" not in programmer_caps:
        print("Error: Programmer firmware does not support blank check")
        exit()
    tmp = exchangeCommand("bc")
    print(tmp)


# Download / Dump cartridge
if args.outfile != None:
    sendAbort()
    tmp = exchangeCommand("")
    tmp = exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)
    download(args.outfile)
    tmp = exchangeCommand("")

    print("Done.")


# Upload / Program file
if args.infile != None:
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


    if args.verify:
        args.infile.seek(0)
        filedata = args.infile.read()
        print("file size: ", len(filedata))

        if "setromsize" in programmer_caps:
            tmp = exchangeCommand("")
            tmp = exchangeCommand("setromsize " + str(len(filedata)) )
        else:
            print("Warning: Programmer firmware does not support 'setromsize'. Verify will be slow.")
            tmp = exchangeCommand("init")

        readbuf = downloadToBuffer()
        print("readback length: ", len(readbuf))

        if filedata == readbuf:
            print("Verify OK")
        elif len(readbuf) > len(filedata):
            # without the setromsize command, download may be larger than file due to failing
            # ROM size auto-detection
            if filedaata == readbuf[0:len(filedata)]:
                print("Verify OK")
            else:
                printf("Verify FAILED")
        else:
            printf("Verify FAILED")
            exit(1)
    print("Done.")


if args.info:
    print("Programmer version:", programmer_version)
    print("Caps: ", programmer_caps)
    tmp = exchangeCommand("")
    tmp = exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)
    tmp = exchangeCommand("")


ser.close()
exit(0)
