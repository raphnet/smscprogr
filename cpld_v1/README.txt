This is to compile the CPLD source code.

You will need WinCUPL, which is at this date (2021-02) available for free from
microchip here:

	https://www.microchip.com/en-us/products/fpgas-and-plds/spld-cplds/pld-design-resources

I could not get the GUI to work reliably on my system (it kept crashing, especially in simulation)
so instead I use batch files. But it's probably better like this anyway.

Run setup.bat once (optionally edit it first if the paths are different on your system), then run
compile.bat to compile and generate a new .JED file and use the ATMISP tool to program the CPLD.
