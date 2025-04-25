#!/usr/bin/env python3

import io, sys, os, subprocess, time

import FreeSimpleGUI as sg
import smscprogr
import hashlib
import serial, serial.tools.list_ports

g_reroute_stdout = True

g_errorMessage = None
g_okMessage = None

g_bufferdata = b""
g_read_buffer = b""

g_portnames = [ ]
g_portdevices = [ ]
g_portlist = [ ]
g_programmer_info = [ ]

g_last_opened_port = ""


g_rom_filetypes = [ ("SMS ROM", "*.SMS *.sms"), ("GAME GEAR ROM", "*.GG *.gg"), ("SG-1000 ROM", "*.SG *.sg"), ("All files", "*.* *") ]

layout_operations = [
    [ sg.Radio(text="Read", default=True, group_id="-GRP-OPS-", key="-OP-READ-", tooltip="Read cartridge to buffer.") ],
    [ sg.Radio(text="Verify", group_id="-GRP-OPS-", key="-OP-VERIFY-", tooltip="Read cartridge and compare to buffer. Buffer left as is.") ],
    [ sg.Radio(text="Program", group_id="-GRP-OPS-", key="-OP-PROG-", tooltip="Erase and program cartridge with data from buffer") ],
    [ sg.Radio(text="Program only", group_id="-GRP-OPS-", key="-OP-PROGONLY-", tooltip="Orogram cartridge with data from buffer without erasing it first. (Use only for known blank cartridge)") ],
    [ sg.Radio(text="Chip erase", group_id="-GRP-OPS-", key="-OP-CHIP-ERASE-", tooltip="Perform a chip erase operation") ],
    [ sg.Radio(text="Blank check", group_id="-GRP-OPS-", key="-OP-BLANK-CHECK-", tooltip="Verify if the cartridge is blank") ],
    [ sg.Button("Run", key='-RUN-') ],
]

layout_buffer = [
    [ sg.Text("Contents: "), sg.Text("", key="-BUFFER-NAME-") ],
    [ sg.Text("Size: "), sg.Text("", key="-BUFFER-SIZE-") ],
    [ sg.Text("MD5: "), sg.Input("", key="-MD5-", disabled=True) ],
    [ sg.Text("TMR header: "), sg.Text("-", key="-TMR-") ],
    [ sg.Text("SDSC header: "), sg.Text("-", key="-SDSC-") ],

    [ sg.Multiline(autoscroll = False, expand_y = True, expand_x = True, size=(80,25),  font=('monospace', 8), key="-BUFFER-", write_only = True, disabled = True ) ],
    [
        sg.FileBrowse("Load file...", enable_events=True, key="-LOAD-", target="-LOAD-", file_types=g_rom_filetypes),
        sg.FileSaveAs("Save file...", enable_events=True, key="-SAVE-", target="-SAVE-"),
    ],
]

layout_programmer = [
    [ sg.Text("Port: "),  sg.Button("(rescan)", key="-RESCAN-") ],
    [ sg.Listbox(key="-SELECTED-PORT-", values=g_portnames, enable_events=True, expand_x = True, expand_y = True, size=(10,5)) ],
    [ sg.Text("Version: "), sg.Text("", key="-TXT-VERSION-") ],
    [ sg.FileBrowse("Update...", enable_events=True, key="-UPDATEFW-", file_types=[("HEX Files","*.hex")]) ],
]


column_leftSide = [
    [   sg.Frame("Operations", layout_operations, expand_x = True) ],
    [   sg.Frame("Programmer", layout_programmer) ],
]

layout_rightSide = [
    [   sg.Frame("Buffer", layout_buffer, expand_y = True, expand_x = True) ],
]

layout = [
    [sg.Text("SMSCPROGR : SMS/Mark-III cartridge reader/programmer", font=("Helvetica", 20)) ],
    [sg.Column(column_leftSide, expand_y=True, expand_x=False),
     sg.VSeparator(),
     sg.Column(layout_rightSide, expand_y=True, expand_x=True),
    ],
    [ sg.HSeparator() ],
    [ sg.Multiline(autoscroll = True, expand_x = True, expand_y = True, size=(80,5),  font=('monospace', 8), key="-LOGS-", write_only = True, disabled = True, reroute_stdout = g_reroute_stdout) ],
    [ sg.Quit() ],
]



