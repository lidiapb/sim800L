// ------------ Pins definition ------------//
  // Valve: 2 pins for H-bridge (A-, A+)
  #define PIN_VALVE_A1 12
  #define PIN_VALVE_A2 11

  // Safety relay
  #define PIN_SAFETY_RELAY 10
  
  // Water flow sensor
  #define PIN_FLOW_SENSOR 2
  
  // Voltage sensor
  #define PIN_VOLTAGE_SENSOR A0
  
  // Humidity sensor
  #define PIN_HUMIDITY_SENSOR A1
  
  // Temperature sensor
  #define PIN_TEMPERATURE_SENSOR A2

  // Input button
  #define PIN_INPUT_BUTTON 7

  // Conversion factor from Hz to L/min flow for the water flow sensor
  const float FLOW_CONVERSION_FACTOR = 7.11;

// ------------ Global variables ------------//
  // Boolean for the irrigation valve to be open
  bool valveOpen = false;
  
  // Boolean for relay status
  bool relayOn = false;

  /*
   * Initialize pinModes and interruptions related to sensors and actuators
   */
  void initalizeSensorsAndActuators()
  {
    pinMode(PIN_FLOW_SENSOR, INPUT); 
    pinMode(PIN_INPUT_BUTTON, INPUT_PULLUP); // Configura pin 7 interno pull-up resistor  Estado en 1, requiere 0 para activarse
    pinMode(PIN_VALVE_A1, OUTPUT); // Pin valvula A-
    pinMode(PIN_VALVE_A2, OUTPUT); // Pin valvula A+
    pinMode(PIN_SAFETY_RELAY, OUTPUT); // Salida rele para apagado voltage seguridad

    // Define interruption for flow sensor
    attachInterrupt(0, countPulses ,RISING);//(Interrupción 0(Pin2),función,Flanco de subida)
    interrupts();
  }

  /*
   *  Get the sensor values and calculate the water flow from the pulses count in interuptions 
   */
  void readSensors(int* humidity, int* temperature, float* voltage, float* waterFlow_L_min, bool* buttonPressed) 
  {
    *humidity = analogRead(PIN_HUMIDITY_SENSOR); 
    *temperature = analogRead(PIN_TEMPERATURE_SENSOR);
    *voltage = analogRead(PIN_VOLTAGE_SENSOR)* (5.01 / 1023.00) * 1.24;
    *buttonPressed = !digitalRead(PIN_INPUT_BUTTON);  
    
    // Calculate water flow
    *waterFlow_L_min = (pulsesCount/FLOW_CONVERSION_FACTOR);
    // Reset pulses count to start new water flow measurement
    pulsesCount = 0;
  }

  /*
   * Función que se ejecuta en interrupción (ISR)
   */
  void countPulses ()  
  {   
    pulsesCount++;  //incrementamos la variable de pulsos 
  } 

  /*
   * Function to open valve if it is closed
   */
  void openValve() 
  {
    if(!valveOpen)
    {
      valveOpen = true;
    
      digitalWrite(PIN_VALVE_A1, HIGH);
      digitalWrite(PIN_VALVE_A2, LOW);
      delay(300);
      digitalWrite(PIN_VALVE_A1, LOW);
      
      logData("OPEN RIEGO");
    }
  }

  /*
   * Function to close valve if it is open
   */
  void closeValve() 
  {
    if(valveOpen)
    {
      valveOpen = false;
      
      digitalWrite(PIN_VALVE_A2, HIGH);
      digitalWrite(PIN_VALVE_A1, LOW);
      delay(300);
      digitalWrite(PIN_VALVE_A2, LOW);
      
      logData("CLOSED RIEGO");
    }
  }

  /*
   * Function to turn relay on if it is off
   */
  void turnRelayOn()
  {
    if(!relayOn)
    {
      digitalWrite(PIN_SAFETY_RELAY, HIGH);
      
      relayOn = true;
    }
  }

  /*
   * Function to turn relay off if it is on
   */
  void turnRelayOff()
  {
    if(relayOn)
    {
      digitalWrite(PIN_SAFETY_RELAY, LOW);
      
      relayOn = false;
    }
  }
