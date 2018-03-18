/*
 *   ESP32 Weather Station development 
 */

#include <WiFi.h>
#include <BME280I2C.h>
#include <Wire.h>
#include <Ticker.h>

// set corrrect values
// TODO: move to EEPROM, and fill over AP
const char* ssid     = "...........";
const char* password = "...........";

// set BME280 sensor
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
//BME280I2C bme;

WiFiServer server(80);
WiFiUDP udpInterface;
const byte BLUE_LED = 2;
const int UPDATE_INTERVAL = 10;
Ticker blinker;
Ticker updater;

void blinkBlueLED() 
{
  digitalWrite(BLUE_LED, !digitalRead(BLUE_LED));
}

void updateData()
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

   // deep sleep after update data
   Serial.println("Going to deep sleep...");
   digitalWrite(BLUE_LED, 1);      // turn OFF LED
   ESP.deepSleep(1000*1000*60); // go to sleep for 60s
}

void setup()
{
    Serial.begin(115200);
    pinMode(BLUE_LED, OUTPUT);      // set the LED pin mode
    digitalWrite(BLUE_LED, 1);      // turn OFF LED
    delay(10);

    // start I2C interface
    Wire.begin(21,22,100000);
    delay(10);

    // find BME280 sensor
    int nTimeOutCnt = 0;
    blinker.attach(1, blinkBlueLED);
    while(!bme.begin())
    {
      Serial.println("Could not find BME280 sensor!");
      delay(1000);
      nTimeOutCnt++;
      if(nTimeOutCnt>30)
      {
        // deep sleep if sensor is not found
        Serial.println("Going to deep sleep...");
        digitalWrite(BLUE_LED, 1);      // turn OFF LED
        ESP.deepSleep(1000*1000*60);    // go to sleep for 60s
      }
    }
    blinker.detach();

    // check which sesor we found
    switch(bme.chipModel())
    {
      case BME280::ChipModel_BME280:
        Serial.println("Found BME280 sensor! Success.");
        break;
      case BME280::ChipModel_BMP280:
        Serial.println("Found BMP280 sensor! No Humidity available.");
        break;
      default:
        Serial.println("Found UNKNOWN sensor! Error!");
     }

    // We start by connecting to a WiFi network

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    nTimeOutCnt = 0;
    WiFi.begin(ssid, password);
    blinker.attach(0.2, blinkBlueLED);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        nTimeOutCnt++;
        if(nTimeOutCnt>120)
        {
          // deep sleep if it can not connect to WiFi
          Serial.println("Going to deep sleep...");
          digitalWrite(BLUE_LED, 1);      // turn OFF LED
          ESP.deepSleep(1000*1000*60);    // go to sleep for 60s
        }
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    blinker.detach();
    server.begin();

    udpInterface.begin(12123);

    // everything started, begin sending weather data to server
    updater.attach(UPDATE_INTERVAL, updateData);

    digitalWrite(BLUE_LED, 0);      // turn ON LED

}

int value = 0;

void loop(){

  if (udpInterface.parsePacket()>0)
  {
    Serial.println("UDP Packet received ...");
    while(udpInterface.available())
    {
      char c = udpInterface.read();    // receive a byte as character
      Serial.print(c);                 // print the character
    }
    Serial.println("");
    IPAddress remoteIP = udpInterface.remoteIP();
    unsigned int remotePort = udpInterface.remotePort(); 
    Serial.print("IP: ");
    Serial.print(remoteIP);
    Serial.print(":");
    Serial.println(remotePort,DEC);

    IPAddress testIP(255,255,255,255);

    // try sending udp data
    udpInterface.beginPacket(testIP,1024);
    udpInterface.write(64);
    udpInterface.write(65);
    udpInterface.endPacket();
  }
 
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // Show weather station info
            // temp = bme.temp() hum = bme.hum() pres = bme.pres()
            float temp(NAN), hum(NAN), pres(NAN);

            BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
            BME280::PresUnit presUnit(BME280::PresUnit_hPa);

            bme.read(pres, temp, hum, tempUnit, presUnit);

            client.print("<H3>Temperatura : ");
            client.print(temp);
            client.print("°C</H3>");
            client.print("<H3>Vlaga : ");
            client.print(hum);
            client.print("% </H3>");
            client.print("<H3>Zračni tlak : ");
            client.print(pres);
            client.print(" hPa </H3>");
            // the content of the HTTP response follows the header:
            client.print("Click <a href=\"/H\">here</a> to turn the LED on pin 5 on.<br>");
            client.print("Click <a href=\"/L\">here</a> to turn the LED on pin 5 off.<br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.endsWith("GET /H")) {
          digitalWrite(BLUE_LED, LOW);               // GET /H turns the LED on
        }
        if (currentLine.endsWith("GET /L")) {
          digitalWrite(BLUE_LED, HIGH);                // GET /L turns the LED off
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
