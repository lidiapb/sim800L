/*
 * This file is dedicated to the management of the GSM module SIM800L
 * The communication with the module is done through Serial communication 
 * with the configured RX and TX pins.
 */

#include <SoftwareSerial.h> // For serial communication with the SIM800L module

#define SMS_SIMULATION false // Simulate Sim800 serial in local Arduino serial. Example message: +CMT: "+34605521505","","22/02/04,01:47:51+04" RIEGO,MEASUREMENTS

// Serial TX and RX for communication with SIM800L
#define PIN_RX 8
#define PIN_TX 9
  
// Create object for serial communication with SIM800L
SoftwareSerial SerialSIM800(PIN_RX, PIN_TX);

// ------------ Constants definition ------------//
const String PHONE_NUMBER = "+34605521505"; // Phone number for the SMS alerts
const String SIM_NUMBER = "+34681960644"; // SIM800L SIM number
  
const int BUFFER_SIZE = 127; // Size of buffer for incoming data from SIM800

// ------------ Global variables ------------//
char bufferData[BUFFER_SIZE]; // Buffer for incoming data from SIM800 
char bufferIndex; // Index for writing the data into the buffer

bool cmtOk = false; // Boolean to signal that sender SMS parsing is on going
int cmtIndex = 0; // Current position of index in the parsing of the sender number 

bool msgOk = false; // Boolean to signal that the message payload parsing is on going
int msgIndex = 0; // Current position of index in the parsing of the message  

// Create the char arrays to store the SMS sender and payload
char senderNum[14]; // Holds the phone number of the SMS sender (example "+34601234567")
char message[100]; // Holds the message payload  

/*
 * Function to initialize and configure the module. Input buffer is read after every command to avoid buffer overflow
 */ 
void initializeSIM800()
{
  SerialSIM800.begin(9600);
  
  SerialSIM800.println("AT");                  // Sends an ATTENTION command for initial handshake, reply should be OK
  delay(100);
  readSIM800Data();
  
  SerialSIM800.println("AT+CCID");             //Read SIM information to confirm whether the SIM is plugged
  delay(100);
  readSIM800Data();

  SerialSIM800.println("AT+CMGF=1");           // Configuration for sending SMS
  delay(100);
  readSIM800Data();
  
  SerialSIM800.println("AT+CNMI=1,2,0,0,0");   // Configuration for receiving SMS
  delay(100);
  readSIM800Data();
  
  SerialSIM800.println("AT+CSQ");              // Check signal strength
  delay(100);
  readSIM800Data();
  
  SerialSIM800.println("AT+CREG?");            // Network registration test
  delay(100); 
  readSIM800Data();
  
  SerialSIM800.println("AT&W");                // Save the configuration settings
  delay(100);
  readSIM800Data();
  
  // Initialize the buffer for reading the incoming Serial data from SIM800
  memset(bufferData, 0, sizeof(bufferData));
  bufferIndex = 0;
}


/*
 * Function to send an SMS to the given phone number and with the given text
 */
void sendSMS(String text, String phone_number)
{
  logData("Sending SMS: " + text + " to number: " + phone_number);
  
  //Your phone number don't forget to include your country code, example +212123456789"
  SerialSIM800.print("AT+CMGS=\""+phone_number+"\"\r");  
  delay(100);
  readSIM800Data();
  
  //This is the text to send
  SerialSIM800.print(text);       
  delay(100);
  readSIM800Data();
  
  //Required according to the datasheet)
  SerialSIM800.print((char)26);
  delay(10000);
  readSIM800Data();
}

/*
 * Function that checks if something has been received by SIM800 and parses it
 * It also prints the text if DEBUG is enabled    
 * The sender number is stored in variable senderNum
 * The message is stored in variable message
 */
char* readSIM800Data()
{
  // If SMS_SIMULATION is ON, data will be get from the user input in the console
  if(SMS_SIMULATION ? seriaInputAvailable() : (SerialSIM800.available() > 0))
  {    
    while (SMS_SIMULATION ? seriaInputAvailable() : (SerialSIM800.available() > 0))
    {     
      bufferData[bufferIndex] = SMS_SIMULATION ? readSerialInput() : SerialSIM800.read();                  
      
      // Finds the string "CMT:"
      // if found, reset the senderNum buffer
      if( !cmtOk &&
        (bufferData[bufferIndex-3] == 'C') && 
        (bufferData[bufferIndex-2] == 'M') && 
        (bufferData[bufferIndex-1] == 'T') && 
        (bufferData[bufferIndex] == ':')      )  {            
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
          cmtIndex++;
        } 
        else 
        {    
          if( cmtIndex > 0 ) // The number has been actually parsed -> The detected " was the end of the number and not the start. Otherwise, don't do anything
          cmtOk = false; // done
        }
      } 
    
      // CMT has been already parsed, the rest is the message
      if(cmtOk == false && cmtIndex > 0 && msgOk == false)
      {
        msgOk = true;
        memset(message, 0, sizeof(message));    
        msgIndex = 0;            // reset pos counter 
      }

      // msgOk means that the message is in the process of being parsed
      if ( msgOk )
      {
        message[msgIndex] = bufferData[bufferIndex];
        msgIndex++;
      }

      bufferIndex++;
    }     

    logData("------------------> New message:\nbufferData"); 
    
    // Clean buffer
    memset(bufferData, 0, sizeof(bufferData)); // Initialize the string 
    bufferIndex = 0;
    msgOk = false;
    cmtOk = false;    
  }
  return bufferData;
}

/* 
 * Function to format and send the SMS with the sensor measurements. The used phone number is the one read from the received request  
 */
void sendMeasurementsSMS(int hum, int temp, float voltage, float totalVolume)
{
  char payload[100];

  // Convert floatt to char arrays. Sprintf does not work with floats
  char voltageStr[10];
  char volumeStr[10];
  dtostrf(voltage,3,2,voltageStr);// Minimum 3 digits (with the decimal point) and 2 decimals of precision
  dtostrf(totalVolume,3,2,volumeStr);
 
  sprintf(payload, "HUM:%d,TEMP:%d,VOLT:%s,LITROS:%s",hum,temp,voltageStr,volumeStr);

  sendSMS(payload, senderNum);
}

/*
 * Function to send SMS alert when the voltage is too low. The used SMS is the one that is configured by default in the application
 */ 
void sendLowVoltageSMSAlert(float voltage)
{
  char payload[100];

  // Convert floatt to char arrays. Sprintf does not work with floats
  char voltageStr[10];
  dtostrf(voltage,3,2,voltageStr);// Minimum 3 digits (with the decimal point) and 2 decimals of precision
 
  sprintf(payload, "ALERTA! Voltage demasiado bajo: %s V",voltageStr);
  
  sendSMS(payload, PHONE_NUMBER);
}

/*
 * Function to inform the user that the requested irrigation is being executed
 */
void sendIrrigationConfirmationSMS()
{
  sendSMS("OK! Riego en marcha", senderNum);
}
