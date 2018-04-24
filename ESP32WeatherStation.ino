/*
 *   ESP32 Weather Station development 
 */

#include <WiFi.h>
#include <WiFiUDP.h>
#include <ESPmDNS.h>
#include <ESP32WebServer.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <Ticker.h>
#include <BME280I2C.h>
#include <Wire.h>
#include "MySensorsLib.h"

// indicator LED
const byte BLUE_LED = 2;
#define blueLEDInit()   pinMode(BLUE_LED, OUTPUT);
#define blueLEDOn()     digitalWrite(BLUE_LED,0)
#define blueLEDOff()    digitalWrite(BLUE_LED,1)
#define blueLEDToggle() digitalWrite(BLUE_LED, !digitalRead(BLUE_LED))
Ticker blueLEDBlinker;
// inidication blink timings
const unsigned int IND_BLINK_AP_MODE_ms = 100;        // very quick blinking when in AP mode
const unsigned int IND_BLINK_WIFI_SEARCH_ms = 500;    // quick blinking when connecting to wifi
const unsigned int IND_BLINK_SENSOR_ERROR_ms = 1000;  // slow blinking when checking sensors
// ticker callbacks
void blinkBlueLED() { blueLEDToggle(); }

// Preferences = NVM storage
Preferences ESP32_NVMSettings;
const char *ESP32_namespace = "ESP32ap";
const char *ESP32_ssid_setting = "ssis";
const char *ESP32_pass_setting = "pass";
const char *ESP32_mode_setting = "mode";

// Working modes
enum ESP32_mode { MODE_AP, MODE_CLIENT } wifiMode = MODE_AP;
enum Sensor_mode { SENSE_NONE, SENSE_BME280, SENSE_BMP280 } senseMode = SENSE_NONE;

// default ssid and password for AP
const char *AP_ssid = "ESP32ap";
const char *AP_pass = "12345678";

// ssid and password for WIFI client
String CL_ssid;
String CL_pass;

// BME280 sensor settings
BME280I2C::Settings settings(
   BME280::OSR_X16,
   BME280::OSR_X16,
   BME280::OSR_X16,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_16,
   BME280::SpiEnable_False,
   0x76 // I2C address. I2C specific.
);
BME280I2C bme(settings);
const int UPDATE_INTERVAL_s = 10;
Ticker bmeDataUpdater;

// UDP Client
WiFiUDP udpClient;

void sendMyMsgOverUDP(IPAddress ip, int port, MyMessage message)
{
  char* DataBuffer;
  DataBuffer = message.protocolFormat();
  /* Test sending reports over UDP packed as MySensors messages */
  udpClient.beginPacket(ip,port);
  for (int i=0;i<sizeof(DataBuffer);i++)
  {
    if (DataBuffer[i]!='/n') udpClient.write((unsigned char)DataBuffer[i]);
    else break;
  }
  udpClient.endPacket();
}

void bmeUpdateData()
{
  float temp(NAN), hum(NAN), pres(NAN);

  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);
  
  bme.read(pres, temp, hum, tempUnit, presUnit);
    
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print("°"+ String(tempUnit == BME280::TempUnit_Celsius ? 'C' :'F'));
  Serial.print("\t\tHumidity: ");
  Serial.print(hum);
  Serial.print("% RH");
  Serial.print("\t\tPressure: ");
  Serial.print(pres);
  Serial.println("hPa");

  // UDP broadcast address / port
  IPAddress ipBCAddress(255,255,255,255);
  int nBCPort = 9009;

  // my MAC address
  String mac = WiFi.macAddress();

  // presentation / init messages
  // MyMessage::MyMessage(uint8_t _node_id, uint8_t _sensor_id, uint8_t _command, bool ack, uint8_t _type)
  MyMessage msgInit(0,0,C_INTERNAL,false,I_ID_REQUEST);
  msgInit.set(mac);
  sendMyMsgOverUDP(ipBCAddress,nBCPort,msgInit);

  // wait for ID, then begin presenting sensors

  // update data messages
  MyMessage messageTemp(1,V_TEMP);
  messageTemp.set(temp,2);
  MyMessage messageHum(2,V_HUM);
  messageHum.set(hum,2);
  MyMessage messagePres(3,V_PRESSURE);
  messagePres.set(pres,2);

  sendMyMsgOverUDP(ipBCAddress,nBCPort,messageTemp);
  sendMyMsgOverUDP(ipBCAddress,nBCPort,messageHum);
  sendMyMsgOverUDP(ipBCAddress,nBCPort,messagePres);
    
  /*
  Serial.println("-- UDP Messages --");
  Serial.println(messageTemp.protocolFormat());
  Serial.println(messageHum.protocolFormat());
  Serial.println(messagePres.protocolFormat());
  Serial.println("-- END --");
  */

  /*
  // deep sleep after update data
  Serial.println("Going to deep sleep...");
  digitalWrite(BLUE_LED, 1);      // turn OFF LED
  ESP.deepSleep(1000*1000*60); // go to sleep for 60s
  */
}

