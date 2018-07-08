#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>

#include "Config.h"
#include "WebClient.h"
#include "MPU.h"


// ESP.Restart won't work until reset by button or by powering off for first time after flashing

//#define WIFI_DISABLED

WebClient web;

WiFiClientSecure *wClient = nullptr;

ESP8266WebServer server(80);

const char html[] PROGMEM = 
"<form method='POST' action='settings'>"
"  Your SSID:<br>"
"  <input type='text' name='ssid' value=''><br>"
"  Your Password:<br>"
"  <input type='text' name='password' value=''><br>"
"  Token:<br>"
"  <input type='text' name='token' value=''><br>"
"  Host:<br>"
"  <input type='text' name='host' value=''><br>"
"  Port:<br>"
"  <input type='number' name='port' value=''><br>"
"  Fingerprint:<br>"
"  <input type='text' name='fingerprint' value=''><br>"
"  GPS serial rate:<br>"
"  <select name='serial_rate'>"
"   <option value='115200'>115200</option>"
"   <option value='9600'>9600</option>"
"  </select><br>"
"  GPS bridge mode:<br>"
"  <input type='checkbox' name='bridge' value='1'><br><br>"
"  <input type='submit' value='Submit'>"
"</form>"
"<a href='/calibrate'>mpu calibrate</a><br>"
"<a href='/restart'>restart</a>";

const char html_gps_bridge[] PROGMEM = 
"<form method='POST' action='settings'>"
"  GPS serial rate:<br>"
"  <select name='serial_rate'>"
"   <option value='115200'>115200</option>"
"   <option value='9600'>9600</option>"
"  </select><br>"
"  <input type='submit' value='Submit'>"
"</form>"
"<a href='/off_bridge'>off bridge</a>";

const char html_wait_start[] PROGMEM = "<html><head><script language='JavaScript' type='text/javascript'>setTimeout(\"location.href = '/wait'\",1000);</script></head><body>Wait please ";
const char html_wait_end[] PROGMEM = "s</body></html>";
const char html_wait_done[] PROGMEM = "<p>done, reboot device please</p>";

const char html_off_bridge[] PROGMEM = "<p>Turning of bridge and restarting...</p><br><a href='/'>home</a>";
const char html_restart[] PROGMEM = "<p>Restarting...</p><br><a href='/'>home</a>";
const char html_type[] PROGMEM = "text/html";

void ICACHE_FLASH_ATTR handleRoot() 
{
  server.send(200, "text/html", cfg.bridgeMode ? html_gps_bridge : html);
}

unsigned long calibrateStarted = 0;
bool calibrateJustStarted = false;

void ICACHE_FLASH_ATTR handleWait() 
{
  String s;

  if (calibrateStarted > 0)
  {
    s = html_wait_start;

    unsigned long t = micros();
    s += (t - calibrateStarted) / 1000000;
    s += html_wait_end;
  }
  else
  {
    s = html_wait_done;
  }
  
  server.send(200, html_type, s);
}

void ICACHE_FLASH_ATTR handleCalibrate() 
{
  if (calibrateStarted == 0)
  {
    calibrateStarted = micros();
    calibrateJustStarted = true;
  }
  
  handleWait();
}

void ICACHE_FLASH_ATTR handleOffBridge()
{
  server.send(200, html_type, html_off_bridge);
  
  cfg.bridgeMode = 0;
  cfg.Write();
  delay(500);
  ESP.restart();
}

void ICACHE_FLASH_ATTR handleRestart()
{
  server.send(200, html_type, html_restart);
  delay(500);
  ESP.restart();
}

