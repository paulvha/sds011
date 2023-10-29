# SDS011 library for Raspberry Pi
===========================================================

A program to set instructions and get information from an SDS-011 running
on linux.<br>
V1 was written in C and contains a flexible command-line options.<br>
V2 is optimized and written in C++<br>
A detailed description of the many options and findings are in sds011.odt

## Getting Started
As part of a larger project I am looking at analyzing and understanding the air quality.
I have done a number of projects on air-sensors. The SDS-011 sensor has been around for a
longer time and there are many articles and drivers to connect to Arduino and/or from Python.
However I wanted to create a C-language version to connect the DYLOS 1700 and SCD30 at the same
time and compare the results. The challenge was larger than expected.

I have tested this on a Raspberry PI running Raspbian Jessie release and Ubunut 18.04. It has been
adjusted and extended for stable working.

## Prerequisites
No dependency on other libraries

## Software installation
* Copy the files in a directory
* 'make' command will create an executable call sds

## Program usage
* To get help type ./sds -h
* to get output: connect the SDS011 to USB and type sudo ./sds -o

### Program options

SDS-011 Options:

* -m    get current working mode
* -p    get current working period
* -r    get current reporting mode
* -d    get Device ID
* -f    get firmware version
* -q    use query reporting mode       (default : continous)


SDS-011 setting:

* -M [ S / W  ]   Set working mode (sleep or work)
* -P  [ 0 - 30 ]  Set working period (minutes)
* -R [ Q / R  ]   Set reporting mode (query or reporting)
* -D [ 0xaabb ]   Set new device ID

Program setting:

* -l x          loop x times ( 0 = endless)  (default : 10 loops)
* -w x          x seconds between query data (default : 5 seconds)
* -H #          set correction for humidity (e.g. 33.5 for 33.5%)
* -u device     set new device               (default : /dev/ttyUSB0)
* -b            set no color output          (default : color)
* -h            show help info
* -v            set verbose / debug info     (default : NOT set)

## Versioning

### version 2.1 /October 2023
 * fixed issue with failing wakeup after setting to sleep

### version 2.0 / May 2019
 * changed to better sturuct between user level and supporting library
 * change to C++ file structure
 * enhanced debugging

### version 1.1 / February 2019
 * set reporting mode to query included in -q option
 * set reporting mode to stream included in  -o option

### version 1.0 / January 2019
 * Initial version for Raspberry Pi and Ubuntu



## Author
* Paul van Haastrecht (paulvha@hotmail.com)

## License
This project is licensed under the GNU GENERAL PUBLIC LICENSE 3.0

## Acknowledgements
A starting point and still small part of this code is based on the work
by karl, found on:  github https://github.com/karlchen86/SDS011
