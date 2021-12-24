#include <SoftwareSerial.h> 

// ------------ Debug mode configuration ------------//
	# define DEBUG_MODE true

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

// ------------ Global variables ------------//
	// Boolean for the irrigation valve to be open
	bool valveOpen = false;
	
	// Boolean for relay status
	bool relayOn = false;

    //Reference time for previous water volume update
	long prevFlowMeasurementTime = 0; 
	
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
	int cmtStartPos = 0; // Position of the start of sender number in the buffer
	int cmtIndex = 0; // Current position of index in the parsing
	char senderNum[14]; // Holds the phone number of the SMS sender (example "+34601234567")
	
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

	// Store the initial time
	prevFlowMeasurementTime = millis();
	
	// Initialise SIM800	
	initializeSIM800();
	
	// DEBUGGING - SMS TEST
	sendSMS("TEST", SIM_NUMBER);
}

void loop() 
{
	// Time measurement
	long currentMillis = millis( );
	
	// Read incoming data in SIM800
	readSIM800Data();
	
	// Read sensors
	int humidity = analogRead(PIN_HUMIDITY_SENSOR);	
	int temperature = analogRead(PIN_TEMPERATURE_SENSOR);
	float voltage = analogRead(PIN_VOLTAGE_SENSOR)* (5.01 / 1023.00) * 1.24;
	float waterFlow_L_min = measureWaterFlow();
	
	// Calculate water volume and sum to previous one
	int dt = currentMillis - prevFlowMeasurementTime; //calculamos la variación de tiempo
	prevFlowMeasurementTime = currentMillis;
	
	// Read serial input to flush volume storage
	if (Serial.available() && Serial.read()=='r') totalWaterVolume = 0;//restablece el volumen si recibe 'r'  
	else totalWaterVolume += (waterFlow_L_min/60)*(dt/1000); // volumen(L)=caudal(L/s)*tiempo(s)
		
	// Check button status. If it is low, buttonPresed = true
	bool buttonPressed = !digitalRead(PIN_INPUT_BUTTON);	
	
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
	delay(100);
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

//---Función para obtener frecuencia de los pulsos--------//
float measureWaterFlow() 
{
	pulsesCount = 0; //Ponemos a 0 el número de pulsos
	interrupts();    //Habilitamos las interrupciones
	delay(1000);   //muestra de 1 segundo
	noInterrupts(); //Deshabilitamos  las interrupciones
	
	float waterFlow = pulsesCount/FLOW_CONVERSION_FACTOR;
	return waterFlow;
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
	if(DEBUG_MODE) Serial.println("Sending SMS..."); //Show this message on serial monitor
	
	//Your phone number don't forget to include your country code, example +212123456789"
	SerialSIM800.print("AT+CMGS=\""+phone_number+"\"\r");  
	delay(500);
	
	//This is the text to send
	SerialSIM800.print(text);       
	delay(500);
	
	//Required according to the datasheet)
	SerialSIM800.print((char)26);
	delay(500);
	
	if(DEBUG_MODE) Serial.println("Text Sent.");
}

//--- Function that checks if something has been received by SIM800 and parses it
//    It also prints the text if DEBUG is enabled 
//    The sender number is stored in variable senderNum
//    The message is stored in variable message
// ---
void readSIM800Data()
{
  if(SerialSIM800.available() > 0)
  {
  	while (SerialSIM800.available() > 0)
  	{ 		
  		bufferData[bufferIndex] = SerialSIM800.read();	   
      
  		// Finds the string "CMT:"
  		// if found, reset the senderNum buffer and save cmtStartPos
  		if( (bufferData[bufferIndex-3] == 'C') && 
  			(bufferData[bufferIndex-2] == 'M') && 
  			(bufferData[bufferIndex-1] == 'T') && 
  			(bufferData[bufferIndex] == ':')      )  {            
  		  if(DEBUG_MODE) Serial.println("CMT");  
  		  cmtOk = true;
  		  memset(senderNum, 0, sizeof(senderNum));    
  		  cmtStartPos = bufferIndex;  // get the position
  		  cmtIndex = 0;            // reset pos counter 
  		}    
  		// String "CMT:" is found, 
  		// parse the sender number for the reply
  		// +CMT: "+34601234567"
  		if ( cmtOk && ( bufferData[bufferIndex-1] == '"' )  ) { // The number starts with character "
  		  if ( bufferData[bufferIndex] != '"' ) { // The number ends with character "
    			senderNum[cmtIndex] =  bufferData[bufferIndex];
    			cmtIndex++;
  		  } 
  		  else 
  		  {
    			if(DEBUG_MODE) Serial.println(senderNum);
    			cmtOk = false; // done
  		  }
  		} 
  	}
  
  	// Clean buffer
  	memset(bufferData, 0, sizeof(bufferData)); // Initialize the string 
  	bufferIndex = 0;
  }
}
