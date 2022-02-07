// ------------ Constants definition ------------//
  // Sample time for measurements
  const int SAMPLE_TIME = 1000;
  
  // Minimum time between consecutive irrigations in milliseconds
  const int TIME_BETWEEN_IRRIGATIONS = 2000;

  // Effective irrigation time in milliseconds
  const int EFFECTIVE_IRRIGATION_TIME = 7000;
  
  // Humidity threshold for irrigation (only irrigate below this value)
  const int HUMIDITY_THRESHOLD = 720;
  
  // Temperature threshold for irrigation (only irrigate over this value)
  const int TEMPERATURE_THRESHOLD = 980;
  
  // Voltage threshold for irrigation (only irrigate over this value) and SMS alert
  const int VOLTAGE_THRESHOLD = 6;  

  // Command to request the status of the measurements and total water volume
  const char* STATUS_COMMAND = "ESTADO";
  
  // Command to request a manual irrigation (similar to pressing the button)
  const char* IRRIGATION_COMMAND1 = "REGAR";
  const char* IRRIGATION_COMMAND2 = "RIEGO";

// ------------ Global variables ------------//
  //Reference time for previous measurement
  long prevMeasurementTime = 0;   
  
  // Reference time when the irrigation started
  unsigned long irrigationStartTime = 0 ;

  // Counter for flow sensor pulses
  volatile int pulsesCount; 
  
  // Storage of total water volume since the program start
  float totalWaterVolume = 0;    
    
  // Variables for commands execution
  bool sendMeasurements = false; // When this is true, the Arduino will send an SMS with the measurements in the next iteration

  // Flag to only send the SMS alert once. When the battery is charged, the program will be reset and the flag will be false again
  bool lowVoltageSmsSent = false;

  // Flag to mark that an irrigation has been requested by SMS
  bool remoteIrrigationPending = false;
  
void setup() 
{  
  initalizeSensorsAndActuators();
    
  initializeSIM800();

  totalWaterVolume = getFromMemory();  
}

void loop() 
{
  char* receivedData = readSIM800Data();
  
  // Only evaluate SMS command if a CMT command was received
  if(strstr(receivedData, "CMT:")) 
  {
    evaluateSmsCommand(receivedData);
  }
    
  // Time measurement
  long currentMillis = millis();
  int dt = currentMillis - prevMeasurementTime; //Elapsed time since last measurement

  if(dt >= SAMPLE_TIME || currentMillis == 0) //Run also on first iteration
  {  
    prevMeasurementTime = currentMillis;          

  // Read sensors
    int humidity; int temperature; float voltage; float waterFlow_L_min; bool buttonPressed;
    readSensors(&humidity, &temperature, &voltage, &waterFlow_L_min, &buttonPressed);

    // Read serial input to flush volume storage or sum to the previous total volume instead
    if(seriaInputAvailable() && readSerialInput() =='r')    
    {
      logData("Clearing total water volume and EEPROM ");
      
      //Flush local variable 
      totalWaterVolume = 0;

      clearMemory();
    }
    else totalWaterVolume += (waterFlow_L_min/60)*(dt/1000); // volumen(L)=caudal(L/s)*tiempo(s)

    // Persist the totalVolume to the EEPROM when the configured time has passed
    writeToMemory(totalWaterVolume, currentMillis);    
        
    // SMS alert if voltage is under threshold
    if(voltage < VOLTAGE_THRESHOLD && !lowVoltageSmsSent)
    {      
      lowVoltageSmsSent = true;
      sendLowVoltageSMSAlert(voltage);
    }         

    // Send measurements when requested through SMS
    if(sendMeasurements)
    {
      sendMeasurements = false; // Setting it to false to avoid send in loop
      sendMeasurementsSMS(humidity, temperature, voltage, totalWaterVolume);
    }
    
    // Print measurements only in debug mode
    printMeasurements(humidity, temperature, voltage, waterFlow_L_min, totalWaterVolume, buttonPressed);
    
    // If batteries are running out, temperature is too low or humidity is too high, turn off the system for safety and to save power
    if(humidity > HUMIDITY_THRESHOLD || voltage < VOLTAGE_THRESHOLD || temperature < TEMPERATURE_THRESHOLD)
    {
      // Ensure valves stay closed
      closeValve();      
      turnRelayOff();
    }
    else
    {
      turnRelayOn();
      
      // If irrigation is requested
      if (buttonPressed || remoteIrrigationPending) 
      {
        // Only irrigate if enough time has passed since last irrigation
        if ((currentMillis - irrigationStartTime) >= (EFFECTIVE_IRRIGATION_TIME + TIME_BETWEEN_IRRIGATIONS)) 
        {          
          irrigationStartTime = currentMillis;
          openValve();

          // If the request was remote, clear the flag and notify the user that the irrigation is starting
          if(remoteIrrigationPending)
          {
            remoteIrrigationPending = false;
            sendIrrigationConfirmationSMS();
          }
        }
      }
      
      // If the valve has been open for the configured EFFECTIVE_IRRIGATION_TIME, turn it off
      if (currentMillis - irrigationStartTime >= EFFECTIVE_IRRIGATION_TIME) 
      {
        closeValve();
      }
    }
  } 
  delay(10);
}

//--- Function that checks whether the received SMS matches any of the predefined commands and executes it ---//
void evaluateSmsCommand(char* message)
{
  //First check for the status command
  if(strstr(message, STATUS_COMMAND))
  {    
    sendMeasurements = true;
    logData("Measurements request received!");
  }

  if(strstr(message, IRRIGATION_COMMAND1) || strstr(message, IRRIGATION_COMMAND2))
  {
    remoteIrrigationPending = true;    
    logData("Remote irrigation request received!");
  }
}
