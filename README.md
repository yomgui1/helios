# Helios

This is Helios, the first Firewire stack for the PPC/MorphOS system.

If you want to develop some applications using the helios.library,
I've put in the package everything you need to to that in the SDK directory.


## ATTENTION

**Helios is in Beta stage.**

SBP2 permits TD\_FORMAT and TD\_WRITE and all SCSI commands!
The author (Guillaume Roguez) declines any responsibilities in usage of Helios.

**Your data may be definitively lost...**

Current SDK is in testing stage, API may changes at any moments.

***

## Build from sources

### Pre-requierements

- You must have MorphOS SDK installed on your machine.

### Build

In a shell, go into helios root directory and type ```make```.

## Binary form distribution

### Helios package description

- README.md                       : this file!
- Libs/helios.library             : High level API and core of Helios.
- Devs/Helios/ohci1394_pci.device : MorphOS device to handle OHCI-1394 complient hardwares.
- Classes/Helios/*                : Helios classes.
- SDK/                            : All files needed to develop applications using Helios.
 - include/                      : C header files (.h).
 - lib/libhelios.a               : gluelib if you don't use inlines.
 - examples/                     : some API usage examples written in C
- C                               : helios start/stop tools.

### Installation and usage

1. Copy contents of libs/* in LIBS:, devs/* in DEVS: and Classes/* in SYS:Classes

2. Then run in a shell, as first program (important!), helios\_rom\_start (in C/ directory).
 - this program shall be run only one time, running twice causes troubles as it install
many tasks and load classes each time
 - Helios and others parties send some debug output through the serial line.
If your computer has been run with RamDebug option you can see also them using
the program LogTool

3. Now each time a device is connected you can see bus reset and enumeration steps
by looking serial line debug ouput.
If a compatible SBP2 device is connected and recognized, SBP2 class auto-mount all partitions

4. Normally Helios should not be removed of the system after run.
But if you're know what you do, un-mount all SBP2 partitions and run program helios_remove.
Finish the removal by running shell command "Flushlib sbp2* helios* ohci*".
You may launch it many time because the internal Helios GarbageCollector task is trigged
only each 3 seconds


## Known issues and limitations

- Only isochrone RX contexts are supported
- Asynchrone modes limitations: stream request not supported
- No SerialBusManager task yet
- No GUI, no prefs, no localisation...

## Changes

### v1.0

**Only Sources**

**No distributed in binary form yet**

This is a major rewrite of Helios

### v0.5

#### new versions
 - helios.library (API major changes): V52.0
 - ohci1394_pci.device: V50.2
 - sbp2.device: V50.1
    
#### details
- __SBP2__
 - mount.library support (auto mount/dismount).
   - fixed various defaults configuration values for CD-ROM and WriteOnly media (WORM)
 - better system compatibility
 - Max transfer limited to 1MB
 - many others optimizations

- __HELIOS__
 - Isochronous RX supported by the ohci1394_pci.device
 - Better device ROM parsing and handling
 - fixed various API bugs and crashes

- __OHCI1394_PCI__
 - Fixed OHCI init (bug was discovered on Powermac)

- __OTHER__
 - Moved Classes into 'Helios' subdirectory
 - Fixed Helios rom startup to be the first app called
 - OpenLibrary() fails if helios_rom_start has not been called in first

### v0.4

- Helios re-design with HW devices and classes: SBP2 is the first class implemented

### v0.3 and older

- outdated
