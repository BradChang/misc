This program reads a serial port for individual characters in the range 1-9.
Whenever one is read, it shells out a command specified in a configuration file.

E.g. the config file:

1 wget http://127.0.0.1/data?q=123
2 logger someone pressed 2
3 ping -c 1 127.0.0.1
...

By default it reads /dev/ttyACM0.

--------------------------------------------------------------------------------
This program only consumes serial inputs. The particular set up it was used in:

* Arduino Uno + Spark Fun Touch Shield (https://www.sparkfun.com/products/12013)
* Load the firmware from https://github.com/sparkfun/Touch_Shield on the Arduino
* Plug the Arduino USB into a Linux computer such as an RPi and run this program
