#include <SoftwareSerial.h> 

// Debug mode configuration
# define DEBUG_MODE true

// Serial TX and RX for communication with SIM800L
// TODO: Check NANO
#define PIN_RX 2
#define PIN_TX 3

// Create object for serial communication with SIM800
SoftwareSerial SerialSIM800(PIN_RX, PIN_TX);

// Phone number for the SMSs
const String PHONE_NUMBER = "+34605521505";

void setup() 
{
	// Start serial connection 
	if(DEBUG_MODE) Serial.begin(9600); Serial.println ("Enviar 'r' para restablecer el volumen a 0 Litros"); 
	
	// Initialise serial communication with SIM800 for AT commands	
	SerialSIM800.begin(9600);
	
	// DEBUGGING - SMS TEST
	sendSMS("Hello from SIM800L", PHONE_NUMBER);
}

void loop()
{
	if (SerialSIM800.available()){            
		if(DEBUG_MODE) 
		{
			//Displays on the serial monitor if there's a communication from the module
			Serial.println("Communication received from SIM800: ");
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
	delay(200);
	
	//This is the text to send
	SerialSIM800.print(text);       
	delay(200);
	
	//(required according to the datasheet)
	SerialSIM800.print((char)26);
	delay(200);
	
	SerialSIM800.println();
	
	if(DEBUG_MODE) Serial.println("Text Sent.");
	delay(200);	
}

// Serial event on monitor serial: Received message from module
void serialEvent(){   
	// Check if anything has been received in the SIM800
	if (SerialSIM800.available() > 0){            
		if(DEBUG_MODE) 
		{
			//Displays on the serial monitor if there's a communication from the module
			Serial.println("Message received from SIM800: ");
			Serial.write(SerialSIM800.readStringUntil("\n")); 
		}
	}
}