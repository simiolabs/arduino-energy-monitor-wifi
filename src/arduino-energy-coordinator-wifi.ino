/*
Energy monitor application. This program receives data
from a node with n sensors and sends the data to the internet

Copyright (C) 2013 Simio Labs, created by Daniel Montero
Copyright (C) 2017 Simio Labs, modified by Daniel Montero

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <SPI.h>
#include <WiFi.h>
#include <XBee.h>

WiFiClient client;
char ssid[] = "Peluche";              //  your network SSID (name)
char pass[] = "chimichurri";          // your network password
int status = WL_IDLE_STATUS;

char server[] = "simiolabs.com";      // remote host site
const String apiKey = "100bdc36203b7a7183fa0b79445ebda6";
const byte NUM_FIELDS = 5;
const String feedName[NUM_FIELDS] = { "apparent_power", "current",
                                      "power_factor", "real_power",
                                      "voltage" };
String feedData[NUM_FIELDS];

XBee xbee = XBee();
// create reusable response objects for responses we expect to handle
XBeeResponse response = XBeeResponse();
ZBRxResponse rx = ZBRxResponse();
ModemStatusResponse msr = ModemStatusResponse();

byte node = 1;
char realPower[8] = { 0 };
char apparentPower[8] = { 0 };
char current[8] = { 0 };
char voltage[8] = { 0 };
char powerFactor[8] = { 0 };


void setup() {
  Serial.begin(9600);
  xbee.begin(Serial); // initialize Xbee module
  initWiFi(); // get IP address from router
  uploadData();
}

void loop() {
  xbee.readPacket();

  if (xbee.getResponse().isAvailable()) {
    // got something
    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
      // got a zb rx packet

      // now fill our zb Rx class
      xbee.getResponse().getZBRxResponse(rx);

      if (rx.getOption() == ZB_PACKET_ACKNOWLEDGED) {
      // the sender got an ACK
        Serial.println("ACK");
      } else {
        // we got it (obviously) but sender didn't get an ACK
        Serial.println("no ACK");
      }

      Serial.print("from: ");
      Serial.println(rx.getRemoteAddress16());

      processData();
    } else if (xbee.getResponse().getApiId() == MODEM_STATUS_RESPONSE) {
      xbee.getResponse().getModemStatusResponse(msr);
      // the local XBee sends this response on certain events, like association/dissociation

      if (msr.getStatus() == ASSOCIATED) {
        // yay this is great
        //Serial.println("Associated");
      } else if (msr.getStatus() == DISASSOCIATED) {
        // this is awful..
        //Serial.println("Disssociated");
      } else {
        // another status
        //Serial.println("Unknown");
      }
    } else {
      // not something we were expecting
      Serial.println("Unexp. error");
    }
  } else if (xbee.getResponse().isError()) {
    Serial.print("Read error: ");
    Serial.println(xbee.getResponse().getErrorCode());
  }
  delay(1000);
}

void initWiFi() {
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while(true);
  }

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi");
  printWifiStatus();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm \n");
}

void processData() {
  // get node id and sensor number
  node = rx.getData(41);
  uint8_t sensor = rx.getData(40);
  Serial.print('N');
  Serial.print(node);
  Serial.print('S');
  Serial.println(sensor);

  getDataFromPayload(realPower, 0, 8); // variable, position, length
  getDataFromPayload(apparentPower, 8, 8);
  getDataFromPayload(current, 16, 8);
  getDataFromPayload(voltage, 24, 8);
  getDataFromPayload(powerFactor, 32, 8);

  printData();

  feedData[0] = String(apparentPower);
  feedData[1] = String(current);
  feedData[2] = String(powerFactor);
  feedData[3] = String(realPower);
  feedData[4] = String(voltage);

  Serial.println("Uploading...");
  uploadData();
}

void getDataFromPayload(char *dataString, int pos, int length) {
  for (int i = pos; i < pos + length; i++, dataString++) {
    *dataString = rx.getData(i);
  }
}

// comment this function out to free some memory!
void printData() {
  Serial.println(realPower);
  Serial.println(apparentPower);
  Serial.println(voltage);
  Serial.println(current);
  Serial.println(powerFactor);
}

void uploadData() {
  Serial.print("Connect to server: ");
  // if you get a connection, report back via serial
  if (client.connect(server, 80)) {
    Serial.println("connected");
    // make a HTTP request
    client.print("GET /emoncms/input/post.json?node=");
    client.print(node);
    client.print("&json={");
    int lastItem = NUM_FIELDS - 1;
    for (int i = 0; i < lastItem; i++) {
      client.print(feedName[i]);
      client.print(":");
      client.print(feedData[i]);
      client.print(",");
    }
    client.print(feedName[lastItem]);
    client.print(":");
    client.print(feedData[lastItem]);
    client.println("}&apikey=100bdc36203b7a7183fa0b79445ebda6 HTTP/1.1");
    client.println("Host: simiolabs.com");
    client.println("Connection: close");
    client.println();
  } else {
    // if you didn't get a connection to the server
    Serial.println("connection failed");
  }

  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  }
  Serial.println();
  Serial.println("Disconnecting...");
  client.stop();
}
