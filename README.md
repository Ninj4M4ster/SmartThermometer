# Home Monitoring Station

The goal of this project is to develop a prototype home monitoring station. Its main functions are:

- monitoring ambient temperature and atmospheric pressure,
- integrating with Apple Home via Matter.

The long-term goal is to design a custom circuit board for the project.

## Development

The first prototype uses:

- an NUCLEO-L476RG board,
- a 1.3-inch, 128 × 64-pixel I²C OLED display,
- a BMP280 barometer with a built-in temperature sensor,
- a breadboard,
- jumper wires.

![Image of first prototype](images/first_prototype.jpeg)

The second step of development was usage of esp32-c6 to connect the firmware with Apple Home over Matter.

The ESP32-C6 firmware reads an HTU31 sensor once per second, publishes its
temperature and relative humidity to Apple Home over Matter, and displays the
same two readings on a SH1106 128 x 64 I2C OLED. Both devices share the I2C bus
at 100 kHz:

- SDA: GPIO 6
- SCL: GPIO 7
- HTU31 address: `0x40`
- SH1106 address: `0x3C`

Change `I2C_SDA_GPIO` and `I2C_SCL_GPIO` in `main/app/app.cpp` to match your
wiring. Use suitable external I2C pull-up resistors if they are not already
provided by the sensor/display boards.