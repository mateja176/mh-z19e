#include <Arduino.h>
#include <MHZ19.h>
#include <SoftwareSerial.h>

const int txPin = D1;
const int rxPin = D2;

SoftwareSerial softwareSerial(rxPin, txPin);

MHZ19 mhz19;

unsigned long elapsedMs = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("\nInitializing MH-Z19 sensor...");
  // Start the software serial port for communication with the MH-Z19
  softwareSerial.begin(9600);
  // Initialize the MHZ19 library, passing the serial port object
  mhz19.begin(softwareSerial); // MH-Z19 default baud rate
  // Reference https://github.com/WifWaf/MH-Z19
  mhz19.autoCalibration(); // Turn auto calibration ON (OFF autoCalibration(false))
  Serial.println("MH-Z19 sensor initialized.");
}

void loop()
{
  if (millis() - elapsedMs >= 2000)
  {
    elapsedMs = millis();
    const int co2Ppm = mhz19.getCO2();
    Serial.print("CO2 (ppm): ");
    Serial.println(co2Ppm);
    const float tempC = mhz19.getTemperature();
    Serial.print("Temperature (C): ");
    Serial.println(tempC);
  }
}
