Clone this repository and place it in a Arduino project directory called "subway"

Install the ESP32 drivers found here: https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers
 - Linux users need to create an account.

<INSERT GEORGE MESSAGE HERE>

Copy and paste the URL found [here|https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md] then paste to Preferences -> Additional Board Manager URLs

Add the Adafruit OLED libraries. Download found here https://github.com/ladyada/Adafruit_CharacterOLED

NTPClient. Go to Sketch -> Add Library -> Library Manager. Search for NTPClient and install.

Copy all headers + C into project directory.

Static MTA GTFS resources - such as station lookups - are found here: https://transitfeeds.com/p/mta/79

Linux:
 - Install pyserial

Cannot do 5G WiFi.