// handle POST request and save settings to flash
void ICACHE_FLASH_ATTR handleSettings() 
{
  Serial.println(F("settings"));

  String r = F("<h1>");
   
  Serial.println(F("Write Settings"));

  String ssid = server.arg("ssid"); 
  String password = server.arg("password"); 
  String token = server.arg("token"); 
  String host = server.arg("host"); 
  String port_s = server.arg("port"); 
  String fingerprint = server.arg("fingerprint"); 

  String bridge_s = server.arg("bridge");
  String serial_rate_s = server.arg("serial_rate");
  

  r += F("Settings applied:<br>");

  if (ssid.length() > 0 && ssid.length() < sizeof(cfg.ssid)) {
    Serial.print(F("new ssid ")); Serial.println(ssid.c_str());
    r += F("new ssid ");
    r += ssid;
    r += F("<br>");
    strcpy(cfg.ssid, ssid.c_str());
  }    
  if (password.length() > 0 && password.length() < sizeof(cfg.pass)) {
    Serial.print(F("new password ")); Serial.println(password.c_str());
    r += F("new password ");
    r += password;
    r += F("<br>");
    strcpy(cfg.pass, password.c_str());
  }
  if (token.length() > 0 && token.length() < sizeof(cfg.token)) {
    Serial.print(F("new token ")); Serial.println(token.c_str());
    r += F("new token ");
    r += token;
    r += F("<br>");
    strcpy(cfg.token, token.c_str());
  }
  if (host.length() > 0 && host.length() < sizeof(cfg.host)) {
    Serial.print(F("new host ")); Serial.println(host.c_str());
    r += F("new host ");
    r += host;
    r += "<br>";
    strcpy(cfg.host, host.c_str());
  }
  if (fingerprint.length() > 0 && fingerprint.length() < sizeof(cfg.fingerprint)) {
    Serial.print(F("new fingerprint ")); Serial.println(fingerprint.c_str());
    r += F("new fingerprint ");
    r += fingerprint;
    r += "<br>";
    strcpy(cfg.fingerprint, fingerprint.c_str());
  }
  if (port_s.length() > 0) {
    int port = atoi(port_s.c_str());
    
    Serial.print(F("new port ")); Serial.println(port);
    r += F("new port ");
    r += port;
    r += "<br>";
    cfg.port = port;
  }

  if (serial_rate_s.length() > 0) {
    int serial_rate = atoi(serial_rate_s.c_str());
    
    Serial.print(F("new serial_rate ")); Serial.println(serial_rate);
    r += F("new serial_rate ");
    r += serial_rate;
    
    if (serial_rate == 115200 || serial_rate == 9600)
    {
      cfg.serialRate = serial_rate;
    }
    else
    {
      r += F(" invalid, not set ");
    }
    
    r += "<br>";
  }

  int bridgeBefore = cfg.bridgeMode;
  
  if (bridge_s.length() > 0) {
    int bridge = atoi(bridge_s.c_str());
    
    Serial.print(F("new bridge ")); Serial.println(bridge);
    r += F("new bride mode ");
    r += bridge;
    r += "<br>";
    cfg.bridgeMode = bridge;
  }

  if (!cfg.Write()) {
    r = F("<h1>Failed to write settings");
  }

  // bridgeMode should be applied after restart
  cfg.bridgeMode = bridgeBefore;

  r += F("</h1><br><a href='/restart'>restart</a>");
  server.send(200, html_type, r.c_str());  
}


extern bool check_backend_done;

void WebClient::update()
{
  // start server if we failed to connect in 60s after startup 
#ifndef WIFI_DISABLED  
  if (!check_backend_done && !enableAP && micros() - initTime > 60 * 1000 * 1000 && WiFi.status() != WL_CONNECTED) {
    enableAP = true;
    WiFi.disconnect(true);
    Serial.print(F("Setting soft-AP ... "));
    Serial.println(WiFi.softAP("racestats", "") ? F("Ready") : F("Failed!"));

    server.on("/", handleRoot);
    server.on("/calibrate", handleCalibrate);
    server.on("/restart", handleRestart);
    server.on("/wait", handleWait);
    server.on("/settings", HTTP_POST, handleSettings);
  
    server.begin();
    Serial.println(F("Start server at 192.168.4.1"));
  }

  // enable AP in gps bridge mode
  if (cfg.bridgeMode && !enableAP)
  {
    enableAP = true;
    WiFi.softAP("racestats", "");
    server.on("/", handleRoot);
    server.on("/settings", HTTP_POST, handleSettings);
    server.on("/restart", handleRestart);
    server.on("/off_bridge", handleOffBridge);
  
    server.begin();
  }
#endif  

  if (enableAP) {
    server.handleClient();
  }

  if (calibrateJustStarted) {
    calibrateJustStarted = false;
    Serial.println(F("Start calibrate"));
    mpu_calibrate();
    Serial.println(F("Finished calibrate"));
    cfg.Write();
    calibrateStarted = 0;
    cfg.Read();
  }
}

WebClient::WebClient()
{
  initTime = micros();
  last_post_code = 0;
  enableAP = false;   
}

void WebClient::init()
{     
  Serial.println();

#ifndef WIFI_DISABLED
  if (strlen(cfg.ssid) > 0) {
    Serial.print(F("connecting to "));
    Serial.println(cfg.ssid);
    WiFi.begin(cfg.ssid, cfg.pass);
    Serial.print(F("..."));
  }
  else
  {
    Serial.print(F("invalid ssid"));
  }
#endif
}


void WebClient::sendData(void *data, int len)
{
  sendData(data, len, 0, 0, 0, 0);
}

extern void gpsSendData(uint8_t *data, int size);

