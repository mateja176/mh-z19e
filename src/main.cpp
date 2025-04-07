#include <Arduino.h>
#include <ArduinoJson.h>
#include <Configuration.h>
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WiFi.h>
#include <MHZ19.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

unsigned long fallingStartMs = 0;
void IRAM_ATTR handleInputFalling()
{
  fallingStartMs = millis();
}

#pragma region eeprom
const byte eepromSize = 128;
const byte nameMinLength = 2;
const byte nameMaxLength = 32;
const byte ssidMinLength = 2;
const byte ssidMaxLength = 32;
const byte pwMinLength = 8;
const byte pwMaxLength = 64;
const String printableAsciiPattern = "[\\x20-\\x80]+";
const String printableAsciiError = "<p>Only printable ASCII characters are supported.</p>";
#pragma endregion
void setName(char *name)
{
  for (byte i = 0; i < nameMaxLength; i++)
  {
    const char c = EEPROM.read(i);
    if (c == 0)
    {
      name[i] = 0;
      break;
    }
    else
    {
      name[i] = c;
    }
  }
}
void setSsid(char *ssid)
{
  for (byte i = 0; i < ssidMaxLength; i++)
  {
    const short j = i + nameMaxLength;
    const char c = EEPROM.read(j);
    if (c == 0)
    {
      ssid[i] = 0;
      break;
    }
    else
    {
      ssid[i] = c;
    }
  }
}
void setPw(char *pw)
{
  for (byte i = 0; i < pwMaxLength; i++)
  {
    const short j = i + nameMaxLength + ssidMaxLength;
    const char c = EEPROM.read(j);
    if (c == 0)
    {
      pw[i] = 0;
      break;
    }
    else
    {
      pw[i] = c;
    }
  }
}

int8_t wifiConnect(const char *ssid, const char *pw)
{
  WiFi.mode(WIFI_STA);
  return WiFi.begin(ssid, pw);
}
void wifiAP()
{
  WiFi.mode(WIFI_AP);

  char name[nameMaxLength];
  setName(name);
  WiFi.softAP(name[0] == 0 ? NAME : name);

  IPAddress ip = WiFi.softAPIP();
  Serial.println("wifi(ip): " + ip.toString());
}