// web server at address 80
ESP32WebServer webServer(80);

// simple web page with forms
const char *AP_WEB_PAGE_HTML = 
"<!DOCTYPE html>\n"
"<html>\n"
"<body>\n"
"<h1 style=\"color:blue;margin-left:30px;\">\n"
"\tESP32 WiFi Settings\n"
"</h1>\n"
"<form action=\"/submit\" method=\"post\">\n"
"    <table>\n"
"    <tr>\n"
"    <td><label class=\"label\">Network Name : </label></td>\n"
"    <td><input type = \"text\" name = \"ssid\"/></td>\n"
"    </tr>\n"
"    <tr>\n"
"    <td><label>Password : </label></td>\n"
"    <td><input type = \"text\" name = \"pass\"/></td>\n"
"    </tr>\n"
"    <tr>\n"
"    <td align=\"center\" colspan=\"2\"><input style=\"color:blue;margin-left:auto;margin-right:auto;\" type=\"submit\" value=\"Submit\"></td>\n"
"    </tr>\n"
"    </table>\n"
"</form>\n"
"</body>\n"
"</html>";

const String CL_WEB_PAGE_HTML_BF_TEMP = 
"<!DOCTYPE html>\n"
"<html>\n"
"<body>\n"
"<h1 align=\"center\" style=\"color:blue;margin-left:30px;\">\n"
"\tESP32 Weatherstation\n"
"</h1>\n"
"<table align=\"center\">\n"
"\t<tr>\n"
"    \t<th colspan=\"2\" align=\"center\">BME280 sensor</th>\n"
"    </tr>\n"
"    <tr>\n"
"    \t<td>Temperature</td>\n";

const String CL_WEB_PAGE_HTML_BF_HUM = 
"    </tr>\n"
"    <tr>\n"
"    \t<td>Humidity</td>\n";

const String CL_WEB_PAGE_HTML_BF_PRES = 
"    </tr>\n"
"    <tr>\n"
"    \t<td>Atmospheric pressure</td>\n";

const String CL_WEB_PAGE_HTML_END = 
"    </tr>\n"
"    <tr>\n"
"    \t<form align action=\"/submit\" method=\"post\">\n"
"    \t<td align=\"center\"><input style=\"color:blue;\" type=\"submit\" value=\"Refresh\"></td>\n"
"        <td align=\"center\"><input style=\"color:red;\" type=\"submit\" value=\"Reset Settings\"></td>\n"
"        </form>\n"
"    </tr>\n"
"</table>\n"
"</body>\n"
"</html>";

// web server callbacks
void handleAPRoot()
{
  webServer.send(200,"text/html",AP_WEB_PAGE_HTML);
}

void handleCLRoot()
{
  float temp(NAN), hum(NAN), pres(NAN);

  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);
  
  bme.read(pres, temp, hum, tempUnit, presUnit);

  String page = CL_WEB_PAGE_HTML_BF_TEMP+
                "\t<td align=\"right\">"+
                String(temp,2)+
                "°C</td>\n"+
                CL_WEB_PAGE_HTML_BF_HUM+
                "\t<td align=\"right\">"+
                String(hum,2)+
                "%</td>\n"+
                CL_WEB_PAGE_HTML_BF_PRES+
                "\t<td align=\"right\">"+
                String(pres,2)+
                "hPa</td>\n"+
                CL_WEB_PAGE_HTML_END;
  
  webServer.send(200,"text/html",page);
/*
  "    \t<td align=\"right\">20°C</td>\n"
  "    \t<td align=\"right\">35%</td>\n"
  "    \t<td align=\"right\">900hPa</td>\n"
*/
}

void handleAPOnSubmit()
{
  webServer.send(200, "text/plain", "Form submited! Restarting...");
  Serial.print("Form submitted with: ");
  Serial.print(webServer.arg("ssid"));
  Serial.print(" and ");
  Serial.println(webServer.arg("pass"));
  ESP32_NVMSettings.putString(ESP32_ssid_setting,webServer.arg("ssid"));
  ESP32_NVMSettings.putString(ESP32_pass_setting,webServer.arg("pass"));
  ESP32_NVMSettings.putUChar(ESP32_mode_setting,MODE_CLIENT);
  blueLEDOff();
  ESP.restart();
}

void handleCLOnSubmit()
{
  webServer.send(200, "text/plain", "Settings cleared! Restarting...");
  ESP32_NVMSettings.clear();
  ESP32_NVMSettings.putUChar(ESP32_mode_setting,MODE_AP);
  blueLEDOff();
  ESP.restart();
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  webServer.send(404, "text/plain", message);
}

