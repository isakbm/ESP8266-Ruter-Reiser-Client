#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <time.h>

// =============================================================================

bool contains(String line, String pattern){
  auto c = line.begin();
  while ( c != line.end() ) {
    bool match = true;
    for ( char & p : pattern ) {
      if ( p != *c ) { match = false; break; }
      else c++;
    }
    if ( match ) return true;
    else c++;
  }
  return false;
}

void get_hh_mm_ss(const char * line, char * out, int hour_offset = 0) {
  char * o = out;
  const char * c = line + 2;
  while ( *c != '\n' and *c != 0 ){
    if ( isdigit(*(c-2)) and isdigit(*(c-1)) and *c == ':' ){ 
      c -= 2; int i = 0;
      while ( *c != 0 and i < 8 ){
        (*o++) = (*c++); i++;
      }
      *o = 0;
      break;
    }
    else c++;
  }

  // Juggling around with the strings to offset the hours by hour_offset
  // dont forget the termination symbols here
  char hh_cstr[8];
  char mm_ss_cstr[8];
  strncpy(hh_cstr, out, 2); hh_cstr[2] = 0;
  strncpy(mm_ss_cstr, out + 2, 6); mm_ss_cstr[6] = 0;

  int hours = (atoi(hh_cstr) + hour_offset) % 24 ; // GMT + 2 
  
  char temp[16];
  sprintf(temp, "%02d%s", hours, mm_ss_cstr);
  strncpy(out, temp, 8); out[8] = 0;
  
}

void print_departures(const char * line){
  
  Serial.println("Departures:");
  Serial.println("");
  //Serial.println(line); 
  
  String pattern = "ExpectedDepartureTime";
  
  const char * c = line;
  while ( *c != 0 ) {
    if ( *c == '(' ) {
      c++;
      String travel_dir = "";
      while ( *c != 0 && *c != ')' ) {
        travel_dir += *c;
        c++;
      }
      Serial.println(travel_dir);
    }
    bool match = true;
    for ( char & p : pattern ) {
      if ( p != *c ) { match = false; break; }
      else c++;
    }
    if ( match ) {
      char temp[16];
      get_hh_mm_ss(c, temp);
      Serial.println(temp);
    }
    c++;
  }
  Serial.println("");
}


// =============================================================================


const int ms_delay_between_requests = 60*1000; // 2 minute


const char* ssid = "isak_ASUS_2G"; //your WiFi Name
const char* password = "alispale";  //Your Wifi Password

const char* host = "reisapi.ruter.no";
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
// You can find out what the fingerprint is from here: https://www.grc.com/fingerprints.htm
const char* fingerprint = "55 80 FF 07 52 F1 64 1C 6A B6 08 F8 40 A3 F1 ED BF 2B 6B 5F"; 

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());  
}

void loop() {

  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  if (client.verify(fingerprint, host)) {
    //Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  String url = "/StopVisit/GetDepartures/3012230/?json=true";

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: BuildFailureDetectorESP8266\r\n" +
                 "Connection: close\r\n\r\n");

  if (!client.connected()) Serial.println("client has been disconnected!");
  
  char temp[16];
  while (client.connected()) {
    
    String line = client.readStringUntil('\n');

    if ( contains(line, "Date") ) {
      get_hh_mm_ss(line.c_str(), temp, 2);
    }

    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }

  Serial.println("");
  Serial.print("time: ");


  Serial.println(temp);

  String line = client.readStringUntil('\n');
  print_departures(line.c_str());
  //Serial.println(line.c_str());

  delay(ms_delay_between_requests);
  
}
