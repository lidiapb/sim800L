#include <EEPROM.h> // For persistent data storage into the memory

// ------------ Constants definition ------------//
  // Time between writes of total volume to the EEPROM in minutes 
  const int EEPROM_WRITE_SAMPLE_TIME = 60;

  // EEPROM memory adress to store the data to
  const int EEPROM_ADDRESS = 0;

// ------------ Global variables ------------//
  // EEPROM persistance last time
  long prevEepromWriteTime = 0;

/*
 * Get a float from the EEPROM
 */
float getFromMemory()
{
  float value;
  EEPROM.get(EEPROM_ADDRESS, value);
  if(isnan(value)) value = 0; //First time EEPROM is empty, so populate it with 0 as initial value
  
  logData("Last value from internal memory: "+String(value, 3));
  return value;
}

/*
 * Clear the EEPROM memory
 */
void clearMemory()
{
  //Clear EEPROM
  for (int idx = 0 ; idx < EEPROM.length() ; idx++) {
    EEPROM.write(idx, 0);        
  }
}

/*
 * Write a float to the EEPROM when a time bigger than the configured one elapses.
 */
void writeToMemory(float value, long currentTime)
{
  if((currentTime - prevEepromWriteTime)/60000 >= EEPROM_WRITE_SAMPLE_TIME)
  {
      prevEepromWriteTime = currentTime;

      // Write to EEPROM
       EEPROM.put(EEPROM_ADDRESS, value);
      logData("Wrote to EEPROM value: "+String(totalWaterVolume, 3));
  } 
}