void setup()
{
  // initialize LED indicator
  blueLEDInit();
  blueLEDOff();

  // initialize debug port
  Serial.begin(115200);
  Serial.println();

  // initialize preferences
  ESP32_NVMSettings.begin(ESP32_namespace, false);

  // print values
  Serial.println("Reading preferences...");
  CL_ssid = ESP32_NVMSettings.getString(ESP32_ssid_setting,"");
  CL_pass = ESP32_NVMSettings.getString(ESP32_pass_setting,"");
  wifiMode = (ESP32_mode)ESP32_NVMSettings.getUChar(ESP32_mode_setting,MODE_AP);
  Serial.print("SSID : ");
  Serial.println(CL_ssid);
  Serial.print("Password : ");
  Serial.println(CL_pass);
  Serial.print("wifiMode : ");
  
  switch (wifiMode)
  {
    case MODE_CLIENT:
      Serial.println("MODE_CLIENT");
      setup_CL();
      break;
    default:
    case MODE_AP:
      Serial.println("MODE_AP");
      blueLEDBlinker.attach_ms(IND_BLINK_AP_MODE_ms,blinkBlueLED);
      setup_AP();
      break;
  }

  // inidcate init done
  blueLEDOn();
}

// start in AP mode
void setup_AP()
{
  // start AP
  Serial.println("Configuring access point...");
  WiFi.softAP(AP_ssid, AP_pass);
  IPAddress AP_IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(AP_IP);

  // setup mDNS
  if (MDNS.begin("esp32"))
  {
    Serial.println("MDNS responder started...");
  }

  // register web server callback functions
  webServer.on("/",handleAPRoot);
  webServer.onNotFound(handleNotFound);
  webServer.on("/submit",HTTP_POST,handleAPOnSubmit);

  // start web server
  webServer.begin();
  Serial.println("HTTP server started...");
}

// start in client mode
void setup_CL()
{
  // try connecting to a WiFi network
  blueLEDBlinker.attach_ms(IND_BLINK_WIFI_SEARCH_ms,blinkBlueLED);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(CL_ssid.c_str());
  
  int nTimeOutCnt = 0;
  WiFi.begin(CL_ssid.c_str(), CL_pass.c_str());
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    nTimeOutCnt++;
    if(nTimeOutCnt>120)
    {
      Serial.println("Can not connect to AP ...");
      Serial.println("Switching to AP mode for settings ...");
      ESP32_NVMSettings.putUChar(ESP32_mode_setting,MODE_AP);
      blueLEDOff();
      ESP.restart();
    }
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  blueLEDBlinker.detach();
  
  // start I2C interface
  Wire.begin(21,22,100000);
  delay(100);
  
  // find BME280 sensor
  nTimeOutCnt = 0;
  blueLEDBlinker.attach_ms(IND_BLINK_SENSOR_ERROR_ms,blinkBlueLED);
  senseMode = SENSE_BME280;
  while(!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
    nTimeOutCnt++;
    if(nTimeOutCnt>5)
    {
      senseMode = SENSE_NONE;
      break;
      // deep sleep if sensor is not found
      // Serial.println("Going to deep sleep...");
      // digitalWrite(BLUE_LED, 1);      // turn OFF LED
      // ESP.deepSleep(1000*1000*60);    // go to sleep for 60s
    }
  }

  // check which sesor we found
  if (senseMode!=SENSE_NONE)
  {
    switch(bme.chipModel())
    {
      case BME280::ChipModel_BME280:
        Serial.println("Found BME280 sensor! Success.");
        senseMode = SENSE_BME280;
        break;
      case BME280::ChipModel_BMP280:
        Serial.println("Found BMP280 sensor! No Humidity available.");
        senseMode = SENSE_BMP280;
        break;
      default:
        senseMode=SENSE_NONE;
        Serial.println("Found UNKNOWN sensor! Error!");
    }
  }
  // attach data updates
  if (senseMode!=SENSE_NONE) bmeDataUpdater.attach(UPDATE_INTERVAL_s,bmeUpdateData);
  blueLEDBlinker.detach();

  // start UDP Client
  if (udpClient.begin(9009))
  {
    Serial.println("UDP Client started...");
  }

  // setup mDNS
  if (MDNS.begin("esp32"))
  {
    Serial.println("MDNS responder started...");
  }

  // register web server callback functions
  webServer.on("/",handleCLRoot);
  webServer.onNotFound(handleNotFound);
  webServer.on("/submit",HTTP_POST,handleCLOnSubmit);

  // start web server
  webServer.begin();
  Serial.println("HTTP server started...");
  blueLEDOn();
  
}

void loop()
{
  webServer.handleClient();
}
