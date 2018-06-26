#ifndef WEB_CLIENT_H
#define WEB_CLIENT_H

  
  
struct WebClient {
  void init();
  void sendData(void *data, int len);
  bool sendData(void *d1, int l1, void *d2, int l2, void *d3, int l3);
  void requestAGps();

  // date used to make file name and save it to flash if send failed, if succeeded try to send old files in flash
  bool trySendData(int year, int month, int day, int hour, int minute, int second, void *d1, int l1, void *d2, int l2, void *d3, int l3, bool save_on_fail, void *tmp, int tmpsize);
};
#endif
