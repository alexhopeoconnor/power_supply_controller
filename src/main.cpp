#include <FS.h>
#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#define WEBSERVER_H TRUE // Prevents compilation issues with ESPAsyncWebServer
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>

#define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINT(x)     Serial.print (x)
  #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x) 
#endif

// MIME definitions
#define MIME_HTML "text/html"
#define MIME_CSS "text/css"
#define MIME_PNG "image/png"
#define MIME_ICO "image/vnd.microsoft.icon"
#define MIME_XML "application/xml"
#define MIME_JSON "application/json"
#define MIME_JAVASCRIPT "application/json"

// TEMPLATE STRINGS
#define TMPL_VERSION "VERSION"
#define TMPL_DEVICE_NAME "DEVICE_NAME"
#define TMPL_CURRENT_IN "CURRENT_IN"
#define TMPL_CURRENT_OUT "CURRENT_OUT"

// Define input/output pins
#define FAN_OUTPUT_PIN D0

// Define behaviour constants
#define DEVICE_NAME "BenchPSU"
#define FAN_MIN 125
#define FAN_MAX 255

// Config definition
#define CONFIG_START 0
#define CONFIG_VERSION "V1"
#define DEVICE_NAME_SIZE 24 // Max host name length is 24
typedef struct
{
  char version[5];
  char device_name[DEVICE_NAME_SIZE];
} configuration_type;

configuration_type CONFIGURATION = {
  CONFIG_VERSION,
  DEVICE_NAME
};

// Web server
AsyncWebServer server(80);

int loadConfig() 
{
  // Check the version, load (overwrite) the local configuration struct
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2]) {
    for (unsigned int i = 0; i < sizeof(CONFIGURATION); i++) {
      *((char*)&CONFIGURATION + i) = EEPROM.read(CONFIG_START + i);
    }
    return 1;
  }
  return 0;
}

void saveConfig() 
{
  // save the CONFIGURATION in to EEPROM
  for (unsigned int i = 0; i < sizeof(CONFIGURATION); i++) {
    EEPROM.write(CONFIG_START + i, *((char*)&CONFIGURATION + i));
  }
  EEPROM.commit();
}

void setupIOPins() {
  // Initialize pins
  pinMode(FAN_OUTPUT_PIN, OUTPUT);

  // Set initial outputs
  analogWriteFreq(14500);
  analogWrite(FAN_OUTPUT_PIN, 0);
}

void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    DEBUG_PRINTLN("OTA: Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_PRINTLN("\nOTA: End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINT("Progress: ");
    DEBUG_PRINT(progress / (total / 100));
    DEBUG_PRINTLN("%");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINT("Error[");
    DEBUG_PRINT(error);
    DEBUG_PRINT("]: ");
    if (error == OTA_AUTH_ERROR) {
      DEBUG_PRINTLN("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      DEBUG_PRINTLN("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      DEBUG_PRINTLN("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      DEBUG_PRINTLN("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      DEBUG_PRINTLN("End Failed");
    }
  });
  ArduinoOTA.begin();
}

String templateProcessor(const String& var) {
  if(var == TMPL_VERSION) {
    return CONFIG_VERSION;
  } else if(var == TMPL_DEVICE_NAME) {
    return CONFIGURATION.device_name;
  } else if(var == TMPL_CURRENT_IN) {
  } else if(var == TMPL_CURRENT_OUT) {
  }
  return var;
}

void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void addResponseHeaders(AsyncWebServerResponse *response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Headers", "*");
  response->addHeader("Access-Control-Allow-Credentials", "true");
  response->addHeader("Access-Control-Allow-Methods", "GET,PUT,POST,DELETE");
}

void sendResponseCode(AsyncWebServerRequest *request, int code, const String& content) {
  AsyncWebServerResponse *response = request->beginResponse(code, MIME_HTML, content);
  addResponseHeaders(response);
  request->send(response);
}

bool loadFromFS(AsyncWebServerRequest *request, const String& path, const String& dataType, bool templateResponse) {
  // Check the specified path
  if (SPIFFS.exists(path)) {

    // Check the file can be read
    File dataFile = SPIFFS.open(path, "r");
    if (!dataFile) {
        handleNotFound(request);
        return false;
    }
    // Send the response
    if(templateResponse) {
      AsyncWebServerResponse *response = request->beginResponse(SPIFFS, path, dataType, false, templateProcessor);
      addResponseHeaders(response);
      request->send(response);
    } else {
      request->send(SPIFFS, path, dataType);
    }
    dataFile.close();
  } else {
      handleNotFound(request);
      return false;
  }
  return true;
}

bool loadHTMLFromFS(AsyncWebServerRequest *request, const String& path) {
  return loadFromFS(request, path, MIME_HTML, true);
}

bool loadCSSFromFS(AsyncWebServerRequest *request, const String& path) {
  return loadFromFS(request, path, MIME_CSS, false);
}

