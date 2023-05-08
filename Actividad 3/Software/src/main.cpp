#include <Arduino.h>
#include <DHTesp.h>
#include <WiFi.h>               // incluimos libreria de manejo de wifi
#include <PubSubClient.h>       // incluimos libreria de publicaciones y susbcripciones
#include <NTPClient.h>          // incluimos libreria de NTPClient
#include <WiFiUdp.h>            // incluimos libreria de WiFiUdp
#include <LiquidCrystal.h>


const char* ssid = "TP-LINK_B33E";
const char* password = "50868155";


const char *mqtt_server = "tstproyecto1grupo6.ddns.net";// dns del broker mosquitto (MQTT)
unsigned int mqtt_port = 1883;                      // socket port del server MQTT Mosquitto
const char *topico = "/actividad3/casa/";                // topico para publicar los datos en el server


//declaramos pines
const int DHT_Hab1 = 0; //sensor 1
const int DHT_Hab2 = 4; //sensor 2
const int DHT_Hab3 = 15; //sensor 3

int pote1 = 16;
int pote2 = 16;
int pote3 = 16;
String rx = "";

const int canal1 = 23; // PWM 1
const int canal2 = 22; // PWM 2
const int canal3 = 21; // PWM 3
    
// Constantes del controlador
float cocina;
float dormitorio;
float living;
double errorPasH_1 = 0; // suma de errores anteriores para el controlador 1
double errorPasH_2 = 0; // suma de errores anteriores para el controlador 2
double errorPasH_3 = 0; // suma de errores anteriores para el controlador 3
float U1, U2, U3;       // señal de control para los actuadores
int Kp = 7;             // constante proporcional
int Ki = 3;             // constante integral
float H, ref1, ref2, ref3; 

int TiempoMuestreo = 1;   // tiempo de muestreo en miliseg
unsigned long pasado = 0; // tiempo pasado para asegurar el tiempo de muestreo
unsigned long ahora;  // tiempo actual
int ciclo=0;
int ciclo1=0;

const int rs = 14, en = 12, d4 = 33, d5 = 25, d6 = 26, d7 = 27;
LiquidCrystal LCD(14,12,33,25,26,27);
WiFiClient esp32_Client;                                  // creacion de objeto wifi client
PubSubClient grupo6(esp32_Client);                        // creacion de objeto pubsunclient
WiFiUDP ntpUDP;                                           // obejto udp para la hora
NTPClient tiempo(ntpUDP, "ntp.ign.gob.ar", -10800, 8000); //objeto del server Hora

DHTesp dhtHabitacion1;
DHTesp dhtHabitacion2;
DHTesp dhtHabitacion3;

//------------------------------------------------------------------------------------
// Funciones subscripción
//------------------------------------------------------------------------------------
void Suscribe_MQTT()
{
  grupo6.subscribe("/actividad3/casa/");
}
//------------------------------------------------------------------------------------
// Funciones reconexion
//------------------------------------------------------------------------------------
void Reconectar_MQTT()
{
  while (!grupo6.connected())
  {
    String clientId = "TSTGrupo6";
    clientId += String(random(0xffff), HEX);
    if (grupo6.connect(clientId.c_str()))
    {
      Serial.println("Coneccion exitosa a Broker MQTT");
      Suscribe_MQTT();
    }
    else
    {
      Serial.println("Falla en el estado de conexion");
      Serial.println(grupo6.state());
      Serial.println("Reintentando en 5 segundos");
      delay(4000);
    }
  }
}
//------------------------------------------------------------------------------------
// Funciones Callback
//------------------------------------------------------------------------------------
void callback(char *topic, byte *payload, unsigned int length)
{ for (int i = 0; i < 3; i++)
  {
    rx +=(char)payload[i];
  }
    pote1 = rx.toInt();
    rx="";
}
//------------------------------------------------------------------------------------
// Funciones Reconexion
//------------------------------------------------------------------------------------
void Conectar_MQTT()
{
  Serial.println("Conectando a Broker MQTT");
  grupo6.setServer(mqtt_server, 1883);
  Suscribe_MQTT();
  grupo6.setCallback(callback);
}
//------------------------------------------------------------------------------------
// Funciones revisa conexion
//------------------------------------------------------------------------------------
void Revisar_conexion_MQTT()
{
  if (!grupo6.connected())
  {
    Reconectar_MQTT();
  }
  grupo6.loop();
}