ESP8266WebServer server(80);
void handleGetCredentials()
{
  char name[nameMaxLength];
  char ssid[ssidMaxLength];
  char pw[pwMaxLength];
  setName(name);
  setSsid(ssid);
  setPw(pw);
  String response = "";
  response += "<form method=\"POST\" action=\"/credentials\" enctype=\"text/plain\">";
  response += "<p>Version:" + String(VERSION) + "</p>";
  response += "<div><label>Device Name<br /><input";
  response += " name=\"n\"";
  response += " value=\"" + String(name[0] == 0 ? NAME : name) + "\"";
  response += " required";
  response += " minlength=\"" + String(nameMinLength) + "\"";
  response += " maxlength=\"" + String(nameMaxLength) + "\"";
  response += " pattern=\"" + printableAsciiPattern + "\"";
  response += " title=\"" + printableAsciiError + "\"";
  response += " /></label></div>";
  response += "<div><label>WiFi SSID<br /><input";
  response += " name=\"s\"";
  response += " value=\"" + String(ssid) + "\"";
  response += " required";
  response += " minlength=\"" + String(ssidMinLength) + "\"";
  response += " maxlength=\"" + String(ssidMaxLength) + "\"";
  response += " pattern=\"" + printableAsciiPattern + "\"";
  response += " title=\"" + printableAsciiError + "\"";
  response += " /></label></div>";
  response += "<div><label>WiFi Password<br /><input";
  response += " name=\"p\"";
  response += " type=\"password\"";
  response += " value=\"" + String(pw) + "\"";
  response += " required";
  response += " minlength=\"" + String(pwMinLength) + "\"";
  response += " maxlength=\"" + String(pwMaxLength) + "\"";
  response += " pattern=\"" + printableAsciiPattern + "\"";
  response += " title=\"" + printableAsciiError + "\"";
  response += " /></label></div>";
  response += "<hr />";
  response += "<div><input type=\"submit\" /></div>";
  response += "</form>";
  return server.send(200, "text/html", response);
}
void invalidateCredentials(String error)
{
  String response = "";
  response += "<h1>Credentials invalid</h1>";
  response += error;
  response += "<hr />";
  response += "<p>Click <a href=\"/\">here</a> to try again.</p>";
  server.send(400, "text/html", response);
}
void handleSetCredentials()
{
  const String body = server.arg("plain");
  const unsigned int bodyLength = body.length();
  const byte nameStartIndex = 2;
  unsigned int ssidStartIndex = 0;
  char name[nameMaxLength];
  for (unsigned int i = 0; i < bodyLength; i++)
  {
    const unsigned int j = i + nameStartIndex;
    const char c = body[j];
    if (!isAscii(c))
    {
      return invalidateCredentials(printableAsciiError);
    }
    if (!isPrintable(c))
    {
      ssidStartIndex = j + 4;
      name[i] = 0;
      break;
    }
    if (i + 1 >= nameMaxLength)
    {
      return invalidateCredentials("<p>Name cannot be longer than " + String(nameMaxLength) + " characters.</p>");
    }
    name[i] = c;
  }
  const unsigned int nameLength = ssidStartIndex - 4 - nameStartIndex;
  if (nameLength < nameMinLength)
  {
    return invalidateCredentials("<p>Name must be at least " + String(nameMinLength) + " characters long.</p>");
  }
  unsigned int pwStartIndex = 0;
  char ssid[ssidMaxLength];
  for (unsigned int i = 0; i < bodyLength; i++)
  {
    const unsigned int j = i + ssidStartIndex;
    const char c = body[j];
    if (!isAscii(c))
    {
      return invalidateCredentials(printableAsciiError);
    }
    if (!isPrintable(c))
    {
      pwStartIndex = j + 4;
      ssid[i] = 0;
      break;
    }
    if (i + 1 >= ssidMaxLength)
    {
      return invalidateCredentials("<p>SSID cannot be longer than " + String(ssidMaxLength) + " characters.</p>");
    }
    ssid[i] = c;
  }
  const unsigned int ssidLength = pwStartIndex - 4 - ssidStartIndex;
  if (ssidLength < ssidMinLength)
  {
    return invalidateCredentials("<p>SSID must be at least " + String(ssidMinLength) + " characters long.</p>");
  }
  const unsigned int pwLength = bodyLength - 2 - pwStartIndex;
  if (pwLength < pwMinLength)
  {
    return invalidateCredentials("<p>Password must be at least " + String(ssidMinLength) + " characters long.</p>");
  }
  if (pwLength > pwMaxLength)
  {
    return invalidateCredentials("<p>Password cannot be longer than " + String(pwMaxLength) + " characters.</p>");
  }
  char pw[pwLength];
  for (unsigned int i = 0; i < pwLength; i++)
  {
    const unsigned int j = i + pwStartIndex;
    const char c = body[j];
    if (!isAscii(c))
    {
      return invalidateCredentials(printableAsciiError);
    }
    pw[i] = c;
  }
  Serial.println("server(credentials/name): " + String(name) + ", " + String(nameLength));
  Serial.println("server(credentials/ssid): " + String(ssid) + ", " + String(ssidLength));
  Serial.println("server(credentials/pw): " + String(pw) + ", " + String(pwLength));

  WiFi.hostname(name);

  for (byte i = 0; i < eepromSize; i++)
  {
    if (i < nameLength)
    {
      EEPROM.write(i, name[i]);
    }
    else if (i < nameMaxLength)
    {
      EEPROM.write(i, 0);
    }
    else if (i < (nameMaxLength + ssidLength))
    {
      EEPROM.write(i, ssid[i - nameMaxLength]);
    }
    else if (i < (nameMaxLength + ssidMaxLength))
    {
      EEPROM.write(i, 0);
    }
    else if (i < (nameMaxLength + ssidMaxLength + pwLength))
    {
      EEPROM.write(i, pw[i - (nameMaxLength + ssidMaxLength)]);
    }
    else
    {
      EEPROM.write(i, 0);
      break;
    }
  }

  if (!EEPROM.commit())
  {
    String response = "";
    response += "<h1>Credentials not saved</h1>";
    response += "<p>Please retry <a href=\"/\">here</a>.</p>";
    return server.send(500, "text/html", response);
  };

  String response = "";
  response += "<h1>Credentials saved</h1>";
  response += "<p>Attempt to connect <a href=\"/connect\">here</a>.</p>";
  response += "<p>Connecting to the network may take a while.</p>";
  if (WiFi.getMode() == WIFI_AP)
  {
    response += "<p><strong>The current connection to the device will be lost.</strong></p>";
  }
  else
  {
    response += "<p><strong>If the SSID is wrong, you will need to reconnect to the device by manually switching to AP mode.</strong></p>";
  }
  server.send(200, "text/html", response);
}
void handleConnect()
{
  char ssid[ssidMaxLength];
  char pw[pwMaxLength];
  setSsid(ssid);
  setPw(pw);
  if (WiFi.getMode() == WIFI_AP && !WiFi.softAPdisconnect())
  {
    String response = "";
    response += "<h1>Failed to disconnect access point</h1>";
    response += "<p>Please try connecting again <a href=\"/connect\">here</a>.</p>";
    return server.send(200, "text/html", response);
  }
  const int8_t wifiStatus = wifiConnect(ssid, pw);
  if (wifiStatus == WL_WRONG_PASSWORD)
  {
    wifiAP();
  }
  // * only sent when reconnected
  String response = "";
  response += "<h1>Connected successfully</h1>";
  response += "<p>You may return to the <a href=\"/\">main page</a>.</p>";
  server.send(200, "text/html", response);
}

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
bool pubSubConnect()
{
  char name[nameMaxLength];
  setName(name);
  return pubSubClient.connect(name[0] == 0 ? NAME : name, PUBSUB_USER, PUBSUB_PW);
}