void WebClient::requestAGps()
{
  /*
  if (enableAP || WiFi.status() != WL_CONNECTED) {
    return ;
  }

  HTTPClient http;
  
  Serial.print("[HTTP] begin...\n");
  http.begin("http://online-live1.services.u-blox.com/GetOnlineData.ashx?token=" UBLOX_TOKEN ";gnss=gps;datatype=eph,alm,aux;format=aid"); //HTTP
  
  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  const int max_data_len = 6000;
  
  // httpCode will be negative on error
  if(httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
  
      // file found at server
      if(httpCode == HTTP_CODE_OK) {
         // get lenght of document (is -1 when Server sends no Content-Length header)
         int len = http.getSize();

         Serial.printf("[HTTP] size %d\n", len);
 
         if (len < max_data_len && len > 0) {
            // create buffer for read
           uint8_t *buff = new uint8_t[len];
           int buff_len = len;
           
           uint8_t *buf_cur = buff;
    
           // get tcp stream
           WiFiClient * stream = http.getStreamPtr();
    
           // read all data from server
           while (http.connected() && (len > 0 || len == -1)) {
               // get available data size
               size_t size = stream->available();
    
               if (size) {
                   int c = stream->readBytes(buf_cur, size);
    
                   // write it to Serial
                   Serial.printf("[HTTP] size block %d\n", c);
                   //Serial.write(buff, c);
    
                   if (len > 0) {
                      buf_cur += c;
                      len -= c;
                   }
               }
               delay(1);
           }

          Serial.print("set gps data\n");
          gpsSendData(buff, buff_len);
          delete [] buff;
           Serial.println();
           Serial.print("[HTTP] connection closed or file end.\n");
         } else {
            Serial.printf("[HTTP] bad size %d\n", len);         
         }
   


      }
  } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  */
}

bool WebClient::sendData(void *d1, int l1, void *d2, int l2, void *d3, int l3)
{
  last_post_code = 1;
  if (enableAP || WiFi.status() != WL_CONNECTED) {
    last_post_code = 2;
    return false;
  }
  if (wClient == nullptr) {
    wClient = new WiFiClientSecure();
  }

  Serial.print(F("connecting to "));
  Serial.println(cfg.host);
  if (!wClient->connect(cfg.host, cfg.port)) {
    last_post_code = 3;
    Serial.println(F("connection failed"));
    return false;
  }

  if (wClient->verify(cfg.fingerprint, cfg.host)) {
    Serial.println(F("certificate matches"));
  } else {
    last_post_code = 4;
    Serial.println("certificate doesn't match");
    wClient->stop();
    return false;
  }

  String url = F("/api/runs");
  Serial.print(F("post URL: "));
  Serial.println(url);
  Serial.print(F("with data l1 "));
  Serial.print(l1);
  Serial.print(F(" l2 "));
  Serial.print(l2);
  Serial.print(F(" l3 "));
  Serial.print(l3);
  Serial.println("");

  String req;
  req += F("POST ");
  req += url;
  req += F(" HTTP/1.1\r\nHost: ");
  req += cfg.host;
  req += F("\r\nUser-Agent: WEMOSD1R2\r\nConnection: close\r\nContent-Type: application/octet-stream\r\n");
  req += F("Authorization: Token ");
  req += cfg.token;
  req += F("\r\nContent-Length: ");
  req += (l1+l2+l3);
  req += F("\r\n\r\n");
  
  wClient->print(req);
  
  if (l1 > 0) {
    int rc = wClient->write((uint8_t*)d1, l1);
    Serial.println(rc);
  }
  if (l2 > 0) {
    int rc = wClient->write((uint8_t*)d2, l2);
    Serial.println(rc);
  }
  if (l3 > 0) {
    int rc = wClient->write((uint8_t*)d3, l3);
    Serial.println(rc);
  }  

  Serial.println(F("request sent"));
  while (wClient->connected() || wClient->available()) {
    String line = wClient->readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      Serial.println(F("headers received"));
      break;
    }
  }
  String line = wClient->readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println(F("Post successfull!"));
    last_post_code = 5;
    return true;
  } else {
    last_post_code = 6;
    Serial.println(line);
    Serial.println(F("Post has failed"));
    return false;
  }
}

bool WebClient::trySendData(int year, int month, int day, int hour, int minute, int second, void *d1, int l1, void *d2, int l2, void *d3, int l3, bool save_on_fail, void *tmp, int tmpsize)
{
  if (sendData(d1, l1, d2, l2, d3, l3)) {
    // try send old stuff
    Dir dir = SPIFFS.openDir("/data");
    
    while (dir.next()) {
      Serial.println(dir.fileName());
      File f = dir.openFile("r");
      Serial.println(f.size());
      
      if (f.size() > tmpsize) {
        Serial.println(F("too big"));
        f.close();
        SPIFFS.remove(dir.fileName());
        continue;
      }
      
      if (f.read((uint8_t *)tmp, f.size()) != f.size()) {
        Serial.println(F("failed to read"));
        f.close();
        SPIFFS.remove(dir.fileName());
        continue;       
      }

      if (!sendData(tmp, f.size(), 0, 0, 0, 0)) {
        Serial.println(F("failed to send"));
        break;
      }

      Serial.println(F("sent"));
      f.close();
      SPIFFS.remove(dir.fileName());
    }
  } else if (save_on_fail) {
    // save to flash and send it later
    char fname[32];
    sprintf(fname, "/data/%04d%02d%02d_%02d%02d%02d", year, month, day, hour, minute, second);

    File f = SPIFFS.open(fname, "w");
    Serial.println(fname);
    if (f) {
      Serial.println(F("opened"));
      if (l1 > 0) f.write((const uint8_t *)d1, l1);
      if (l2 > 0) f.write((const uint8_t *)d2, l2);
      if (l3 > 0) f.write((const uint8_t *)d3, l3);
      f.close();
      Serial.println(F("closed"));
    }
  }  
}


