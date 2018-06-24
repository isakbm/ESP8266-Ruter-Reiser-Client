
//https://www.c-sharpcorner.com/article/blinking-led-by-esp-12e-nodemcu-v3-module-using-arduinoide/

// https://github.com/esp8266/Arduino/

#include <ESP8266WiFi.h>
#include <ctype.h>
#include <time.h>

// Liquid crystal library
#include <LiquidCrystal.h>

// ESP8266 pin mapping to node MCU breakout
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2 // also the LED builtin
#define D5 14
#define D6 12
#define D7 13
#define D8 15

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

void parse_response(WiFiClientSecure * cli, char * dep_time, char * dep_dest){

  char * time_key = "ExpectedDepartureTime";
  char * dest_key = "DestinationName";

  char temp_cstr[128];
  int found = 0;
  
  while( cli->connected() and cli->available() ){

    if (found == 2) return;
    cli->readStringUntil('"');
    String key = cli->readStringUntil('"');

    if ( strcmp(key.c_str(), time_key ) == 0 ){  // TIME
      
      cli->readStringUntil('"');
      String value = cli->readStringUntil('"');
      
      strncpy(temp_cstr, value.c_str(), value.length());
      temp_cstr[value.length()] = 0;
      get_hh_mm_ss(temp_cstr, dep_time);
      found ++;
    }
    else if ( strcmp(key.c_str(), dest_key ) == 0){  // DEST
      
      cli->readStringUntil('"');
      String value = cli->readStringUntil('"');
      
      strncpy(dep_dest, value.c_str(), value.length());
      dep_dest[value.length()] = 0;
      found ++;
    }
  }
}

void time_diff(char * A_t, char * B_t, char * diff_t){
  
  char temp[2];
  
  strncpy(temp, A_t + 0, 2); int A_h = atoi(temp);
  strncpy(temp, B_t + 0, 2); int B_h = atoi(temp);

  strncpy(temp, A_t + 3, 2); int A_m = atoi(temp);
  strncpy(temp, B_t + 3, 2); int B_m = atoi(temp);

  strncpy(temp, A_t + 6, 2); int A_s = atoi(temp);
  strncpy(temp, B_t + 6, 2); int B_s = atoi(temp);

  int diff_ti = ( 60*(60*B_h + B_m) + B_s ) - ( 60*(60*A_h + A_m) + A_s );

  if (diff_ti < 0){
    sprintf(diff_t, "--:--:--");
  }
  else {
    int diff_s = diff_ti % 60;
    int diff_m = (diff_ti - diff_s)/60 % 60;
    int diff_h = ((diff_ti - diff_s)/60 - diff_m)/60;
    
    sprintf(diff_t, "%02d:%02d:%02d", diff_h, diff_m, diff_s);
  }
  
}

void time_add_seconds(char * A_t, int sec){

  char temp[2];
    
  strncpy(temp, A_t + 0, 2); int A_h = atoi(temp);
  strncpy(temp, A_t + 3, 2); int A_m = atoi(temp);
  strncpy(temp, A_t + 6, 2); int A_s = atoi(temp);
  
  int sum = 60*(60*A_h + A_m) + A_s + sec;

  if (sum < 0){
    sprintf(A_t, "00:00:00");
  }
  else {
    A_s = sum % 60;
    A_m = (sum - A_s)/60 % 60;
    A_h = ((sum - A_s)/60 - A_m)/60;
    sprintf(A_t, "%02d:%02d:%02d", A_h, A_m, A_s);
  }
}

void truncate_time_mm_ss(char * A_t){
  char temp[16]; strcpy(temp, A_t +3);
  strcpy(A_t, temp);
}

// =============================================================================

const int ms_delay_between_requests = 30*1000; // 2 minute
const int update_period_seconds = 30;

const char* ssid = "isak_ASUS_2G"; //your WiFi Name
const char* password = "alispale";  //Your Wifi Password

const char* host = "reisapi.ruter.no";
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
// You can find out what the fingerprint is from here: https://www.grc.com/fingerprints.htm
const char* fingerprint = "55 80 FF 07 52 F1 64 1C 6A B6 08 F8 40 A3 F1 ED BF 2B 6B 5F"; 