ESP8266HTTPUpdateServer httpUpdater;

SoftwareSerial softwareSerial(RX_PIN, TX_PIN);

MHZ19 mhz19;

// guarantees that a measurement will be taken when loop is first called
long msSinceMeasurement = -1 * MEASUREMENT_INTERVAL_MS;

void setup()
{
  Serial.begin(115200);

  pinMode(INPUT_PIN, INPUT_PULLUP);
  int interruptPin = digitalPinToInterrupt(INPUT_PIN);
  attachInterrupt(interruptPin, handleInputFalling, FALLING);

  EEPROM.begin(eepromSize);

  char name[nameMaxLength];
  setName(name);
  WiFi.hostname(name[0] == 0 ? NAME : name);
  char ssid[ssidMaxLength];
  char pw[pwMaxLength];
  setSsid(ssid);
  setPw(pw);
  Serial.println("eeprom(name): '" + String(name) + "'");
  Serial.println("eeprom(ssid): '" + String(ssid) + "'");
  Serial.println("eeprom(pw): '" + String(pw) + "'");
  const bool noCredentials = ssid[0] == 0 || pw[0] == 0;
  if (noCredentials)
  {
    wifiAP();
  }
  else
  {
    wifiConnect(ssid, pw);
  }

  server.begin();
  server.on("/", HTTP_GET, handleGetCredentials);
  server.on("/credentials", HTTP_POST, handleSetCredentials);
  server.on("/connect", HTTP_GET, handleConnect);

  pubSubClient.setServer(PUBSUB_BROKER_IP_ADDRESS, PUBSUB_PORT);
  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
  {
    Serial.println("wifi(ip): " + WiFi.localIP().toString());
    const bool connected = pubSubConnect();
    const String status = connected ? "connected" : "disconnected";
    Serial.println("pubsub(connection): " + status);
  }

  httpUpdater.setup(&server);

  Serial.println("\nInitializing MH-Z19 sensor...");
  // Start the software serial port for communication with the MH-Z19
  softwareSerial.begin(9600);
  // Initialize the MHZ19 library, passing the serial port object
  mhz19.begin(softwareSerial); // MH-Z19 default baud rate
  // Reference https://github.com/WifWaf/MH-Z19
  mhz19.autoCalibration(); // Turn auto calibration ON (OFF autoCalibration(false))
  Serial.println("MH-Z19 sensor initialized.");
}