bool loadPNGFromFS(AsyncWebServerRequest *request, const String& path) {
  return loadFromFS(request, path, MIME_PNG, false);
}

bool loadICOFromFS(AsyncWebServerRequest *request, const String& path) {
  return loadFromFS(request, path, MIME_ICO, false);
}

bool loadXMLFromFS(AsyncWebServerRequest *request, const String& path) {
  return loadFromFS(request, path, MIME_XML, false);
}

bool loadJSONFromFS(AsyncWebServerRequest *request, const String& path) {
  return loadFromFS(request, path, MIME_JSON, true);
}

bool loadJSFromFS(AsyncWebServerRequest *request, const String& path) {
  return loadFromFS(request, path, MIME_JAVASCRIPT, false);
}

bool setupServer() {
  // Mount SPIFFS
  if(SPIFFS.begin()) {
    // Setup the web server page handlers
    server.onNotFound(handleNotFound);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { loadHTMLFromFS(request, "/index.html"); });

    // Setup other resource handlers
    server.on("/static/css/main.ddcf899a.css", HTTP_GET, [](AsyncWebServerRequest *request) { loadCSSFromFS(request, "/static/css/main.ddcf899a.css"); });
    server.on("/static/js/main.a4cfdbdd.js", HTTP_GET, [](AsyncWebServerRequest *request) { loadJSFromFS(request, "/static/js/main.a4cfdbdd.js"); });
    server.on("/browserconfig.xml", HTTP_GET, [](AsyncWebServerRequest *request) { loadXMLFromFS(request, "/browserconfig.xml"); });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) { loadICOFromFS(request, "/favicon.ico"); });

    // Setup image handlers
    server.on("/logo.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/logo.png"); });
    server.on("/android-icon-36x36.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/android-icon-36x36.png"); });
    server.on("/android-icon-48x48.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/android-icon-48x48.png"); });
    server.on("/android-icon-72x72.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/android-icon-72x72.png"); });
    server.on("/android-icon-96x96.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/android-icon-96x96.png"); });
    server.on("/android-icon-144x144.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/android-icon-144x144.png"); });
    server.on("/android-icon-192x192.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/android-icon-192x192.png"); });
    server.on("/apple-icon-57x57.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-57x57.png"); });
    server.on("/apple-icon-60x60.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-60x60.png"); });
    server.on("/apple-icon-72x72.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-72x72.png"); });
    server.on("/apple-icon-76x76.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-76x76.png"); });
    server.on("/apple-icon-114x114.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-114x114.png"); });
    server.on("/apple-icon-120x120.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-120x120.png"); });
    server.on("/apple-icon-144x144.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-144x144.png"); });
    server.on("/apple-icon-152x152.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-152x152.png"); });
    server.on("/apple-icon-180x180.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-180x180.png"); });
    server.on("/apple-icon-precomposed.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon-precomposed.png"); });
    server.on("/apple-icon.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/apple-icon.png"); });
    server.on("/favicon-16x16.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/favicon-16x16.png"); });
    server.on("/favicon-32x32.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/favicon-32x32.png"); });
    server.on("/favicon-96x96.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/favicon-96x96.png"); });
    server.on("/ms-icon-70x70.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/ms-icon-70x70.png"); });
    server.on("/ms-icon-144x144.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/ms-icon-144x144.png"); });
    server.on("/ms-icon-150x150.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/ms-icon-150x150.png"); });
    server.on("/ms-icon-310x310.png", HTTP_GET, [](AsyncWebServerRequest *request) { loadPNGFromFS(request, "/ms-icon-310x310.png"); });

    // Setup JSON resource handlers
    server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest *request) { loadJSONFromFS(request, "/manifest.json"); });

    // Setup API handlers
    server.on("/current-status", HTTP_GET, [] (AsyncWebServerRequest *request) { loadJSONFromFS(request, "/current-status.json"); });

    // Start the server, return true
    server.begin();
    return true;
  }
  return false;
}

void setup() {
  #ifdef DEBUG
  // Initialize serial
  Serial.begin(9600);
  #endif

  // Initialize config
  EEPROM.begin(sizeof(CONFIGURATION));
  if(!loadConfig()) {
    saveConfig();
  }

  // Setup IO pins
  setupIOPins();
  
  // Initialize station mode
  WiFi.mode(WIFI_STA);
  WiFi.hostname(CONFIGURATION.device_name);
  WiFi.setAutoReconnect(true);

  // Initialize WiFi
  WiFiManager wifiManager;
  wifiManager.setConnectTimeout(180);
  wifiManager.setHostname(CONFIGURATION.device_name);
  bool wifiManagerAutoConnected = wifiManager.autoConnect(CONFIGURATION.device_name);

  // Restart if connection fails
  if(!wifiManagerAutoConnected) {
      ESP.restart();
      return;
  }

  // Setup OTA
  setupOTA();

  // Init web server
  setupServer();
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
}