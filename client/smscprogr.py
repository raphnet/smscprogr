import serial, sys, logging, argparse
from xmodem import XMODEM

class SMSCProgrException(Exception):
    pass

#logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

rxbytes = 0
rxbytes2 = 0
txbytes = 0
txbytes2 = 0
ser = None

progressCb = None

def sendCommand(command):
    #print("Sending command: " + command)
    ser.write(bytes(command + "\r\n", "ASCII"))
    ser.flush()

def sendAbort():
    ser.write(b'\x18')
    ser.flush()
    ser.write(b'\x03')
    ser.flush()


def exchangeCommand(command, ender="\r\n> ", atEnd=True):
    ser.reset_input_buffer()    # Drop any unread characters
    sendCommand(command)        # Send the command
    # Now wait for the answer
    answer = ""
    ser.timeout = 0.1
    while True:
        b = ser.read(1)
        if len(b) > 0:
            #print(b.decode("ascii"))
            answer = answer + b.decode("ascii", "ignore")
            if atEnd:
                if answer[-len(ender):] == ender:
                    break
            else:
                if ender in answer:
                    break

    return answer


def download(outfile):
    global rxbytes, rxbytes2

    rxbutes=0
    rxbytes2=0

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
    global txbytes, txbytes2

    txbytes = 0
    txbytes2 = 0

    print("Starting upload")

    # Initiate xmodem download, wait for the initial NAK character
    exchangeCommand("ux", "\x15", atEnd=True)

    xm = XMODEM(getc,  putc)
    print("Uploading", end="", flush=True)
    n = xm.send(infile, retry=10, timeout=1, quiet=False )
    print("") # newline
    if n:
        print("Upload completed with success.")
    else:
        print("Upload error")
    return n




# Glue functions for xmodem
def getc(size, timeout=0.1):
    global rxbytes, rxbytes2
    ser.timeout = timeout
    dat = ser.read(size)

    # Display a . each 1k
    rxbytes = rxbytes + len(dat)
    if rxbytes - rxbytes2 > 1024:
        if progressCb:
            progressCb(rxbytes)
#        print(".", end="", flush=True)
        rxbytes2 = rxbytes

    return dat

def putc(data, timeout=0.1):
    global txbytes, txbytes2
    ser.timeout = timeout
    n = ser.write(data)
    ser.flush()

    # Display a . each 1k
    txbytes = txbytes + len(data)
    if txbytes - txbytes2 > 1024:
        if progressCb:
            progressCb(txbytes)
#        print(".", end="", flush=True)
        txbytes2 = txbytes

    return n


def open(device):
    global ser
    try:
        ser = serial.Serial(device, 115200, 8)
    except:
        return False

    if ser == None:
        return False

    return True


def close():
    ser.close()


def setProgressCallback(cb):
    global progressCb

    progressCb = cb


def chipErase():
    exchangeCommand("")
    exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)

    if not "Cartridge type: FLASH" in tmp:
        raise SMSCProgrException("Cartridge not flash based - cannot chip erase")

    if not "(supported)" in tmp:
        raise SMSCProgrException("Cartridge flash type not supported yet")

    print("Erasing...")
    tmp = exchangeCommand("ce")
    exchangeCommand("")
    print(tmp)

    return True


def blankCheck():
    exchangeCommand("")
    exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)

    print("Blank checking...")
    tmp = exchangeCommand("bc")
    exchangeCommand("")
    print(tmp)

    if not "Cartridge is blank: YES" in tmp:
        return False

    return True


def eraseAndProgram(infile):
    exchangeCommand("")
    exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)

    if not "Cartridge type: FLASH" in tmp:
        raise SMSCProgrException("Cartridge not flash based - cannot chip erase")

    if not "(supported)" in tmp:
        raise SMSCProgrException("Cartridge flash type not supported yet")

    if progressCb:
        progressCb(-1)

    print("Erasing...")
    tmp = exchangeCommand("ce")
    print(tmp)

    upload(infile)
    exchangeCommand("")

    return True


def program(infile):
    exchangeCommand("")
    exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)

    if not "Cartridge type: FLASH" in tmp:
        raise SMSCProgrException("Cartridge not flash based - cannot chip erase")

    if not "(supported)" in tmp:
        raise SMSCProgrException("Cartridge flash type not supported yet")

    if progressCb:
        progressCb(-1)

    upload(infile)
    exchangeCommand("")

    return True




def readROM(outfile):
    exchangeCommand("")
    exchangeCommand("")
    tmp = exchangeCommand("init")
    print(tmp)

#    "ROM size set to <size>"

    download(outfile)

    return True

