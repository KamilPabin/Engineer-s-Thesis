#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "Karla_kliknij_tu";
const char* password = "sznycel123";

const int redPin = 2;
const int bluePin = 0;
const int greenPin = 1;

struct Color {
  int red;
  int green;
  int blue;
};

bool isOn = false;
bool isInSetupMode = false;
String deviceType = "LIGHTS";

Color color;

String method;
String endpoint;
String body;
WiFiServer server(10000);
String serverURL = "http://192.168.43.197:4000/api/externalDevice";

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

String handleRoot(){
  String response = "{\"isPoweredOn\":" + String(isOn) + "}";
  return prepareResponse(200, "application/json", response);
}

String changeBulbColor(){
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(body);
  
  if(!root.success()){
    return prepareResponse(400, "text/plain", body);
  }
    
  color.red = root["red"];
  color.green = root["green"];
  color.blue = root["blue"];

  if(isOn) {
    analogWrite(redPin, color.red);
    analogWrite(bluePin, color.blue);
    analogWrite(greenPin, color.green);
  }

  return prepareResponse(200, "text/plain", "Zmieniono kolory zarowki");
}

String changeBulbPower(){
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(body);
  
  if(!root.success()){
    return prepareResponse(400, "text/plain", body);
  }

  isOn = root["power"];  
  if(isOn) {
    analogWrite(redPin, color.red);
    analogWrite(bluePin, color.blue);
    analogWrite(greenPin, color.green);
  } else {
    analogWrite(redPin, 0);
    analogWrite(bluePin, 0);
    analogWrite(greenPin, 0);
  }
  
  return prepareResponse(200, "text/plain", (isOn) ? "wlaczono zarowke" : "wylaczono zarowke");
}

String highlightDevice(){ 
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(body);
  
  if(!root.success()){
    return prepareResponse(400, "text/plain", body);
  }

  isInSetupMode = root["highlight"];
  return prepareResponse(200, "text/plain", (isInSetupMode) ? "Urzadzenie jest w trybie konfiguracji" : "Zakonczono tryb konfiguracji");
}

void setupMode(){
  analogWrite(redPin, 0); 
  analogWrite(greenPin, 0); 
  analogWrite(bluePin, 0); 

  analogWrite(redPin,255);
  delay(1000);
  analogWrite(redPin, 0);
  delay(3000);
}

void setup() {
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    
  }
  registerToServer();
  server.begin();
}

void loop() {
  if(isInSetupMode){
    setupMode();
  }

  handleClient();
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
  if(endpoint.equals("/changeBulbColor") && method.equals("POST")) { 
    client.print(changeBulbColor());
  } else
  if(endpoint.equals("/changeBulbPower") && method.equals("POST")) { 
    client.print(changeBulbPower());
  } else 
  if(endpoint.equals("/highlightDevice") && method.equals("POST")) { 
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