def getHexDump(data):
    text = ""

    decoded = ""
    address = 0
    for byte in data:

        if 0 == address % 16:
            if address != 0:
                text += decoded
                decoded = ""
                text += "\n"
            text += format(address, '06x') + ": "

        if 0 == address % 8:
            text += " "

        text += format(byte, '02x') + " "
        a = chr(byte)
        if a.isprintable():
            decoded += a
        else:
            decoded += "."


        address += 1

    if 0 == address % 16:
        if address != 0:
            text += decoded
            decoded = ""
            text += "\n"


    return text




def syncBufferInfo(origin):
    global g_bufferdata

    window['-BUFFER-SIZE-'].update(len(g_bufferdata))
    window['-BUFFER-'].update(getHexDump(g_bufferdata))
    window['-MD5-'].update(hashlib.md5(g_bufferdata).hexdigest())
    window['-BUFFER-NAME-'].update(origin)

    tmr_magic = b'TMR SEGA'
    sdsc_magic = b'SDSC'

    tmr_text = "Absent"
    sdsc_text = "Absent"

    if len(g_bufferdata) >= 0x8000:
        if g_bufferdata[0x7FF0:0x7FF8] == tmr_magic:
            tmr_text = "Present"

        if g_bufferdata[0x7FE0:0x7FE4] == sdsc_magic:
            sdsc_text = "Present"


    window['-TMR-'].update(tmr_text)
    window['-SDSC-'].update(sdsc_text)

    # TODO : Extract and display SDSC information



def openProgrammer(values):
    global g_errorMessage, g_last_opened_port

    selection = values['-SELECTED-PORT-'];
    if not selection:
        g_errorMessage = "No port selected"
        return False
    else:
        devpath = g_portdevices[g_portnames.index(selection[0])]
        print("Device path:", devpath)

        if not smscprogr.open(devpath):
            print("Could not open port")
            return False

    smscprogr.sendAbort()

    smscprogr.exchangeCommand("")
    smscprogr.exchangeCommand("")

    g_last_opened_port = selection[0]
    sg.user_settings_set_entry("-lastport-", g_last_opened_port)

    print("Programmer opened")

    return True



def closeProgrammer():
    smscprogr.close()
    print("Programmer closed")


def getProgrammerInfo(values):
    if not openProgrammer(values):
        return False

    programmerInfo = None

    try:
        smscprogr.exchangeCommand("")
        smscprogr.exchangeCommand("")
        tmp = smscprogr.exchangeCommand("?")
        #print(tmp)

        programmerInfo = { "version": "1.0" }

        programmerInfo["caps"] = [ ];

        if "version" in tmp:
            tmp = smscprogr.exchangeCommand("version")

            # Sample output:
            #
            #   ...
            #       "Version: 1.1\r\n"
            #   ...

            lines = tmp.split("\r\n")
            tmpv = [v.split(": ")[1] for v in lines if "Version:" in v][0]
            #print(tmpv)

            programmerInfo["version"] = tmpv
            programmerInfo["caps"].append("blankcheck")

    except BaseException as e:
        g_errorMessage = e
    finally:
        closeProgrammer();


    closeProgrammer();

    return programmerInfo


def readTobuffer(values):
    global g_read_buffer
    global g_errorMessage
    retval = True

    if not openProgrammer(values):
        return False

    f = io.BytesIO()
    try:
        smscprogr.readROM(f)
        g_read_buffer = f.getvalue();
    except BaseException as e:
        g_errorMessage = e
        retval = False
    finally:
        f.close()
        closeProgrammer();

    return retval



def programFromBuffer(values):
    global g_bufferdata, g_errorMessage
    retval = True

    if not g_bufferdata or len(g_bufferdata) < 1:
        g_errorMessage = "Buffer is empty - nothing to program"
        return False

    if not openProgrammer(values):
        return False

    f = io.BytesIO(g_bufferdata)

    try:
        smscprogr.eraseAndProgram(f)
    except BaseException as e:
        g_errorMessage = e
        retval = False
    finally:
        f.close()
        closeProgrammer();

    return retval



def programOnlyFromBuffer(values):
    global g_bufferdata, g_errorMessage
    retval = True

    if not g_bufferdata or len(g_bufferdata) < 1:
        g_errorMessage = "Buffer is empty - nothing to program"
        return False

    if not openProgrammer(values):
        return False

    f = io.BytesIO(g_bufferdata)

    try:
        smscprogr.program(f)
    except BaseException as e:
        g_errorMessage = e
        retval = False
    finally:
        f.close()
        closeProgrammer();

    return retval


