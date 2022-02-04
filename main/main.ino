#include <SoftwareSerial.h> 

// ------------ Debug mode configuration ------------//
  # define DEBUG_MODE true
  # define SMS_SIMULATION false // Simulate Sim800 serial in local Arduino serial

// ------------ Pins definition ------------//
  // Valve: 2 pins for H-bridge (A-, A+)
  #define PIN_VALVE_A1 12
  #define PIN_VALVE_A2 11

  // Water flow sensor
  #define PIN_FLOW_SENSOR 2
  
  // Voltage sensor
  #define PIN_VOLTAGE_SENSOR A0
  
  // Humidity sensor
  #define PIN_HUMIDITY_SENSOR A1
  
  // Temperature sensor
  #define PIN_TEMPERATURE_SENSOR A2

  // Safety relay
  #define PIN_SAFETY_RELAY 10

  // Input button
  #define PIN_INPUT_BUTTON 7
  
  // Serial TX and RX for communication with SIM800L
  #define PIN_RX 8
  #define PIN_TX 9
  
// Create object for serial communication with SIM800
SoftwareSerial SerialSIM800(PIN_RX, PIN_TX);

// ------------ Constants definition ------------//
  // Sample time for measurements
  const int SAMPLE_TIME = 1000;
  
  // Minimum time between consecutive irrigations in milliseconds
  const int TIME_BETWEEN_IRRIGATIONS = 2000;

  // Effective irrigation time in milliseconds
  const int EFFECTIVE_IRRIGATION_TIME = 7000;

  // Conversion factor from Hz to L/min flow for the water flow sensor
  const float FLOW_CONVERSION_FACTOR = 7.11;
  
  // Humidity threshold for irrigation (only irrigate below this value)
  const int HUMIDITY_THRESHOLD = 720;
  
  // Temperature threshold for irrigation (only irrigate over this value)
  const int TEMPERATURE_THRESHOLD = 980;
  
  // Voltage threshold for irrigation (only irrigate over this value)
  const int VOLTAGE_THRESHOLD = 6;
  
  // Phone number for the SMSs
  const String PHONE_NUMBER = "+34605521505";
  
  // SIM800L SIM number
  const String SIM_NUMBER = "+34681960644";
  
  // Size of buffer for incoming data from SIM800
  const int BUFFER_SIZE = 127;

  // Command to request the status of the measurements and total water volume
  const char* STATUS_COMMAND = "ESTADO";

// ------------ Global variables ------------//
  // Boolean for the irrigation valve to be open
  bool valveOpen = false;
  
  // Boolean for relay status
  bool relayOn = false;

  //Reference time for previous measurement
  long prevMeasurementTime = 0;   
  
  // Reference time when the irrigation started
  unsigned long irrigationStartTime = 0 ;

  // Counter for flow sensor pulses
  volatile int pulsesCount; 
  
  // Storage of total water volume since the program start
  float totalWaterVolume = 0; 
  
  // Variables for parsing the incoming SMS
  char bufferData[BUFFER_SIZE]; // Buffer for incoming data from SIM800 
  char bufferIndex; // Index for writing the data into the buffer
  
  bool cmtOk = false; // Boolean to signal that sender SMS parsing is on going
  int cmtIndex = 0; // Current position of index in the parsing of the sender number 
  
  bool msgOk = false; // Boolean to signal that the message payload parsing is on going
  int msgIndex = 0; // Current position of index in the parsing of the message  

 // Create the char arrays to store the SMS sender and payload
  char senderNum[14]; // Holds the phone number of the SMS sender (example "+34601234567")
  char message[100]; // Holds the message payload  
    
  // Variables for commands execution
  bool sendMeasurements = false; // When this is true, the Arduino will send an SMS with the measurements in the next iteration
  
void setup() 
{
  // Start serial connection 
  if(DEBUG_MODE) Serial.begin(9600); Serial.println ("Enviar 'r' para restablecer el volumen a 0 Litros"); 
  
  // Define pin modes
  pinMode(PIN_FLOW_SENSOR, INPUT); 
  pinMode(PIN_INPUT_BUTTON, INPUT_PULLUP); // Configura pin 7 interno pull-up resistor  Estado en 1, requiere 0 para activarse
  pinMode(PIN_VALVE_A1, OUTPUT); // Pin valvula A-
  pinMode(PIN_VALVE_A2, OUTPUT); // Pin valvula A+
  pinMode(PIN_SAFETY_RELAY, OUTPUT); // Salida rele para apagado voltage seguridad

  // Define interruption for flow sensor
  attachInterrupt(0, countPulses ,RISING);//(Interrupción 0(Pin2),función,Flanco de subida)
  interrupts();

  // Store the initial time
  prevMeasurementTime = millis();
  
  // Initialise SIM800  
  initializeSIM800();
  
  // DEBUGGING - SMS TEST
  //sendSMS("Hello from Arduino!", PHONE_NUMBER);
}

