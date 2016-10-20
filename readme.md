# ESP8266_Fatal
## Save fatal exception details to non-volatile memory

My personal library based and inspired by krzychb's [EspSaveCrash](https://github.com/krzychb/EspSaveCrash)

The main difference is that my implementation calls `EEPROM.begin()` when the sketch is started, instead of when an exception occurs, because once an exception occurs the system might be low on memory causing `EEPROM.begin()` to fail.

Refer to krzychb's [EspSaveCrash](https://github.com/krzychb/EspSaveCrash) for details.

More projects at [coertvonk.com](http://www.coertvonk.com)