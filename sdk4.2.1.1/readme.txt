Release notes for version 4.2.1.x

This is a combined SDK release for
- Atomix
- Atomix + BoB
- Atomix HDMI
- Atomix HDMI + BoB
- Atomix LT
- Atomix LT 4 BNC
- Centaurus II

It is available for
- Windows 2000
- Windows XP
- Windows Vista
- Windows 7
- Linux Kernels 2.6, 3.0, 3.1, 3.2, 3.3
- Mac OS X

New Features
------------
* ATI/NVIDIA interface (Direct API)
* Support for Linux kernels 3.0 and 3.1
* Support for SDTV ANC Streamer Mode
* Reduced signal delay using Direct API
* SMPTE352 ANC on secondary SDI links (Atomix and Atomix HDMI)

Changes 4.1 -> 4.2
------------------
* Additional dynamic dvs libraries for dynamic linking
* Direct API support
* HDMI 1.4 3D

Known Bugs / Limitations
------------------------
* For SDI output of 4K QUAD rasters and PHDTV rasters (1920x1080/47P...60P) there is only the combination
  of storage color mode YUV422 and I/O mode YUV422 available. This limitation can be avoided on the output
  side by using the Render API for a matrix conversion before the image is displayed.
* GPI functionality is currently only available on Atomix LT 4 BNC.
* On Atomix LT ANC data on each I/O channel's second SDI link will be available later.
* In new sync mode introduced in SDK 3.2.14.0 the output signal is shifted by one output line
  in case of syncing SMPTE296 with a SD signal (Centaurus II / Centaurus II LT).
* Multi-channel: Analog out (DVI/CVBS) is always assigned to the output channel 0 (Centaurus II / Centaurus II LT).
* Closed Caption will be available later.
* Rasters PAL24I, PALHR, NTSCHR, SMPTE240/29I and 30I, SMPTE295/25I, SMPTE296/71P and 72P are no longer supported.

Known Bugs / Limitations in the Example Programs
------------------------------------------------
* The 'dpxrender' example is not usable with Centaurus II and Centaurus II LT.
* The 'stereo_player' example is not usable with Centaurus II and Centaurus II LT.
* Using 'dpxio' example for field-based output is not implemented.
* The 'counter' and 'logo' examples do not support 12- and 16-bit modes.
* The 'logo' example program cannot be used in BOTTOM2TOP storage mode or when multichannel is activated.
* The 'overlay' example requires Centaurus II with a firmware version of
  either exactly 3.2.68.11_12_9 or at least 3.2.70.6_12_9.
  Atomix cards and Centaurus II LT are currently not supported by this example program.
* The 'rs422test' example returns an error message on Centaurus II.
  To circumvent this use it with the 'base' command line option.
* The 'dmaspeed' example requires 8-bit storage modes to measure reasonable
  data rates. Blocksize commands are handled as frame commands.
* Currently there is no example for the hardware watchdog feature included.
  It is available on request.

Upgrade Information:
--------------------
The latest firmware revisions for the DVS video boards can be found on
our web site (http://www.dvs.de). All necessary upgrade
information, such as firmware recommendations, will be detailed there.

This SDK has been tested and released for the following firmware revisions:
  Atomix                         4.3.0.26_4.0.22
  Atomix + BoB                   6.3.0.9_6.0.8
  Atomix HDMI                    7.4.0.11_7.0.10
  Atomix HDMI + BoB              8.3.0.9_8.0.10
  Atomix LT                      5.4.0.7_5.2.4
  Atomix LT 4 BNC                5.4.0.7_5.2.4
  Atomix LT for Streamer Mode   10.4.0.1_5.2.4
  Centaurus II                   3.2.76.7_19_15
