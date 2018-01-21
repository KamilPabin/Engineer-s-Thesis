#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "Karla_kliknij_tu";
const char* password = "sznycel123";

bool isInSetupMode = false;
String deviceType = "BLINDS";

const int motor[] = {4, 5};
const int ledIndicator = 14;
const int krancowkaGorna = 12;
const int krancowkaDolna = 13;
String direction = "STOP";

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
  SimpleJsonBuilder *jsonBuilder = new SimpleJsonBuilder();
  jsonBuilder->addString("ip",getIPAsString());
  jsonBuilder->addString("mac", WiFi.macAddress());
  jsonBuilder->addString("deviceType",deviceType);
  String response = prepareResponse(200,"application/json",jsonBuilder->build());
  delete jsonBuilder;
  return response;
}

String blindMovement(){
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(body);
  
  if(!root.success()){
    return prepareResponse(400, "text/plain", body);
  }
  String dir = root["direction"];
  if( !dir.equals("UP") && !dir.equals("DOWN") && !dir.equals("STOP")) {
    return prepareResponse(400, "text/plain", "dozwolone wartosci to: UP, DOWN, STOP");
  }
  direction = dir;

  return prepareResponse(200, "text/plain", "Rolety pracuja");
}

void moveBlinds(){

  if(direction.equals("UP")) {
    driveForward();
  } else 
  if(direction.equals("DOWN")) {
    driveBackward();
  } else {
    stopMotor();
    return;
  }

  if(digitalRead(krancowkaGorna)) {
    direction = "STOP";
    stopMotor();
    sendBlindState("OPENED");
  }

  if(digitalRead(krancowkaDolna)) {
    direction = "STOP";
    stopMotor();
    sendBlindState("CLOSED");
  }
  
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
  digitalWrite(ledIndicator, HIGH);
  delay(1000);
  digitalWrite(ledIndicator,LOW);
  delay(3000);
}

void setup() {
 
  pinMode(krancowkaGorna, INPUT);
  pinMode(krancowkaDolna, INPUT);
  pinMode(ledIndicator, OUTPUT);
  pinMode(motor[0], OUTPUT);
  pinMode(motor[1], OUTPUT);
  stopMotor();
  
  Serial.begin(115600);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(WiFi.localIP());
  
  registerToServer();
  server.begin();
}

void loop() {
  
  if(isInSetupMode){
    setupMode();
  }
  moveBlinds();
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

  parseRequest(request);
  
  if(endpoint.equals("/") && method.equals("GET")) { 
    client.print(handleRoot());
  } else
  if(endpoint.equals("/blindMovement") && method.equals("POST")) { 
    client.print(blindMovement());
  } else 
  if(endpoint.equals("/highlightDevice") && method.equals("POST")) { 
    client.print(highlightDevice());
  } else {
    client.print(prepareResponse(404,"text/plain", "Nie istnieje taka strona"));
  } 
}

void registerToServer(){
   HTTPClient http;
   http.begin(serverURL+"/register"); //HTTP
   http.addHeader("Content-Type", "application/json");
   SimpleJsonBuilder *jsonBuilder = new SimpleJsonBuilder();
   jsonBuilder->addString("ip",getIPAsString());
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

void sendBlindState(String state){
   HTTPClient http;
   http.begin( serverURL + "/blindState/" + WiFi.macAddress()); //HTTP
   http.addHeader("Content-Type", "application/json");
   String json = "{\"state\":\"" + state + "\"}";
   int httpCode = http.POST(json);
   if(httpCode > 0) {
      if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();         
      }
   } else {
          
   }
   http.end();
}

void stopMotor(){
  digitalWrite(motor[0], LOW); 
  digitalWrite(motor[1], LOW); 
  
  delay(25);
}

void driveForward(){
  digitalWrite(motor[0], HIGH); 
  digitalWrite(motor[1], LOW); 

}

void driveBackward(){
  digitalWrite(motor[0], LOW); 
  digitalWrite(motor[1], HIGH); 
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