//------------------------------------------------------------------------------------
// Funcionespublica por mqtt datos cocina
//------------------------------------------------------------------------------------
void publicaCocina(){
    TempAndHumidity  data = dhtHabitacion1.getTempAndHumidity();
    
    cocina = data.temperature;               // adquirimos temperatura del DHT11
    String cocina_2 = String(cocina,2);                      // Convierto el float a string
    int str_len4 = cocina_2.length() + 1;               // cargo el largo del float a una variable
    char cocina_1[str_len4];                          // cargo el largo de la temperatura en al arreglo
    cocina_2.toCharArray(cocina_1, str_len4);             // convierto el envio3 en arreglo y del largo de temp1
    grupo6.publish("/actividad3/cocina/temperatura/", cocina_1);     // publico en el broker el topico y el arreglo 
    delay(100);


    cocina = ref1;               // adquirimos temperatura del DHT11
    cocina_2 = String(cocina,2);                      // Convierto el float a string
    str_len4 = cocina_2.length() + 1;               // cargo el largo del float a una variable
    cocina_1[str_len4];                          // cargo el largo de la temperatura en al arreglo
    cocina_2.toCharArray(cocina_1, str_len4);             // convierto el envio3 en arreglo y del largo de temp1
    grupo6.publish("/actividad3/cocina/setpoint/", cocina_1);     // publico en el broker el topico y el arreglo 
    delay(100);

    
    cocina = ciclo1;               // adquirimos temperatura del DHT11
    cocina_2 = String(cocina,2);                      // Convierto el float a string
    str_len4 = cocina_2.length() + 1;               // cargo el largo del float a una variable
    cocina_1[str_len4];                          // cargo el largo de la temperatura en al arreglo
    cocina_2.toCharArray(cocina_1, str_len4);             // convierto el envio3 en arreglo y del largo de temp1
    grupo6.publish("/actividad3/cocina/pwm/", cocina_1);     // publico en el broker el topico y el arreglo 
    delay(100);
}

float PI_Controller(DHTesp &sensor, int LCD_row, float &temp_set, float Kp, float Ki, int TiempoMuestreo, double &errorPasH)
{
  double errorH;                      // error actual
  TempAndHumidity  data = sensor.getTempAndHumidity();
  ahora = millis();                   // tiempo actual
  int CambioTiempo = ahora - pasado;  // dif de tiempo actual - pasado
  if (CambioTiempo >= TiempoMuestreo) // si el tiempo de muestreo se cumple
  {
    errorH = temp_set - data.temperature;                         // calculo error actual
    errorPasH = errorH * TiempoMuestreo + errorPasH; // suma acumulativa de error
    H = ((Kp * errorH) + (Ki * errorPasH));          // calculo de la señal de control
    pasado = ahora;                                  // Actualiza tiempo
    int PWM_value = constrain(H, -255, 255); // Convertir U a un valor entre -255 y 255
    if (PWM_value < 50 && PWM_value > -50) // si el valor de la señal de control es menor a 50
    {
      PWM_value = 0;
    }
    LCD.setCursor(0, LCD_row);
    LCD.print("Temp: " + String(data.temperature) + " C");
    return PWM_value; // retornamos la señal de control
  }
  return 0;
}


void setup()
{
  Serial.begin(9600);
  WiFi.begin(ssid, password);                     // conecto al wifi del lugar (micasa)
  delay(2000);
  Conectar_MQTT();
  tiempo.begin();                             // inicializo el objeto del servidor de fecha y hora
  LCD.begin(20,4);
 
  // declaro senseores
  dhtHabitacion1.setup(DHT_Hab1, DHTesp::DHT11);
  dhtHabitacion2.setup(DHT_Hab2, DHTesp::DHT11);
  dhtHabitacion3.setup(DHT_Hab3, DHTesp::DHT11);
  delay(2000);
}

