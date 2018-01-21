#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "Karla_kliknij_tu";
const char* password = "sznycel123";

bool isInSetupMode = false;
String deviceType = "ALARM_SENSORS";
bool isDeviceAssigned = false;

String method;
String endpoint;
String body;
WiFiServer server(10000);
String serverURL = "http://192.168.43.197:4000/api/externalDevice";
const int timeBetweenMeasurements = 1*60*1000000;

int previousMillisForConfig = 0;
int lightIndicatorState = LOW;

const int turnDevicesOnPin = 13;
const int BPin = 14;
const int APin = 12;
const int measurementPin = A0;
const int indicatorLedPin = 5;

int tresholdLevel = 500;

String handleRoot(){
  
  return prepareResponse(200, "text/plain",String(isDeviceAssigned));
}

class SimpleJsonBuilder {
  private: String content;

  public: SimpleJsonBuilder(){content = "{";};
  public: void addString(String key, String value){
    content += "\"" + key + "\":\"" + value + "\",";
  }
  public: void addInteger(String key, int value){
    content += "\"" + key + "\":" + String(value) + ",";
  }
  public: void addFloat(String key, float value){
    content += "\"" + key + "\":" + String(value) + ",";
  }
  public: String build(){
    return content.substring(0,content.length()-1) + "}";
  }
};

String highlightDevice(){ 
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(body);
  
  if(!root.success()){
    return prepareResponse(400, "text/plain", body);
  }

  isInSetupMode = root["highlight"];
  lightIndicatorState = LOW;
  digitalWrite(indicatorLedPin,LOW);
  isDeviceAssigned = root["isAssigned"];
  return prepareResponse(200, "text/plain", (isInSetupMode) ? "Urzadzenie jest w trybie konfiguracji" : "Zakonczono tryb konfiguracji");
}

void setupMode(){ 
  
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillisForConfig >= 2000) {
    previousMillisForConfig = currentMillis;
    
    if (lightIndicatorState == LOW) {
      lightIndicatorState = HIGH;
    } else {
      lightIndicatorState = LOW;
    }

    digitalWrite(indicatorLedPin, lightIndicatorState);
  }
}

void setup() {
  
  pinMode(turnDevicesOnPin, OUTPUT);
  pinMode(APin, OUTPUT);
  pinMode(BPin, OUTPUT);
  pinMode(indicatorLedPin, OUTPUT);
  pinMode(measurementPin, INPUT);

  digitalWrite(turnDevicesOnPin, LOW);

  WiFi.begin(ssid, password);
  Serial.begin(115600);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println(WiFi.localIP());
  registerToServer();
  server.begin();
}

void loop() {
  
  if(isDeviceAssigned) {
    
    sendMeasurements();
    ESP.deepSleep(timeBetweenMeasurements);
  
  } else {
    if(isInSetupMode){
      setupMode();
    }
    handleClient();
  }
  
}

void handleClient(){
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  
  String request;
  while(!client.available()){
    delay(1);
  }
  
  while(client.available()){
    request+= (char)client.read();
  }
  client.flush();
  Serial.println(request);
  parseRequest(request);
  
  if(endpoint.equals("/") && method.equals("GET")) { 
    client.print(handleRoot());
  } else 
  if (endpoint.equals("/highlightDevice") && method.equals("POST")) {
    client.print(highlightDevice());
  } else {
    client.print(prepareResponse(404,"text/plain", "Nie istnieje taka strona"));
  }
}

void registerToServer(){

   //Send basic information for system to start
   HTTPClient http;
   http.begin(serverURL + "/register"); //HTTP
   http.addHeader("Content-Type", "application/json");
   SimpleJsonBuilder *jsonBuilder = new SimpleJsonBuilder();
   jsonBuilder->addString("ip", getIPAsString());
   jsonBuilder->addString("mac", WiFi.macAddress());
   jsonBuilder->addString("deviceType",deviceType);
   int httpCode = http.POST(jsonBuilder->build());
   if(httpCode > 0) {
      if(httpCode == HTTP_CODE_OK) {
        String body = http.getString();
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(body);
    
        if(!root.success()){
          isDeviceAssigned = false;
        } else {
          isDeviceAssigned = root["isAssigned"];
        }
      }
   } else {
          
   }
   http.end();
   delete jsonBuilder;
}

void sendMeasurements(){
   //Switch sensors on and give time for sensors to heat up
  digitalWrite(turnDevicesOnPin, HIGH);
  delay(5000);

  //Choose monoxide sensor
  digitalWrite(APin, LOW);
  digitalWrite(BPin,LOW);
  delay(10);
  
  bool isDangerOfMonoxide = analogRead(measurementPin) >tresholdLevel;
  //Choose dioxide sensor
  digitalWrite(APin, HIGH);
  digitalWrite(BPin,LOW);
  delay(10);
  
  bool isDangerOfDioxide = analogRead(measurementPin) > tresholdLevel;

  //Switch sensors off for energy savings
  digitalWrite(turnDevicesOnPin, LOW);
  
  SimpleJsonBuilder *jsonBuilder = new SimpleJsonBuilder();
  jsonBuilder->addInteger("monoxide",isDangerOfMonoxide);
  jsonBuilder->addInteger("dioxide", isDangerOfDioxide);
  String json = jsonBuilder->build();
  
  //Send measurements to server
  HTTPClient http;
   http.begin(serverURL + "/alarm/" + WiFi.macAddress()); //HTTP
   http.addHeader("Content-Type", "application/json");
   int httpCode = http.POST(json);

   // Retrive response if u r interested
   if(httpCode > 0) {
      if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      
      }
   } else {
          
   }
   http.end();
   delete jsonBuilder;
}

String getIPAsString(){
  IPAddress myIP = WiFi.localIP();
  return String(myIP[0]) + "." + String(myIP[1]) + "." + String(myIP[2]) + "." + String(myIP[3]);
}

void parseRequest(String request) {
  method = "";
  int i = 0;

  for(i; request.charAt(i) != ' '; i++) {
    method+= request.charAt(i);
  }

  endpoint = "";
  //Read endpoint
  for(i++ ; request.charAt(i) != ' '; i++) {
    endpoint+= request.charAt(i);
  }

  body = "";
  String bodyStart = "\r\n\r\n";
  int bodyIndex = request.indexOf(bodyStart);
  if(bodyIndex != -1) {
    for(i = bodyIndex + bodyStart.length(); i < request.length(); i++ ) {
      body += request.charAt(i);
    }
  }
}

String prepareResponse(int code, String contentType, String msg) {
  String s = "HTTP/1.1 " + String(code) + " " + codeToText(code)+ "\r\n";
  s+= "Content-Type: "+ contentType + "\r\n";
  s+= "Content-Length: " + String(msg.length()) + "\r\n";
  s+= "Connection: Close\r\n\r\n";
  s+= msg;
  return s;
}

String codeToText(int code) {
  switch (code) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Time-out";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Request Entity Too Large";
    case 414: return "Request-URI Too Large";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested range not satisfiable";
    case 417: return "Expectation Failed";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Time-out";
    case 505: return "HTTP Version not supported";
    default:  return "";
  }
}


