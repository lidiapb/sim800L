  #include <SoftwareSerial.h> 

  //valvula Riego
  const int ini_1 = 12;
  const int ini_2 = 11;

  int abrir = 0;
  int checkCerrar = 0;
  int desactivacionTotal = 0;

  int timeSalidaRiego = 2000; // tiempo minimo entre riegos
  unsigned long PreviousMillis = 0 ;
  const long interval = 7000 ; // tiempo de riego eficaz

  volatile int NumPulsos; //variable para la cantidad de pulsos recibidos
  int PinSensor = 2;    //Sensor flujo conectado en el pin 2
  float factor_conversion=7.11; //para convertir de frecuencia a caudal
  float volumen=0;
  long dt=0; //variación de tiempo por cada bucle
  long t0=0; //millis() del bucle anterior

  //---Función que se ejecuta en interrupción---------------
  void ContarPulsos ()  
{ 
  NumPulsos++;  //incrementamos la variable de pulsos
  
} 

  //---Función para obtener frecuencia de los pulsos--------
  int ObtenerFrecuecia() 
{
  int frecuencia;
  NumPulsos = 0;   //Ponemos a 0 el número de pulsos
  interrupts();    //Habilitamos las interrupciones
  delay(1000);   //muestra de 1 segundo
  noInterrupts(); //Deshabilitamos  las interrupciones
  frecuencia=NumPulsos; //Hz(pulsos por segundo)
  return frecuencia;
  
}

  void setup() {

  //start serial connection
  
  Serial.begin(9600);
  pinMode(PinSensor, INPUT); 
  attachInterrupt(0,ContarPulsos,RISING);//(Interrupción 0(Pin2),función,Flanco de subida)
  Serial.println ("Enviar 'r' para restablecer el volumen a 0 Litros"); 
  t0=millis();
  //configura pin 7 interno pull-up resistor
  pinMode(7, INPUT_PULLUP); // Estado en 1, requiere 0
  para activarse
  pinMode(ini_1, OUTPUT); // Pin valvula A-
  pinMode(ini_2, OUTPUT); // Pin valvula A+
  pinMode(10, OUTPUT); // Salida rele para apagado voltage seguridad

}

  void loop() {

  int sensorVal = digitalRead(7);
  int sensorValue = analogRead(A0);
  float voltage = sensorValue * (5.01 / 1023.00) * 1.24;
  int  humedad = analogRead(A1);
  int paramHumedad = 0;
  int temperatura = analogRead(A2);
  int paramTemperatura = 0;
  long currentMillis = millis ( ) ;

  if (Serial.available()) {
  if(Serial.read()=='r')volumen=0;//restablece el volumen si recibe 'r'
  
}

  float frecuencia=ObtenerFrecuecia(); //obtenemos frecuencia de los pulsos en Hz
  float caudal_L_m=frecuencia/factor_conversion; //calculamos el caudal en L/m
  dt=millis()-t0; //calculamos la variación de tiempo
  t0=millis();
  volumen=volumen+(caudal_L_m/60)*(dt/1000); // volumen(L)=caudal(L/s)*tiempo(s)

  Serial.print("humedad--");
  Serial.println(humedad);
  Serial.print("temperatura--");
  Serial.println(temperatura);
  Serial.println(sensorVal);
  Serial.println(voltage);
  Serial.print ("Caudal: "); 
  Serial.print (caudal_L_m,3); 
  Serial.print ("L/min\tVolumen: "); 
  Serial.print (volumen,3); 
  Serial.println (" L");
  
  if (humedad > 720 || voltage < 6 || temperatura < 980) {
  paramHumedad = 1;
  paramTemperatura = 1;
    
  // Elegir alarmas con desactivacion total:
    
  if (humedad > 720 || voltage < 6 || temperatura < 980) {
  if(desactivacionTotal==0){
  desactivacionTotal=1;
  cerrar_valvula();
  checkCerrar = 0;
  abrir = 0;
  digitalWrite(10, LOW);
}
} else {
  
  if ( currentMillis - PreviousMillis >= interval && abrir == 1 && checkCerrar == 0 ) {
  checkCerrar = 1;
  cerrar_valvula();
}
       
  if ( currentMillis - PreviousMillis >= (interval + timeSalidaRiego) && abrir == 1 ) {
  checkCerrar = 0;
  abrir = 0;
}
}
} else {
  paramHumedad = 0;
  desactivacionTotal =0;
  paramTemperatura = 0;
  digitalWrite(10, HIGH);
}
  if (paramHumedad == 0) {
  if (sensorVal == LOW) {

  //Riego intervalo minimo "x" segundos
  if ( currentMillis - PreviousMillis >= interval && abrir == 0 ) {
  PreviousMillis = currentMillis ;
  abrir = 1;
  abre_valvula();
}
}
  if ( currentMillis - PreviousMillis >= interval && abrir == 1 && checkCerrar == 0 ) {
  checkCerrar = 1;
  cerrar_valvula();
}

  if ( currentMillis - PreviousMillis >= (interval + timeSalidaRiego) && abrir == 1 ) {
  checkCerrar = 0;
  abrir = 0;
}
}

  delay(100);
 
}

  void abre_valvula() {
  digitalWrite(ini_1, HIGH);
  digitalWrite(ini_2, LOW);
  delay(300);
  digitalWrite(ini_1, LOW);
  Serial.println("OPEN RIEGO");
}

  void cerrar_valvula() {
  digitalWrite(ini_2, HIGH);
  digitalWrite(ini_1, LOW);
  delay(300);
  digitalWrite(ini_2, LOW);
  Serial.println("CLOSED RIEGO");
}