void loop() 
{
  // Read incoming data in SIM800
  readSIM800Data();
    
  // Time measurement
  long currentMillis = millis( );
  int dt = currentMillis - prevMeasurementTime; //Elapsed time since last measurement

  if(dt >= SAMPLE_TIME)
  {  
    prevMeasurementTime = currentMillis;      

    // Calculate water flow
    float waterFlow_L_min = (pulsesCount/FLOW_CONVERSION_FACTOR);
    // Reset pulses count to start new water flow measurement
    pulsesCount = 0;
        
    // Read serial input to flush volume storage or sum to the previous total volume instead
    if (Serial.available() && Serial.read()=='r') totalWaterVolume = 0;//restablece el volumen si recibe 'r'  
    else totalWaterVolume += (waterFlow_L_min/60)*(dt/1000); // volumen(L)=caudal(L/s)*tiempo(s)
    
    // Read sensors
    int humidity = analogRead(PIN_HUMIDITY_SENSOR); 
    int temperature = analogRead(PIN_TEMPERATURE_SENSOR);
    float voltage = analogRead(PIN_VOLTAGE_SENSOR)* (5.01 / 1023.00) * 1.24;
      
    // Check button status. If it is low, buttonPresed = true
    bool buttonPressed = !digitalRead(PIN_INPUT_BUTTON);  

    // Send measurements when requested through SMS
    if(sendMeasurements)
    {
      sendMeasurements = false; // Setting it to false to avoid send in loop
      sendMeasurementsSMS(humidity, temperature, voltage, totalWaterVolume);
    }
    
    // Print measurements only in debug mode
    if(DEBUG_MODE) printMeasurements(humidity, temperature, voltage, waterFlow_L_min, totalWaterVolume, buttonPressed);
    
    // If batteries are running out, temperature is too low or humidity is too high, turn off the system for safety and to save power
    if(humidity > HUMIDITY_THRESHOLD || voltage < VOLTAGE_THRESHOLD || temperature < TEMPERATURE_THRESHOLD)
    {
      // Ensure valves stay closed
      closeValve();
      valveOpen = false;
      
      if(relayOn)
      {
        // Turn off relay to remove power from the valve
        digitalWrite(PIN_SAFETY_RELAY, LOW);
        
        relayOn = false;
      }
    }
    else
    {
      if(!relayOn)
      {
        // Turn on relay to give power to the valve if it was off
        digitalWrite(PIN_SAFETY_RELAY, HIGH);
        
        relayOn = true;
      }
      
      // If irrigation is requested
      if (buttonPressed) 
      {
        // Only irrigate if enough time has passed since last irrigation
        if ((currentMillis - irrigationStartTime) >= (EFFECTIVE_IRRIGATION_TIME + TIME_BETWEEN_IRRIGATIONS) && !valveOpen) 
        {
          irrigationStartTime = currentMillis;
          valveOpen = true;
          openValve();
        }
      }
      
      // If the valve has been open for the configured EFFECTIVE_IRRIGATION_TIME, turn it off
      if (currentMillis - irrigationStartTime >= EFFECTIVE_IRRIGATION_TIME && valveOpen) 
      {
        valveOpen = false;
        closeValve();
      }
    }
  } 
  delay(10);
}

// -------- Function to open valve ---------- //
void openValve() 
{
  digitalWrite(PIN_VALVE_A1, HIGH);
  digitalWrite(PIN_VALVE_A2, LOW);
  delay(300);
  digitalWrite(PIN_VALVE_A1, LOW);
  if(DEBUG_MODE) Serial.println("OPEN RIEGO");
}

// -------- Function to close valve ---------- //
void closeValve() 
{
  digitalWrite(PIN_VALVE_A2, HIGH);
  digitalWrite(PIN_VALVE_A1, LOW);
  delay(300);
  digitalWrite(PIN_VALVE_A2, LOW);
  if(DEBUG_MODE) Serial.println("CLOSED RIEGO");
}

//---Función que se ejecuta en interrupción (ISR) ---------------//
void countPulses ()  
{   
  pulsesCount++;  //incrementamos la variable de pulsos 
} 

//--- Function for pretty-printing the measurements--------//
void printMeasurements(int hum, int temp, float voltage, float waterFlow, float totalVolume, bool buttonPressed)
{
  if(!DEBUG_MODE) return;
  Serial.print("Humedad: ");
  Serial.println(hum);
  Serial.print("Temperatura: ");
  Serial.println(temp);
  Serial.print("Voltage: ");
  Serial.println(voltage);
  Serial.print("Caudal: "); 
  Serial.print(waterFlow,3); 
  Serial.print("L/min\tVolumen: "); 
  Serial.print(totalVolume,3); 
  Serial.println(" L");
  Serial.print("Boton pulsado: ");
  Serial.println(buttonPressed);
}

//--- Function to initialize and configure the module
void initializeSIM800()
{
  SerialSIM800.begin(9600);
  SerialSIM800.println("AT");                  // Sends an ATTENTION command, reply should be OK
  delay(1000);
  SerialSIM800.println("AT+CMGF=1");           // Configuration for sending SMS
  delay(1000);
  SerialSIM800.println("AT+CNMI=1,2,0,0,0");   // Configuration for receiving SMS
  delay(1000);
  SerialSIM800.println("AT&W");                // Save the configuration settings
  delay(1000);
  
  // Initialize the buffer for reading the incoming Serial data from SIM800
  memset(bufferData, 0, sizeof(bufferData)); // Initialize the string 
  bufferIndex = 0;
}