def performBlankCheck(values):
    global g_bufferdata, g_errorMessage
    retval = True

    if not openProgrammer(values):
        g_errorMessage = "Error opening programmer";
        return False

    try:
        retval = smscprogr.blankCheck()
        if not retval:
            g_errorMessage = "Cartridge not blank";
            return False;
    except BaseException as e:
        g_errorMessage = e
        retval = False
    finally:
        closeProgrammer();

    return retval





def performChipErase(values):
    global g_bufferdata, g_errorMessage
    retval = True

    if not openProgrammer(values):
        g_errorMessage = "Error opening programmer";
        return False

    try:
        smscprogr.chipErase()
    except BaseException as e:
        g_errorMessage = e
        retval = False
    finally:
        closeProgrammer();

    return retval



def updateFirmware(values):
    global g_bufferdata, g_errorMessage
    retval = True

#    dfuTool=os.path.realpath("dfu-programmer")
#    print("Dfu programmer path:", dfuTool)

    # Check if the update file exists, and keep it in
    # filename.
    filename = values['-UPDATEFW-']
    print("Update file:", filename)

    if not os.path.isfile(filename):
        # This probably cannot happen through the GUI, but still...
        g_errorMessage = "No file selected";
        return False
    if not os.access(filename, os.R_OK):
        g_errorMessage = "Selected file is not readable";
        return False

    # Quick test for dfu-programmer presence
    print("Checking for dfu-programmer...")
    command = [ "dfu-programmer", "--help" ];
    result = subprocess.run(command, capture_output=True);
    #print(result)
    # stderr should contain help from --help, look for it
    if not "global-options" in ascii(result.stderr):
        g_errorMessage = "dfu-programmer not found";
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
        if not openProgrammer(values):
            g_errorMessage = "Error opening programmer";
            return False

        # This causes the firmware to enter boot loader mode. The virtual comm
        # port will cease to exist. Close the programmer.
        smscprogr.sendCommand("boot")
        closeProgrammer();


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

    # When the operatin completes, the gui will perform a rescan port.
    # Add a delay here to increase the odds that the programmer will
    # be ready by then so it gets listed. If this fails, the user will
    # simply have to click rescan and reselect the port. So it is not too critical.
    time.sleep(1)

    print("Done.")

    return retval




def rescanPorts():
    global g_portlist, g_portnames, g_portdevices, g_last_opened_port

    g_portlist = serial.tools.list_ports.comports()
    g_portnames = [p.name for p in g_portlist]
    g_portdevices = [p.device for p in g_portlist]

    window['-SELECTED-PORT-'].update(g_portnames)


def autoSelectLastOpenedPort():
    global g_portnames, g_last_opened_port

    if g_last_opened_port in g_portnames:
        window['-SELECTED-PORT-'].set_value(g_last_opened_port)
    else:
        print("Last opened port", g_last_opened_port, "not found.")



progressWindow = None

def updateProgress(value):
    progressWindow.write_event_value('-PROGRESSUPDATE-', value)


def createProgressDialog(caption = "Progress", disable_close = False):
    global progressWindow

    if progressWindow:
        progressWindow.close()

    layout_progressDialog = [
        [ sg.Text("Operation in progress...") ],
        [ sg.Text(caption), sg.Text(key="-PROGRESS VALUE-") ],
        [ sg.Cancel(disabled = disable_close) ],
    ]

    smscprogr.setProgressCallback(updateProgress)

    progressWindow = sg.Window("In progress...", layout_progressDialog, modal=True, finalize=True, disable_close=disable_close)



def loadSettings():
    global g_last_opened_port

    g_last_opened_port = sg.user_settings_get_entry('-lastport-', '')
    autoSelectLastOpenedPort()



def syncProgrammerInfos(values):
    global g_programmer_info

    if values['-SELECTED-PORT-']:
        v = getProgrammerInfo(values)
        if v:
            print("Programmer version: ", v['version'])
            window['-TXT-VERSION-'].update(v['version'])
            g_programmer_info = v
        else:
            sg.popup_error("Could not determine adapter version.")


