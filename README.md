Hardware used:
- STM32 WB55RG Nucleo Board
- Sharp LS012B7DH02 Memory Display

Goal of this project:
* Bluetooth enabled smartwatch
* Apps, watchfaces and firmware updates over Bluetooth
* Easy user implementation of additions
* Extreme power effciency (<3 µA average for normal operation, at least 365 days runtime on a CR3032 battery)
* Watch casing thinner than 8-9 mm

Active todos:
* Improve GFX functions to use memset()
* Add DMA to SPI transfer
* Only update the necessary lines on the display
* Add a LPTIM for 1 Hz VCOM signal
* Implement Stop 2 power saving mode
* Imporove char/string buffer write efficiency
* Add more GFX functions
* Add bluetooth functionality
* Add a functional UI
