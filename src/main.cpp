#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include "ArduinoJson.h"
#include "main.h"
#include <arduino-timer.h>
#include "keyboardCodes.h"
// #define RAPORT_WIFI_STATUS

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  startWiFi();      // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  startOTA();       // Start the OTA service
  startSPIFFS();    // Start the SPIFFS and list all contents
  startWebSocket(); // Start a WebSocket server
  startMDNS();      // Start the mDNS responder
  startServer();    // Start a HTTP server with a file read handler and an upload handler
}

void loop()
{
  timer.tick();

  webSocket.loop(); // constantly check for websocket events
#ifdef RAPORT_WIFI_STATUS
  raportStatusOnSerial();
#endif
  server.handleClient(); // run the server
  ArduinoOTA.handle();   // listen for OTA events
}

void startWiFi()
{
  WiFi.begin("Orange_Swiatlowod_Gora", "mlekogrzybowe"); // add Wi-Fi networks you want to connect to

  Serial.println("Connecting");

  unsigned long startTime = millis();
  bool connectionFailed = false;

  while (WiFi.status() != WL_CONNECTED && !connectionFailed)
  { // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
    connectionFailed = (millis() - startTime < delayBeforeAP) ? false : true;
  }

  if (connectionFailed)
  {
    WiFi.softAP(ssid, password); // Start the access point
    Serial.print("Access Point \"");
    Serial.print(ssid);
    Serial.println("\" started\r\n");
  }
  else
  {
    Serial.println("\r\n");

    Serial.println("\r\n");

    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  }
}

void startOTA()
{ // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]()
                     { Serial.println("Start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\r\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

void startSPIFFS()
{                 // Start the SPIFFS and list all contents
  SPIFFS.begin(); // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    { // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void startWebSocket()
{                                    // Start a WebSocket server
  webSocket.begin();                 // start the websocket server
  webSocket.onEvent(webSocketEvent); // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void startMDNS()
{                       // Start the mDNS responder
  MDNS.begin(mdnsName); // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startServer()
{
  // Start a HTTP server with a file read handler and an upload handler
  server.onNotFound(handleNotFound); // if someone requests any other file or page, go to function 'handleNotFound'

  server.begin(); // start the HTTP server
  Serial.println("HTTP server started.");
}

void handleNotFound()
{
  if (!handleFileRead(server.uri()))
  { // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.html";                    // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {                                                     // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                      // If there's a compressed version available
      path += ".gz";                                    // Use the compressed verion
    File file = SPIFFS.open(path, "r");                 // Open the file
    size_t sent = server.streamFile(file, contentType); // Send it to the client
    file.close();                                       // Close the file again
    Serial.println(String("\tSent file-> ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path); // If the file doesn't exist, return false
  return false;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght)
{ // When a WebSocket message is received
  switch (type)
  {
  case WStype_DISCONNECTED: // if the websocket is disconnected
    Serial.printf("[%u] Disconnected!\n", num);
    break;
  case WStype_CONNECTED:
  { // if a new websocket connection is established
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
  }
  break;
  case WStype_TEXT: // if new text data is received
    Serial.printf("[%u] got WS text: %s\n", num, payload);

    // String payload_str = String((char*) payload);

    // process received JSON
    DeserializationError error = deserializeJson(jsonDoc, payload);

    // Test if parsing succeeds.
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // Fetch values.
    // Most of the time, you can rely on the implicit casts.
    // In other case, you can do doc["time"].as<long>();
    if (jsonDoc["type"] == "keyPress")
    {
      Serial.print(F("Keypress msg arrived: "));
      String keyPressed = jsonDoc["keyPressed"];
      Serial.println(keyPressed);
      // blink LED to acknowledge
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      parseAndPressKey(keyPressed);
    }
    else if (jsonDoc["type"] == "someotherthing")
    {
      Serial.print(F("hihihi"));
    }

    break;
  }
}

// dummy keyboard obj, so it compiles
class KeyboardDummy
{
public:
  void press(uint32_t keyCode)
  {
  }
};
KeyboardDummy Keyboard;

void parseAndPressKey(String key)
{
  // assign keycode to the string that arrived and send to pc
  // only has to be done for special keys like ctrl/alt
  if (key == "Ctrl")
    Keyboard.press(KEY_LEFT_CTRL);
  else if (key == "Alt")
    Keyboard.press(KEY_LEFT_ALT);
  else if (key == "<i class=\"fa-brands fa-windows\"></i>")
    Keyboard.press(KEY_LEFT_GUI);
  else if (key == "Shift")
    Keyboard.press(KEY_LEFT_SHIFT);
  else if (key == "Enter")
    Keyboard.press(KEY_RETURN);
  else if (key == "Back")
    Keyboard.press(KEY_BACKSPACE);
  else if (key == "") // space
    Keyboard.press(' ');
  else if (key == "Caps Lock")
    Keyboard.press(KEY_CAPS_LOCK);
  else if (key == "Tab")
    Keyboard.press(KEY_TAB);
  else if (key == "↑")
    Keyboard.press(KEY_UP_ARROW);
  else if (key == "↓")
    Keyboard.press(KEY_DOWN_ARROW);
  else if (key == "←")
    Keyboard.press(KEY_LEFT_ARROW);
  else if (key == "→")
    Keyboard.press(KEY_RIGHT_ARROW);
  else if (key == "Del")
    Keyboard.press(KEY_DELETE);
}

String formatBytes(size_t bytes)
{ // convert sizes in bytes to KB and MB
  if (bytes < 1024)
  {
    return String(bytes) + "B";
  }
  else if (bytes < (1024 * 1024))
  {
    return String(bytes / 1024.0) + "KB";
  }
  else if (bytes < (1024 * 1024 * 1024))
  {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
  return String(0);
}

String getContentType(String filename)
{ // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

void raportStatusOnSerial()
{
#if 0
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    Serial.printf("\e[0;34m ############### Info ############### \e[0m \n");

    Serial.printf("Signal strength: %i dBm\n", WiFi.RSSI());

    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    Serial.print("localIP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Status: ");

    char color[20] = "\e[0;31m"; // red by default
    switch (WiFi.status())
    {
    case WL_IDLE_STATUS:
    {
      Serial.printf("%sWL_IDLE_STATUS \e[0m\n", color);
      break;
    }
    case WL_NO_SSID_AVAIL:
    {
      Serial.printf("%sWL_NO_SSID_AVAIL \e[0m\n", color);
      break;
    }
    case WL_SCAN_COMPLETED:
    {
      Serial.printf("%sWL_SCAN_COMPLETED \e[0m\n", color);
      break;
    }
    case WL_CONNECTED:
    {
      strcpy(color, "\e[0;32m"); // change to green when connected
      Serial.printf("%sWL_CONNECTED \e[0m\n", color);
      break;
    }
    case WL_CONNECT_FAILED:
    {
      Serial.printf("%sWL_CONNECT_FAILED \e[0m\n", color);
      break;
    }
    case WL_CONNECTION_LOST:
    {
      Serial.printf("%sWL_CONNECTION_LOST \e[0m\n", color);
      break;
    }
    case WL_DISCONNECTED:
    {
      Serial.printf("%sWL_DISCONNECTED \e[0m\n", color);
      break;
    }
    }
  }
#endif
}