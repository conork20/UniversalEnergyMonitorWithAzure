## Setting up the sensor

### Hardware

#### Compatible Devices
Most [ESP8266 dev boards](https://arduino-esp8266.readthedocs.io/en/latest/boards.html).

This project uses the ESP8266's single analog input, the board you use must make this available.
Boards we tested on did not use a voltage divider on the ADC input, but yours might.
Consider the max input voltage of the board's analog input when choosing a CT clip.

We've tested using `NodeMCU` and `Adafruit Feather HUZZAH` boards.

#### CT Clip

A [current transformer](https://en.wikipedia.org/wiki/Current_transformer) is used to measure current. (AC only)

A current transformer with a built-in resistor was used with this project because it can be sampled directly by the E8266. Ours outputs 1V/30A.

Some current transformers do not have internal circuitry and output mA/A. If your CT outputs mA/A then you will need to add a burden resistor. 
Using this second type of sensor without a burden resistor will fry your board. 

#### Limitations
* This device measures current, it can't measure voltage phase w.r.t current, so all calculations assume a [power factor](https://en.wikipedia.org/wiki/Power_factor) of 1.0.

* This device cannot measure the direction of current, so KWh is calculated assuming that it is always measuring a load.

### Software
This project is based off the Azure IoT Central firmware [sample project](https://github.com/Azure/iot-central-firmware) for the ESP8266. 

#### Build Requirements

* You will need an Arduino development toolset like the [Arduino IDE](https://www.arduino.cc/en/Main/Software) or [arduino-cli](https://github.com/arduino/arduino-cli).

* Install `ArduinoJson` from the Arduino module library.  
Using arduino-cli: ````$ arduino-cli lib install ArduinoJson````

* Get a set of board files for ESP8266 dev boards from [esp8266.com](https://www.esp8266.com/) by adding an additional URL to the Arduino board manager in .cli-config.yml. (Usually in the same directory as the arduino-cli executable.) Arduino IDE users can add the same URL in `Preferences` under `Additional Boards Manager URLs:`.


```.cli-config.yml
board_manager:
  additional_urls:
  - http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

#### Code Setup
Get connection information from the device explorer page in your IoT Central application. 

1. Add device. ![alt text][adddevice]
2. Create. ![alt text][create]
3. Connect. ![alt text][connect]
4. Copy `Scope ID`, `Device ID`, and `Primary Key` from the device connection dialog box. ![alt text][dialogbox]

Fill in the necessary parts of `UniversalEnergyMonitor.ino`.


```UniversalEnergyMonitor.ino
#define WIFI_SSID <WIFI SSID> 
#define WIFI_PASSWORD <WIFI PASSWORD>

const char* scopeId = <IOT CENTRAL SCOPE ID>;
const char* deviceId = <IOT CENTRAL DEVICE ID>;
const char* deviceKey = <IOT CENTRAL DEVICE KEY>;
```

#### Compiling

Setup the environment: (under the project folder)
```
arduino-cli-0.3.3 core update-index
arduino-cli-0.3.3 core install esp8266:esp8266
arduino-cli-0.3.3 board attach esp8266:esp8266:nodemcu
```
(*Note:* In the last line, `nodemcu` is your specific ESP8266 board type.)


Compile!
```
arduino-cli-0.3.3 compile
```

Upload
```
arduino-cli-0.3.3 upload -p <PORT / DEV?? i.e. => /dev/cu.SLAB_USBtoUART >
```

#### Monitoring?

```
npm install -g nodemcu-tool
```

Assuming the port/dev for the board is `/dev/cu.SLAB_USBtoUART`
```
nodemcu-tool -p /dev/cu.SLAB_USBtoUART -b 9600 terminal
```

[adddevice]: ./ESP8266/images/Add\ Device.png "Add a real device."
[create]: ./ESP8266/images/Create.png "Click create."
[connect]: ./ESP8266/images/Connect.png "Click connect."
[dialogbox]: ./ESP8266/images/Credentials.png "Connection dialog box."
