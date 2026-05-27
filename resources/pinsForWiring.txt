3.3V goes to positive bus on breadboard
GND goes to negative bus on breadboard

#BMP280
-- On breadboard --
GND is BMP280 black wire
VIN (3.3V) is BMP280 red wire
-- On arduino --
A4 is BMP280 blue wire
A5 is BMP280 yellow wire

#GPS
-- On arduino --
The 2nd GND on the Arduino goes to the yellow wire on GPS
VBUS (5V) goes to red wire on GPS
D7 goes to green wire on GPS
D8 goes to white wire on GPS
-- On breadboard --
GND goes to black wire on GPS

#DHT22
-- ORIENTATION --
DHT22 front is side with grills on it, the 4 pins with the grills lining up in between them are 1-4, look at the front of the DHT22, and the 1st pin there is pin 1
-- On breadboard --
VIN (3.3V) goes to Pin 1 on DHT22
GND goes to Pin 4 on DHT22
(No pin 3 usage)
Place a 10K Ohm resistor between pins 1 & 2, but on the same row, so the wires are still there, but there is a connection between both
-- On arduino --
D2 goes to Pin 2 on the DHT22
