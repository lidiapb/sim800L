#include <SoftwareSerial.h>
SoftwareSerial Sim800l(8,9);

void setup() 
{
  Serial.begin(9600);
  Sim800l.begin(9600);
}

void loop() 
{
    while(Sim800l.available())
    {
      Serial.write(Sim800l.read());
    }
    
    while(Serial.available())
    {
      Sim800l.write(Serial.read());
    }
}