window = sg.Window("SMSP", layout, finalize=True, resizable=True)

print("Python version:", sys.version)


rescanPorts()
loadSettings()

window.write_event_value('-GUISTART-', True)

while True:

    event, values = window.read()
    #print(event, values)

    if event == sg.WIN_CLOSED or event == 'Quit':
        break

    if event == '-GUISTART-':
        syncProgrammerInfos(values)

    if event == '-LOAD-':
        filename = values['-LOAD-']
        if filename:
            f = open(filename, "rb")
            g_bufferdata = f.read()
            f.close()
            syncBufferInfo(filename)


    if event == '-SAVE-':
        filename = values['-SAVE-']
        if not filename:
            print("Save cancelled")
        else:

            f = open(filename, "wb");
            f.write(g_bufferdata)
            f.close()
            print("Saved buffer to ", filename)


    if event == '-RESCAN-':
        rescanPorts()

    # Event emitted when a port is selected.
    if event == '-SELECTED-PORT-':
        syncProgrammerInfos(values)


    if event == "-UPDATEFW-":
        createProgressDialog("Firmware update...")
        progressWindow.perform_long_operation(lambda: updateFirmware(values), "-OP-ENDED-")

        while True:
            event2, values2 = progressWindow.read()
            if event2 == 'Cancel' or event2 == sg.WIN_CLOSED:
                break
            if event2 == '-OP-ENDED-':
                rescanPorts()
                #autoSelectLastOpenedPort()
                break
            if event2 == '-PROGRESSUPDATE-':
                progressWindow["-PROGRESS VALUE-"].update(values2['-PROGRESSUPDATE-'])
                continue
            if g_errorMessage:
                break

        progressWindow.close()
        progressWindow = None



    ########################## RUN BUTTON #########################
    if event == '-RUN-':

        if values['-OP-READ-']:
            createProgressDialog("Received bytes:")
            progressWindow.perform_long_operation(lambda: readTobuffer(values), "-OP-ENDED-")

            while True:
                event2, values2 = progressWindow.read()
                if event2 == 'Cancel' or event2 == sg.WIN_CLOSED:
                    smscprogr.sendAbort()
                    break
                if event2 == '-OP-ENDED-':
                    g_bufferdata = g_read_buffer
                    syncBufferInfo("From cartridge")
                    break
                if event2 == '-PROGRESSUPDATE-':
                    progressWindow["-PROGRESS VALUE-"].update(values2['-PROGRESSUPDATE-'])
                    continue
                if g_errorMessage:
                    break

            progressWindow.close()
            progressWindow = None


        if values['-OP-BLANK-CHECK-']:

            if "blankcheck" in g_programmer_info["caps"]:
                createProgressDialog("Checking if flash is blank...", disable_close=True)
                progressWindow.perform_long_operation(lambda: performBlankCheck(values), "-OP-ENDED-")

                while True:
                    event2, values2 = progressWindow.read()
                    if event2 == 'Cancel' or event2 == sg.WIN_CLOSED:
                        break
                    if event2 == '-OP-ENDED-':
                        #syncBufferInfo("From cartridge")
                        if not g_errorMessage:
                            g_okMessage = "Cartridge is blank"
                        break

                progressWindow.close()
                progressWindow = None


            else:
                createProgressDialog("Checked bytes:")
                progressWindow.perform_long_operation(lambda: readTobuffer(values), "-OP-ENDED-")

                while True:
                    event2, values2 = progressWindow.read()
                    if event2 == 'Cancel' or event2 == sg.WIN_CLOSED:
                        smscprogr.sendAbort()
                        break
                    if event2 == '-OP-ENDED-':
                        if len(g_read_buffer) < 0:
                            g_errorMessage = "Received zero bytes?!"
                            break

                        ffcount = 0
                        for val in g_read_buffer:
                            if val != 0xFF:
                                ffcount += 1

                        if ffcount > 0:
                            g_errorMessage = "Not blank. " + str(round(ffcount / len(g_read_buffer))) + "% free"
                        else:
                            g_okMessage = "Verify ok"

                        break
                    if event2 == '-PROGRESSUPDATE-':
                        progressWindow["-PROGRESS VALUE-"].update(values2['-PROGRESSUPDATE-'])
                        continue
                    if g_errorMessage:
                        break

                progressWindow.close()
                progressWindow = None




        if values['-OP-VERIFY-']:
            if not g_bufferdata or len(g_bufferdata) == 0:
                sg.popup_error("Buffer is empty - Nothing to verify")
            else:
                createProgressDialog("Received bytes:")
                progressWindow.perform_long_operation(lambda: readTobuffer(values), "-OP-ENDED-")

                while True:
                    event2, values2 = progressWindow.read()
                    if event2 == 'Cancel' or event2 == sg.WIN_CLOSED:
                        smscprogr.sendAbort()
                        break
                    if event2 == '-OP-ENDED-':
                        if g_bufferdata == g_read_buffer:
                            g_okMessage = "Verify ok"
                        elif len(g_read_buffer) > len(g_bufferdata):
                            # In some cases, the programmer determines the wrong (larger) size for the ROM. Verify
                            # only up to len(g_bufferdata) since the data in the buffer is what we are checking against.
                            if g_bufferdata == g_read_buffer[0:len(g_bufferdata)]:
                                g_okMessage = "Verify ok"
                            else:
                                g_errorMessage = "Verify failed"
                        else:
                            g_errorMessage = "Verify failed"

                        break
                    if event2 == '-PROGRESSUPDATE-':
                        progressWindow["-PROGRESS VALUE-"].update(values2['-PROGRESSUPDATE-'])
                        continue
                    if g_errorMessage:
                        break

                progressWindow.close()
                progressWindow = None



        if values['-OP-CHIP-ERASE-']:
            createProgressDialog("Erasing - This can take over 30 seconds!", disable_close=True)
            progressWindow.perform_long_operation(lambda: performChipErase(values), "-OP-ENDED-")

            while True:
                event2, values2 = progressWindow.read()
                if event2 == 'Cancel' or event2 == sg.WIN_CLOSED:
                    break
                if event2 == '-OP-ENDED-':
                    #syncBufferInfo("From cartridge")
                    break

            progressWindow.close()
            progressWindow = None


        if values['-OP-PROG-']:
            createProgressDialog("Transmitted bytes:")
            progressWindow.perform_long_operation(lambda: programFromBuffer(values), "-OP-ENDED-")

            while True:
                event2, values2 = progressWindow.read()
                if event2 == 'Cancel' or event2 == sg.WIN_CLOSED:
                    smscprogr.sendAbort()
                    break
                if event2 == '-OP-ENDED-':
                    break
                if event2 == '-PROGRESSUPDATE-':
                    bytesSoFar = values2['-PROGRESSUPDATE-']
                    percent = (bytesSoFar / len(g_bufferdata)) * 100;
                    percent = round(percent)
                    # FIXME: the byte count includes the xmodem overhead so it goes over 100%...
                    if percent > 100:
                        percent = 100

                    progressString = str(bytesSoFar) + " / " + str(len(g_bufferdata)) + " (" + str(percent) + "%)"
                    progressWindow["-PROGRESS VALUE-"].update(progressString)
                    continue
                if g_errorMessage:
                    break

            progressWindow.close()
            progressWindow = None


        if values['-OP-PROGONLY-']:
            createProgressDialog("Transmitted bytes:")
            progressWindow.perform_long_operation(lambda: programOnlyFromBuffer(values), "-OP-ENDED-")

            while True:
                event2, values2 = progressWindow.read()
                if event2 == 'Cancel' or event2 == sg.WIN_CLOSED:
                    smscprogr.sendAbort()
                    break
                if event2 == '-OP-ENDED-':
                    break
                if event2 == '-PROGRESSUPDATE-':
                    bytesSoFar = values2['-PROGRESSUPDATE-']
                    percent = (bytesSoFar / len(g_bufferdata)) * 100;
                    percent = round(percent)
                    # FIXME: the byte count includes the xmodem overhead so it goes over 100%...
                    if percent > 100:
                        percent = 100

                    progressString = str(bytesSoFar) + " / " + str(len(g_bufferdata)) + " (" + str(percent) + "%)"
                    progressWindow["-PROGRESS VALUE-"].update(progressString)
                    continue
                if g_errorMessage:
                    break

            progressWindow.close()
            progressWindow = None




    if g_errorMessage:
        sg.popup_error(g_errorMessage)
        g_errorMessage = None

    if g_okMessage:
        sg.popup_ok(g_okMessage)
        g_okMessage = None



window.close()
