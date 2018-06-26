#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include "WebClient.h"
#include "Config.h"

#include <ESP8266HTTPClient.h>

#include "FS.h"

WiFiClientSecure *wClient = nullptr;
void WebClient::init()
{
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(inet_ssid);
  WiFi.begin(inet_ssid, inet_password);
}


void WebClient::sendData(void *data, int len)
{
  sendData(data, len, 0, 0, 0, 0);
}

extern void gpsSendData(uint8_t *data, int size);

void WebClient::requestAGps()
{
  if (WiFi.status() != WL_CONNECTED) {
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
}

int last_web_post_code = 0;

bool WebClient::sendData(void *d1, int l1, void *d2, int l2, void *d3, int l3)
{
  last_web_post_code = 1;
  if (WiFi.status() != WL_CONNECTED) {
    last_web_post_code = 2;
    return false;
  }
  if (wClient == nullptr) {
    wClient = new WiFiClientSecure();
  }

  Serial.print("connecting to ");
  Serial.println(web_host);
  if (!wClient->connect(web_host, web_port)) {
    last_web_post_code = 3;
    Serial.println("connection failed");
    return false;
  }

  if (wClient->verify(web_fingerprint, web_host)) {
    Serial.println("certificate matches");
  } else {
    last_web_post_code = 4;
    Serial.println("certificate doesn't match");
    wClient->stop();
    return false;
  }

  String url = "/api/runs";
  Serial.print("post URL: ");
  Serial.println(url);
  Serial.print("with data l1 ");
  Serial.print(l1);
  Serial.print(" l2 ");
  Serial.print(l2);
  Serial.print(" l3 ");
  Serial.print(l3);
  Serial.println("");

  char str[256];
  sprintf(str, "Authorization: Token %s\r\nContent-Length: %d\r\n\r\n", web_auth, l1+l2+l3);
  

  wClient->print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + web_host + "\r\n" +
               "User-Agent: WEMOSD1R2\r\n" +
               "Connection: close\r\n"
               "Content-Type: application/octet-stream\r\n");
  wClient->print(str);
  
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

  Serial.println("request sent");
  while (wClient->connected() || wClient->available()) {
    String line = wClient->readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = wClient->readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("Post successfull!");
    last_web_post_code = 5;
    return true;
  } else {
    last_web_post_code = 6;
    Serial.println(line);
    Serial.println("Post has failed");
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
        Serial.println("too big");
        f.close();
        SPIFFS.remove(dir.fileName());
        continue;
      }
      
      if (f.read((uint8_t *)tmp, f.size()) != f.size()) {
        Serial.println("failed to read");
        f.close();
        SPIFFS.remove(dir.fileName());
        continue;       
      }

      if (!sendData(tmp, f.size(), 0, 0, 0, 0)) {
        Serial.println("failed to send");
        break;
      }

      Serial.println("sent");
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
      Serial.println("opened");
      if (l1 > 0) f.write((const uint8_t *)d1, l1);
      if (l2 > 0) f.write((const uint8_t *)d2, l2);
      if (l3 > 0) f.write((const uint8_t *)d3, l3);
      f.close();
      Serial.println("closed");
    }
  }  
}


