/*
 * This file contains all the functionalities dedicated to debugging and logging
 * It is mainly composed of functions that write to the Serial console
 * The behaviour is configured by using DEBUG_MODE. If set to false, logging to the 
 * console will be disabled.
 */

 # define DEBUG_MODE true

/*
 * This function initailises the Serial console if DEBUG_MODE is active and prints initial info.
 */
 void intialiseLogger()
 {
    if(DEBUG_MODE) 
    {
      Serial.begin(9600); 
      logData("Send 'r' to restore water volume to 0");
    }
 }

/*
 * Checks whether debug mode is enabled and prints data to the Serial console
 * Params:
 *  - String data: Content of the message to be logged
 *  - bool lineBreak: Whether to add a line break at the end of the line or not. Defaults to true
 */
 void logData(String data)
 {  
    // If debug mode is disabled, do not do anything
    if(DEBUG_MODE) Serial.println(data);  
 }

/*
 * Function for pretty-printing the measurements from the sensors
 * Params:
 *  - int hum: Humidity as an int (direct input from ADC, not converted)
 *  - int temp: Temperature as an int (direct input from ADC, not converted)
 *  - float voltage: Voltage in Volts
 *  - float waterFlow: Water flow in L/min
 *  - float totalVolume: Total water volume in L
 *  - bool buttonPressed: Manual irrigation button currently pressed
 */
 void printMeasurements(int hum, int temp, float voltage, float waterFlow, float totalVolume, bool buttonPressed)
 {
    if(!DEBUG_MODE) return;
    logData("Humedad: "+String(hum));    
    logData("Temperatura: "+String(temp));  
    logData("Voltaje: "+String(voltage));  
    logData("Caudal: "+String(waterFlow,3)+"L/min\tVolumen: "+String(totalVolume,3)+" L");  
    logData("Boton pulsado: "+buttonPressed);
 }

/*
 * Function to check if Serial input has osmthing available
 */
 bool seriaInputAvailable()
 {
    return (Serial.available()>0);
 }

/*
 * Function to read Serial input
 */
 char readSerialInput()
 {
    return (Serial.read());
 }