//--- Function to send an SMS to the given phone number and with the given text
void sendSMS(String text, String phone_number)
{
  if(DEBUG_MODE) Serial.println("Sending SMS");
  
  //Your phone number don't forget to include your country code, example +212123456789"
  SerialSIM800.print("AT+CMGS=\""+phone_number+"\"\r");  
  delay(500);
  
  //This is the text to send
  SerialSIM800.print(text);       
  delay(500);
  
  //Required according to the datasheet)
  SerialSIM800.print((char)26);
  delay(500);
  
  SerialSIM800.println();
  
  if(DEBUG_MODE) Serial.println("Text Sent.");
}

//--- Function that checks if something has been received by SIM800 and parses it
//    It also prints the text if DEBUG is enabled 
//    The sender number is stored in variable senderNum
//    The message is stored in variable message
// ---
void readSIM800Data()
{
  // If SMS_SIMULATION is ON, data will be get from the user input in the console
  bool messageReceived = SMS_SIMULATION ? (Serial.available() > 0) : SerialSIM800.available() > 0;
  if(messageReceived)
  {
    if(DEBUG_MODE) Serial.println("---------------------------------> Message has been received!");
    
    while (SMS_SIMULATION ? (Serial.available() > 0) : (SerialSIM800.available() > 0))
    {     
      bufferData[bufferIndex] = SMS_SIMULATION ? Serial.read() : SerialSIM800.read();            
      
      // Finds the string "CMT:"
      // if found, reset the senderNum buffer
      if( (bufferData[bufferIndex-3] == 'C') && 
        (bufferData[bufferIndex-2] == 'M') && 
        (bufferData[bufferIndex-1] == 'T') && 
        (bufferData[bufferIndex] == ':')      )  {            
        if(DEBUG_MODE) Serial.print("CMT: ");  
        cmtOk = true;
        memset(senderNum, 0, sizeof(senderNum));    
        cmtIndex = 0;            // reset pos counter 
      }
    
      // String "CMT:" is found, 
      // parse the sender number for the reply
      // +CMT: "+34601234567"
      if ( cmtOk )
      {
        if(bufferData[bufferIndex] != ' ' && bufferData[bufferIndex] != '"' && bufferData[bufferIndex] != ':') 
        {         
          senderNum[cmtIndex] =  bufferData[bufferIndex];
          if(DEBUG_MODE) Serial.print(senderNum[cmtIndex]);
          cmtIndex++;
        } 
        else 
        {    
          if( cmtIndex > 0 ) // The number has been actually parsed -> The detected " was the end of the number and not the start. Otherwise, don't do anything
          cmtOk = false; // done
        }
      } 
    
      // CMT has been already parsed, the rest is the message. TODO: Review this, there are more fields. Look at the file Sample_SMS.txt
      if(cmtOk == false && cmtIndex > 0 && msgOk == false)
      {
        if(DEBUG_MODE) Serial.println(); Serial.print("Message: ");
        msgOk = true;
        memset(message, 0, sizeof(message));    
        msgIndex = 0;            // reset pos counter 
      }

      // msgOk means that the message is in the process of being parsed
      if ( msgOk )
      {
        message[msgIndex] = bufferData[bufferIndex];
        if(DEBUG_MODE) Serial.print(message[msgIndex]);
        msgIndex++;
      }

      bufferIndex++;
    }     
  
    // Clean buffer
    memset(bufferData, 0, sizeof(bufferData)); // Initialize the string 
    bufferIndex = 0;
    msgOk = false;
    if(DEBUG_MODE) Serial.println("---------------------------------> End of message");

    evaluateSmsCommand();
  }
}

//--- Function that checks whether the received SMS matches any of the predefined commands and executes it ---//
void evaluateSmsCommand()
{
  //First check for the status command
  if(strstr(message, STATUS_COMMAND))
  {
    sendMeasurements = true;
  }
}

//--- Function to format and send the SMS with the sensor measurements ---//
void sendMeasurementsSMS(int hum, int temp, float voltage, float totalVolume)
{
  char payload[100];

  // Convert floatt to char arrays. Sprintf does not work with floats
  char voltageStr[10];
  char volumeStr[10];
  dtostrf(voltage,3,2,voltageStr);// Minimum 3 digits (with the decimal point) and 2 decimals of precision
  dtostrf(totalVolume,3,2,volumeStr);
 
  sprintf(payload, "HUM:%d,TEMP:%d,VOLT:%s,LITROS:%s",hum,temp,voltageStr,volumeStr);
  if(DEBUG_MODE)
  {
    Serial.print("SMS payload: ");
    Serial.println(payload);
    Serial.print("Phone number: ");
    Serial.println(senderNum);
  }
  
   sendSMS(payload, senderNum);
}