unsigned long lastWifiCheckMs = 0;
unsigned long msSincePubsub = 0;
void loop()
{
  if (fallingStartMs != 0 && (millis() - fallingStartMs) >= AP_THRESHOLD_MS /* && digitalRead(INPUT_PIN) == HIGH */)
  {
    fallingStartMs = 0;
    wifiAP();
    return;
  }

  // runs in AP_MODE too
  server.handleClient();

  if ((millis() - lastWifiCheckMs) >= WIFI_DELAY_MS)
  {
    lastWifiCheckMs = millis();
    const wl_status_t wifiStatus = WiFi.status();
    if (WiFi.getMode() != WIFI_STA)
    {
      return;
    }
    if (wifiStatus == WL_NO_SHIELD)
    {
      Serial.println("wifi(status): no shield");
      return;
    }
    if (wifiStatus == WL_WRONG_PASSWORD)
    {
      Serial.println("wifi(status): wrong password");
      wifiAP();
      return;
    }
    if (wifiStatus == WL_IDLE_STATUS || wifiStatus == WL_SCAN_COMPLETED || wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_DISCONNECTED)
    {
      Serial.println("wifi(status): connecting");
      return;
    }
    if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_CONNECTION_LOST)
    {
      Serial.println("wifi(status): " + String(wifiStatus));
      char ssid[ssidMaxLength];
      char pw[pwMaxLength];
      setSsid(ssid);
      setPw(pw);
      wifiConnect(ssid, pw);
      return;
    }
    // WL_CONNECTED
  }

  if ((millis() - msSincePubsub) >= PUBSUB_DELAY_MS)
  {
    msSincePubsub = millis();
    const int pubSubState = pubSubClient.state();
    if (WiFi.status() != WL_CONNECTED)
    {
      return;
    }
    if (pubSubState == MQTT_CONNECT_BAD_PROTOCOL || pubSubState == MQTT_CONNECT_BAD_CLIENT_ID || pubSubState == MQTT_CONNECT_BAD_CREDENTIALS || pubSubState == MQTT_CONNECT_UNAUTHORIZED)
    {
      Serial.println("pubsub(state): " + String(pubSubState));
      return;
    }
    if (pubSubState == MQTT_CONNECTION_TIMEOUT || pubSubState == MQTT_CONNECTION_LOST || pubSubState == MQTT_CONNECT_FAILED || pubSubState == MQTT_DISCONNECTED || pubSubState == MQTT_CONNECT_UNAVAILABLE)
    {
      Serial.println("pubsub(state): " + String(pubSubState));
      pubSubConnect();
      return;
    }
    // MQTT_CONNECTED
  }

  pubSubClient.loop();

  if ((millis() - msSinceMeasurement) >= MEASUREMENT_INTERVAL_MS)
  {
    if (pubSubClient.state() != MQTT_CONNECTED)
    {
      return;
    }
    msSinceMeasurement = millis();
    const int co2Ppm = mhz19.getCO2();
    Serial.print("CO2 (ppm): ");
    Serial.println(co2Ppm);
    const float tempC = mhz19.getTemperature();
    Serial.print("Temperature (C): ");
    Serial.println(tempC);
    char name[nameMaxLength];
    setName(name);
    String topic = String(name[0] == 0 ? NAME : name) + "/" + String(TOPIC_MEASURE);
    StaticJsonDocument<32> response;
    response["co2"] = co2Ppm;
    response["temp"] = tempC;
    char payload[32];
    serializeJson(response, payload);
    pubSubClient.publish(topic.c_str(), payload);
  }
}
