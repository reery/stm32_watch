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
* Improve GFX functions to use memset() (DONE)
* Add more GFX functions (rounded corners, line thickness)
* Add DMA to SPI transfer
* Only update the necessary lines on the display (DONE)
* Add an RTC for 1 Hz VCOM signal (DONE)
* Implement Stop 2 power saving mode (DONE)
* Imporove char/string buffer write efficiency (DONE)
* Add bluetooth functionality
* Add font handling
* Add a highly efficient font for watchface
* Add a functional UI

Standardized performance test:
* draw a shape 100x100 px in size
* without optimization = 28 ms
** removing boundary checks = 25 ms
** bitwise operators = 23 ms
** bitbanding feature = 19 ms
** memset() = 1 ms