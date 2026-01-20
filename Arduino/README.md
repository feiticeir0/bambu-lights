## Code

The code hasn't much in need of change, just some configurations.

First, you need to add your Wi-Fi details
```c
// WiFi
const char* WIFI_SSID     = "";    // Your wifi network
const char* WIFI_PASSWORD = "";    // your wifi network password
```

The next information needed is from your printer. 
* **PRINTER_IP** is the address of the printer on your network
* **ACCESS_CODE** you get it from the printer
* **PRINTER_SERIAL** is your printer serial number that you can also get from the menus of the printer. 

```c
// Bambu
const char* PRINTER_IP      = "xxx.xxx.xxx.xxx";  // Printer IP
const int   PRINTER_PORT    = 8883;    // No need to change - static
const char* PRINTER_USER    = "bblp";  // no change - static
const char* ACCESS_CODE     = "";      // LAN Access Code (From printer)
const char* PRINTER_SERIAL  = "";    // Serial number (From printer)
```

These are the libraries that you're going to need:
```c
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
```