void loop()
{
   // SENSOR 1
  LCD.clear(); //borramos los datos del LCD
  LCD.setCursor(0, 0);
  LCD.print("Cocina ");
  //int valuPote1 = analogRead(pote1);                                       // lectura del potenciometro
  ref1 = pote1;// map(pote1, 0, 40, 0, 40); //mapeo el potenciometro en rando de 16 a 30
  LCD.setCursor(0, 2);
  LCD.print("Temp Set " + String(ref1));
 
  U1 = PI_Controller(dhtHabitacion1, 1, ref1, Kp, Ki, TiempoMuestreo, errorPasH_1); // seña de control para el sensor 1
  Serial.println("Error PI H1: "+String(H));
  Serial.println("PWM H1: "+String(U1));
  if (U1 > 0)// si la señal de control es mayor a 0 necesito calefaccion
  {
    LCD.setCursor(0, 3);
    LCD.print("Calefaccion");
    Serial.println("CALEFFACCION");
    analogWrite(canal1, U1); //"Actua calefaccion"
    publicaCocina();
  }
  else // si la señal de control es menor a 0 necesito refrigeracion
  {
    LCD.setCursor(0, 3);
    LCD.print("Refrigeracion = ");
    ciclo= (255-(U1*(-1)));
    analogWrite(canal1, U1 * (-1)); //"actua refrigeracion"
    analogWrite(canal1, ciclo); //"actua refrigeracion" .
    ciclo1 = map(ciclo, 255, 0, 0,100);
    LCD.print(ciclo1); 
    LCD.print("%"); 
    publicaCocina();
  }

delay(1000);
/*
   //SENSOR 2
  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.print("Habitacion 2");
  //int valuPote2 = analogRead(pote2);
  float ref2 = map(pote2, 0, 4096, 16, 30);
  LCD.setCursor(0, 2);
  LCD.print("Temp Set " + String(ref2));
  U2 = PI_Controller(dhtHabitacion2, 1, ref2, 2, 2, TiempoMuestreo, errorPasH_2);
  Serial.println("Error PI H2: "+String(H));
  Serial.println("PWM H2: "+String(U2));
  if (U2 > 0)
  {
    LCD.setCursor(0, 3);
    LCD.print("Calefaccion");
    analogWrite(canal2, U2); //"Actua calefaccion"
  }
  else
  {
    LCD.setCursor(0, 3);
    LCD.print("Refrigeracion = ");
    ciclo = (255-(U2*(-1)));
    //analogWrite(canal2, U2 * (-1)); //"actua refrigeracion"
    analogWrite(canal2, ciclo); //"actua refrigeracion"
    ciclo1 = map(ciclo, 255, 0, 0,100);
    LCD.print(ciclo1); 
    LCD.print("%"); 
  }

  delay(3000);

  // SENSOR 3
  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.print("Habitacion 3");
  //int valuPote3 = analogRead(pote3);
  float ref3 = map(pote3, 0, 4096, 16, 30);
  LCD.setCursor(0, 2);
  LCD.print("Temp Set " + String(ref3));
  U3 = PI_Controller(dhtHabitacion3, 1, ref3, 10, 10, TiempoMuestreo, errorPasH_3);
  Serial.println("Error PI H3: "+String(H));
  Serial.println("PWM H3: "+String(U3));
  if (U3 > 0)
  {
    LCD.setCursor(0, 3);
    LCD.print("");
    LCD.print("Calefaccion");
    analogWrite(canal3, U3); //"Actua calefaccion"
  }
  else
  {
    LCD.setCursor(0, 3);
    LCD.print("Refrigeracion = ");
    ciclo= (255-(U3*(-1)));
    //analogWrite(canal3, U3 * (-1)); //"actua refrigeracion"
    analogWrite(canal3, ciclo); //"actua refrigeracion"
    ciclo1 = map(ciclo, 255, 0, 0,100);
    LCD.print(ciclo1); 
    LCD.print("%");    
  }
  Serial.println("---------------------------------");
  */
         //if (!grupo6.connected())                     // si la conexion esta negada reconecto
        Revisar_conexion_MQTT();
        //delay(5000);
}