#include <SoftwareSerial.h> 

// Debug mode configuration
# define DEBUG_MODE true

// Serial TX and RX for communication with SIM800L
#define PIN_RX 8
#define PIN_TX 9

// Create object for serial communication with SIM800
SoftwareSerial SerialSIM800(PIN_RX, PIN_TX);

// Phone number for the SMSs
const String PHONE_NUMBER = "+34605521505";

void setup() 
{
	// Start serial connection 
	if(DEBUG_MODE) Serial.begin(9600);
	
	// Initialise serial communication with SIM800 for AT commands	
	SerialSIM800.begin(9600);
	
	// Delay 1 second to wait for connection with network
	delay(1000);
	
	// DEBUGGING - SMS TEST
	sendSMS("Hello from SIM800L", PHONE_NUMBER);
}

void loop()
{
  if(DEBUG_MODE) 
	{
	  while (SerialSIM800.available())
	  {            
			//Displays on the serial monitor if there's a communication from the module
			Serial.write(SerialSIM800.read()); 
		}
	}
}

void sendSMS(String text, String phone_number)
{
	if(DEBUG_MODE) Serial.println("Sending SMS..."); //Show this message on serial monitor
	//Set the module to SMS mode
	SerialSIM800.print("AT+CMGF=1\r");                   
	delay(100);
	
	//Your phone number don't forget to include your country code, example +212123456789"
	SerialSIM800.print("AT+CMGS=\""+phone_number+"\"\r");  
	delay(500);
	
	//This is the text to send
	SerialSIM800.print(text);       
	delay(500);
	
	//(required according to the datasheet)
	SerialSIM800.print((char)26);

	if(DEBUG_MODE) Serial.println("Text Sent.");
}