// be warned pin D8 on the nodeMCU is right next to rx, tx pins
//so it seems like it doesn't work well to use it
//                4    6  11  12  13  14
LiquidCrystal lcd(D7, D6, D3, D2, D1, D0);  

int num_stations = 4;
int station_id = 0;
int button_state = 0;

void my_lcd_print(const char * first_line, const char * second_line = NULL){
  char msg[32];
  sprintf(msg, "%-16s", first_line );
  lcd.setCursor(0, 0);
  lcd.print(msg);
  if (second_line != NULL) {
    sprintf(msg, "%-16s", second_line );
  }
  else {
    sprintf(msg, "%-16s", " ");
  }
  lcd.setCursor(0, 1);
  lcd.print(msg);
}

void setup() {
  lcd.begin(16,2);
  my_lcd_print("hello, world!");
  pinMode(D8, INPUT_PULLUP);
  Serial.begin(115200);
  //Serial.println();
  //Serial.print("connecting to ");
  //Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }
  //Serial.println("");
  //Serial.println("WiFi connected");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());  
}

uint16_t loop_ctr = 0;

void loop() {

  // To signify that the departure time is being updated
  lcd.setCursor(0, 0);
  lcd.print("**:**");
  lcd.setCursor(0, 1);
  lcd.print("**:**");
  
  loop_ctr++;
  //Serial.printf("=====================[%03d]===========================\n", loop_ctr);
  //Serial.printf("WiFi.status() = %d\n", WiFi.status());
  
  if ( WiFi.status() != WL_CONNECTED){
    //Serial.println("WiFi is not connected?");
  }
  
  //Serial.println("checking connection:");

  WiFiClientSecure client;
  
  if (!client.connect(host, httpsPort)) {
    
    //Serial.println("connection failed");
    return;
  }
  else {
    //Serial.println("connection is good");
  }

  if (client.verify(fingerprint, host)) {
    //Serial.println("certificate matches");
  } else {
    //Serial.println("certificate doesn't match");
  }

  String url = "/StopVisit/GetDepartures/3012230/?json=true";

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: BuildFailureDetectorESP8266\r\n" +
                 "Connection: close\r\n\r\n");

  //Serial.println("sent request to client");

  if (!client.connected()){
    //Serial.println("client has been disconnected!");
  }
  
  char time_ref[16];
  uint16_t req_at_time = 0;
  while (client.connected()) {
    
    String line = client.readStringUntil('\n');

    if ( contains(line, "Date") ) {
      get_hh_mm_ss(line.c_str(), time_ref, 2);
    }

    if (line == "\r") {
      //Serial.println("headers received");
      req_at_time = millis()/1000;
      break;
    }
  }
  
  //Serial.printf("\ntime_ref: %s\n\n", time_ref);

  #define num_departures 2 
  
  char dep_times [2][32];
  char dep_dests [2][32];
  char dif_times [2][32];

  for (int i = 0; i < num_departures; i++){    
    parse_response(&client, dep_times[i], dep_dests[i]);
    time_diff(time_ref, dep_times[i], dif_times[i]);
    //Serial.println(dep_times[i]);
  }

  for (int i = 0; i < update_period_seconds; i++){
    for (int j = 0; j < num_departures; j++){
      char temp[32]; strcpy(temp, dif_times[j]);
      time_add_seconds(temp, req_at_time - millis()/1000);
      truncate_time_mm_ss(temp);

      char msg[32];
      sprintf(msg, "%s %-16s", temp, dep_dests[j]);
      
      //Serial.println("----------------------");
      //Serial.println(msg);
  
      lcd.setCursor(0, j);
      lcd.print(msg);
    }
    // Loop for pulling the D8 pin, it's state changes the station selection.
    for (int chk = 0; chk < 10; chk++){
      int D8_read = digitalRead(D8);
      bool change_selection = (button_state == LOW) and (D8_read == HIGH);
      if (change_selection){
        station_id++; station_id %= num_stations;
        char first_line[32]; sprintf(first_line, "Station ID %d", station_id );
        my_lcd_print( first_line, "Selected!");
        delay(2000);
      }
      button_state = D8_read;
      delay(100);
    }
  }

  //delay(ms_delay_between_requests);
  
}
