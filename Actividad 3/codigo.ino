#include <analogWrite.h>
#include <WiFi.h>
#include <DHTesp.h>
#include <PubSubClient.h>

const int dhtPin=15; //pin dht
const int pinPote=32;
const int pinLedDimmer=33;
const int pinLed=14; 
const int pinPulsador=4;
bool estado=false; //defino estado
float tempAnt=0;
DHTesp sensorDHT;
int valuPote;

// ---------- Establezo parametros wifi ---------------
const char* ssid = "cyj";
const char* password = "abcd123456";
WiFiClient espClient;
// ----------------------------------------------------
//-------Establezco  Parametros MQTT ------------------
const char *mqtt_server = "138.59.172.29";
const int mqtt_port = 1883;
const char *mqtt_user = "esp32pedro";
const char *mqtt_pass = "esp3223";
const char* topico1 = "Led1";
//const char* topico2 = "Led2";
const char* topico3 = "Temperatura";
const char* topico4 = "Humedad";
PubSubClient client(espClient);

void setup_wifi(){  // Funcion para conectarnos al WIFI --------
  delay(10);
  // Nos conectamos a nuestra red Wifi
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Conectado a red WiFi!");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}
//---------------- Funcion para restablecer coneccion MQTT -----
void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión Mqtt...");
    // Creamos un cliente ID
    String clientId = "esp32pedro";
    
    // Intentamos conectar
    if (client.connect(clientId.c_str(),mqtt_user,mqtt_pass)) {
      Serial.println("Conectado!");
      // Nos suscribimos a los topicos
       client.subscribe("Led1");
      // client.subscribe("Led2");
       client.subscribe("Temperatura");
       client.subscribe("Humedad");
       
    } else {
      Serial.print("falló :( con error -> ");
      Serial.print(client.state());
      Serial.println(" Intentamos de nuevo en 5 segundos");

      delay(5000);
    }
  }
}
//-------------- Funcion callback para mqtt----------------
void callback(char* topic, byte* payload, unsigned int length){
  String incoming = "";
  for (int i = 0; i < length; i++) {
    incoming += (char)payload[i];
  }
  incoming.trim();
  int orden = String(incoming).toInt();
  
  switch (orden) {    // 
      
       case 0 : Serial.println("Apagar Led1"); 
                digitalWrite (pinLed, LOW);
       break;         
       case 1 : Serial.println("Encender Led1"); 
                digitalWrite (pinLed, HIGH);         
       break;
  }

}

void setup() {
  setup_wifi(); // Inicializamos WIFI
  delay(3000);
  Serial.begin(9600);
  sensorDHT.setup(dhtPin, DHTesp::DHT22);
  pinMode(pinPulsador, INPUT_PULLUP);
  pinMode(pinLed, OUTPUT);
  client.setServer(mqtt_server, mqtt_port); // Conectamos a broker
  client.setCallback(callback); // Dejamos en escucha las subscripciones
}

void loop() {
  if (!client.connected()) { // si se corta conexion mqtt
       reconnect();
    }
  client.loop();
  TempAndHumidity data= sensorDHT.getTempAndHumidity();
  Serial.println("TEMPERATURA: "+ String(data.temperature));
  Serial.println("HUMEDAD    : "+ String(data.humidity));
  char buffert1[10];
  char buffert2[10];
  dtostrf(data.temperature,0, 2, buffert1); // Convierte a *char para poder enviar por mqtt
  dtostrf(data.humidity,0, 2, buffert2);
  client.publish(topico3,buffert1);
  client.publish(topico4,buffert2);
    
  valuPote= analogRead(pinPote);
  valuPote = map(valuPote, 0, 4096, 0,255);
  analogWrite(pinLedDimmer,valuPote);
  Serial.println("VALOR POTE: "+ String(valuPote));
  delay(500);

  bool valor=digitalRead(pinPulsador);//leo el estado del pulsador
  Serial.println(valor);
  if (valor!=estado){
     delay(10);
     bool valor2=digitalRead(pinPulsador);//leo nuevamente el pulsador para saber si se solto
     if (digitalRead(pinPulsador)==LOW){
        digitalWrite(pinLed, !digitalRead(pinLed));
    
      }
  }
  estado=valor;
 // if (valor = true) {
 //    client.publish(topico1,"1");  
 // } else { client.publish(topico1,"0"); 
 //   }             
 
}
