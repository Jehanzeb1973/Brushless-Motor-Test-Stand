# Brushless-Motor-Test-Stand
Test Stand to Measure Performance of a brushless motor
MotorThrustRig.ino file is the main file with Arduino code
The project uses two MLX90614 temperature sensors on a common I2C bus. This requires one Sensor to have a different I2C Bus address.
I2C bus address for one sensor can be changed using SetI2CAddress Arduino code. Make sure that only one MLX90614 sensor is connected to I2C bus while executing this code
