/*
 IR Wifi Server
 This application is used as a wifi server on a arduino nano esp32. It can be used 
 to extend the range of rc5 IR remote controlls to the range of you wifi network.

 A dual colour led is connected to pin D5 (G) and D6(R). The IR LED is connected to D4
 The ssid and password is hard compiled and has to be set. 
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include "PinDefinitionsAndMore.h" 
#include <IRremote.hpp>

#define LED_G D5
#define LED_R D6

const char* ssid     = "ZTE_A1DE7F";//"Hochmueller";// // Change this to your WiFi SSID
const char* password = "5P64662Z2D";//"hochmueller";// // Change this to your WiFi password
const char* hostname = "IR_RemoteServer";  // This is the mDNS hostname

WiFiServer server(5000);

void setup()
{
    Serial.begin(115200);
    //while (!Serial) //can be uncommend for debugging to see first logs.
    Serial.println("RC5 Server starting...\n");

    //construct the IR receiver
    IrSender.begin();
    IrSender.enableIROut(38); //Enable output with 38kHz modulation

    //setting Status outputs
    // RED: No Wifi
    // GREEN+RED = YELLOW: Wifi connected but no connection to client
    // GREEN: Wifi and client connected
    pinMode(LED_G, OUTPUT);
    pinMode(LED_R, OUTPUT);

    // We start by connecting to a WiFi network
}

void loop(){
  //Nothing connected
  analogWrite(LED_R,255);
  analogWrite(LED_G,0);

  //Start and connect wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // Start mDNS to be found from clients
  if (!MDNS.begin(hostname)) {
    Serial.println("Error starting mDNS");
    return;
  }
  Serial.println("mDNS responder started");
  server.begin();

  int size = 0;
  char rxbuf[100]; //buffer to receive data from client
  char framebuf[100]; //buffer to reassamble the send frame.
  char frameIdx =0;
  bool frameStart = false;

  WiFiClient client;
  
  //we only need to reconnect if wifi is dissconnected. 
  while (WiFi.status() == WL_CONNECTED) {
    //Yellow: wifi ok, no client
    analogWrite(LED_R,32);
    analogWrite(LED_G,32);

    //we need to implement a periodic pin to the client so the client can
    //detect a lost connection. Same is done on the server. 
    //the proviced functions of the library wifi client don't work properly
    //the size of send msg is alway greather zero and the isconnected methode
    //is only retruning fals if the connection was closed by one of the nodes
    bool connected = true;
    uint32_t time_ms_rx=millis(); //time variable to detect timeout from client
    uint32_t time_ms_tx=millis(); //time variable to send a ping each s
    client = server.available();   // listen for incoming clients
    if (client) {  
      Serial.println("New Client."); 
      //we only support one client connected. As long it is connected we 
      //don't need to open a new connection      
      while(connected)
      {
        //Green -> wifi + client connected.
        analogWrite(LED_R,0);
        analogWrite(LED_G,64);
        //wait for new data
        size = client.available();
        if (size) {             // if there's bytes to read from the client,
          time_ms_rx=millis(); //reset rx time variable 
          //expecting a frame of format <a: <addr>; c: <cmd>> or <p>(ping)
          //is is possible that this frame is received in multiple tcp frames, so we need to reassamble it
          client.read((uint8_t*)rxbuf,size);
          for(int i = 0; i<size;i++)
          {
            if (rxbuf[i]=='<')
              frameStart=true;
            else if(rxbuf[i]=='>')
            {
              frameStart = false;
              framebuf[frameIdx]=0;
              frameIdx=0;
              if(framebuf[0]=='a')
              {
                //Serial.printf("%s\n",framebuf);
                int addr = 0, cmd = 0;
                sscanf(framebuf, "a: %i; c: %i", &addr, &cmd);
                Serial.printf("%i,%i\n",addr,cmd);
                IrSender.sendRC5(addr, cmd, 1);
              }
            }
            else if(frameStart)
              framebuf[frameIdx++]=rxbuf[i];
          }
        } 
        //RX timeout we can assume the conneciton did break
        if(millis()-time_ms_rx > 3000)
        {
            connected=false;
            Serial.println("RX Timeout");
        }
        //send a empyt frame each 1s
        if((millis()-time_ms_tx > 1000))
        {
          client.println("");
          time_ms_tx = millis();
        }
      }
      // close the connection:
      client.stop();
      Serial.println("Client Disconnected.");
    }
  }
}
