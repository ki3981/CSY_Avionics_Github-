#include <BMP180.h>
#include <BMP180DEFS.h>
#include <MetricSystem.h>

BMP180 bmp180;

void setup()
{
    Serial.begin(9600);
    if (!bmp180.begin()) //indicates if BMP180 is not connected correctly
    {
        Serial.println("BMP180 not found!");
        while (1);
    }
}
void loop()
{
    for(int i=1; i<=5; i=i+1) //repeat 5 times
    {
    Serial.print("Temperature: ");
    Serial.print(bmp180.readTemperature());
    Serial.println(" °C");
    Serial.print("Pressure: ");
    Serial.print(bmp180.readPressure());
    Serial.println(" Pa");
    Serial.print("Altitude: ");
    Serial.print(bmp180.readAltitude());
    Serial.println(" m");
    delay(2000); //this wii be replaced with other function
    }
    exit(0); //exit the loop
}
