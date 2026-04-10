# DIY-Hi-Fi-Pocket-Music-Player-Based-on-a-RP2040-Microcontroller

## Hardware components and specifications
The central processing unit is a Raspberry Pi Pico (RP2040) GroundStudio Marble Pico. This variant was chosen because it has an integrated MicroSD slot, which helps manage the internal space much better. 

For the audio output and display, the player uses a PCM5102 DAC and an OLED SSD1306 display. The Digital-to-Analog Converter ensures Hi-Fi sound, capable of decoding up to 384kHz/32-bit audio. The 0.96-inch monochrome OLED has a 128x32 resolution and is used for showing device states and track information.

Power and control are handled by a 3.7V 500mAh Li-Po battery (Model 802035), which is compact and sufficient for a portable music player. It is charged via a TP4056 module featuring USB-C, overcurrent protection, discharge protection, and 5W charging capabilities. The user interface consists of 3 simple push-down buttons.

## Structural Design and Ergonomics
The custom case was built using an old DVD case for the front and back plates. The structure was consolidated by creating pillars made of hot glue to prevent the plastic from flexing under pressure. After measuring and planning the internal layout, holes were cut in the front plate for the display and buttons. B-7000 adhesive secured the battery, velcro tape was used for initial positioning, and hot glue provided the final structural fixing, especially around components with ports. Finally, electrical tape was used to cover the edges, protect the internal components, and give it a uniform look.

The resulting case is 10 cm wide, 5 cm tall, and 2 cm thick, making it very portable. On the front, it features the OLED display and the 3 control buttons. The top side houses the ON/OFF switch, the RP Pico USB-C port, and a power LED indicator. The bottom side contains the 3.5mm audio jack and the MicroSD card slot, while the left side has the TP4056 USB-C charging port.

## Software Logic and Architecture
The software is running on a compatible Raspberry Pi Pico Board, written in C++ using the Arduino IDE environment. It relies on the ESP8266Audio library for audio processing and Adafruit_GFX for the display interface.

External components are connected directly to the Pico Board. The OLED display uses I2C pins 4 (SDA) and 5 (SCL). The DAC connects via I2S to pins 10 (BCK), 11 (LCK), 12 (DIN), and GND (SCK). The control buttons utilize digital pins 20, 26, and 21. Storage is managed entirely through the GroundStudio Marble Pico's integrated MicroSD slot.

To provide a seamless user experience, the program uses a filtering function during the initialization phase that selectively loads only files with .mp3, .wav, or .flac extensions. Any other files are automatically ignored. Data read from the SD card passes through an 8KB RAM buffer (AudioFileSourceBuffer). This acts as a safety cushion that constantly feeds the audio decoder, eliminating the risk of audio stuttering or lag caused by SD card read speeds (a minimum of Class 10, U1 MicroSD card is required). The system automatically selects the appropriate decoder based on the detected file extension.

The GUI indicates the player's status using geometric symbols drawn pixel-by-pixel: a solid triangle for Play, two vertical rectangles for Pause, and a solid square for Ready/Finished. For filenames that exceed the 128-pixel width of the OLED screen, a custom horizontal scrolling algorithm shifts the text to the left by 2 pixels every 150 milliseconds. Track timing is handled relative to the microcontroller's internal clock (millis()), deliberately avoiding the slow process of parsing complex metadata from audio files. The system calculates the elapsed seconds from the moment a track starts.aa
