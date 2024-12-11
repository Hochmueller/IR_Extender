/*
  IR Wifi Client
  This application as a wifi client on a arduino nano esp32. It can be used
  to extend the range of the rc5 IR remote controlls to the range of your wifi network.

  A dual colour led is connected to D5 (G) and D6(R). The IR receover is connected on D3
*/

#include <WiFi.h>
#include <ESPmDNS.h>

#include <Arduino.h>

#include "PinDefinitionsAndMore.h" // Define macros for input and output pin etc.
#include <IRremote.hpp> //from manage libraries

//Flag to tell main new IR packed received
volatile bool sIRDataJustReceived = false;

void ReceiveCompleteCallbackHandler();

#define DECODE_RC5

#define LED_G D5
#define LED_R D6

const char* ssid     = "ZTE_A1DE7F";//"Hochmueller";// // Change this to your WiFi SSID
const char* password = "5P64662Z2D";//"hochmueller";// // Change this to your WiFi password
const char* hostname = "IR_RemoteServer";  // This is the mDNS hostname


const int port = 5000; // This should not be changed
IPAddress serverIP;


void setup()
{
  Serial.begin(115200);
  //while(!Serial){delay(100);} //can be uncommend for debugging to see first logs. 

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing IR library version " VERSION_IRREMOTE));
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  IrReceiver.registerReceiveCompleteCallback(ReceiveCompleteCallbackHandler);
  Serial.print(F("Ready to receive IR signals of protocols: "));
  printActiveIRProtocols(&Serial);
  Serial.println(F("at pin " STR(IR_RECEIVE_PIN)));

  //setting Status outputs
  // RED: No Wifi
  // GREEN+RED = YELLOW: Wifi connected but no connection to Serever
  // GREEN: Wifi + Server connected
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);

  

}

//IR Callback form library.
IRAM_ATTR
void ReceiveCompleteCallbackHandler() {
    IrReceiver.decode(); // fill IrReceiver.decodedIRData
    /*
     * Enable receiving of the next value.
     */
    IrReceiver.resume();

    sIRDataJustReceived = true;
}

//This loop includes multiple sub loops
//The loops are exit if some conection is lost and it will reopen it in the parent loop again
void loop(){
  //First set the red led to indicate we don't have Wifi
  analogWrite(LED_R,255);
  analogWrite(LED_G,0);

  //Try to conect to Wifi
  WiFi.begin(ssid, password); // in init?
  Serial.println("Try to connect to wifi");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  //Set LED to yellow as soon we have Wifi connected
  digitalWrite(LED_G,HIGH);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Resolve server IP using mDNS
  while ( (!MDNS.begin("esp32-client")) && (WiFi.status() == WL_CONNECTED)) {  // Start mDNS on client
    Serial.println("Starting mDNS client...");
    delay(1000);
  }

  WiFiClient client;
  char buf[100];
  int size;

  int addr;
  int cmd;

  while(WiFi.status() == WL_CONNECTED)
  {
    //Set yellow as in this stage wifi is connected but no tcp connection is established
    analogWrite(LED_R,32);
    analogWrite(LED_G,32);
    // Try to get IP address from Server
    serverIP = MDNS.queryHost(hostname);
    if (serverIP == IPAddress(0, 0, 0, 0))
    {
      //Try Again if not successfull
      Serial.println("Failed to resolve server IP");
    } 
    else 
    {
      //Got IP address from Server
      Serial.print("Resolved IP for ");
      Serial.print(hostname);
      Serial.print(": ");
      Serial.println(serverIP);
      bool connected = false;

    
      //wait to connect to Server
      while(!connected && WiFi.status() == WL_CONNECTED)
      { 
        Serial.println("Try to connect to Server");
        connected=client.connect(serverIP, port);
        sleep(0.5);
      }
      //Set the LED to green as we have a TCP connection
      analogWrite(LED_R,0);
      analogWrite(LED_G,64);
      //Lets save two variables to compare time later
      uint32_t time_ms_tx=millis();
      uint32_t time_ms_rx=millis();

      //While we are connected to Serve
      while(connected && WiFi.status() == WL_CONNECTED)
      {
        int bytesSend;
        if (sIRDataJustReceived && IrReceiver.decodedIRData.protocol == RC5) 
        {
          cmd = IrReceiver.decodedIRData.command;
          addr = IrReceiver.decodedIRData.address;
          //IrReceiver.printIRSendUsage(&Serial); //Can be enabled for debugging
          //Create a packat of format <a: <addres>; c: <command>>
          String msg = "<a: " + String(addr) + "; c: " + String(cmd) + ">";
          bytesSend=client.print(msg);
          //I realized that even if the client is offline the client.print
          //will return a positive bytesSend. So actually this is useless.
          //maybe in future versions of the library is fixe.
          if(bytesSend==0)
            connected=false;
          //Reset the tx time counter
          time_ms_tx=millis();
        } 
        //required in case the IR sender sends with high packed rate. 
        //In this case we would overload the tcp stack and data will be send
        //after we stopped pushing any button on the remote
        usleep(200000); 
        //reset the flag
        sIRDataJustReceived=false; 

        //In case we have not send anything for a second we will send a ping packet
        //This is required as the wifi clint library can't detect if the connection 
        //is lost. So if the Server is offline we have no way to detect this even.
        //To solve this we regular send ping packets to the communication partner (in both directions)
        //So we can detect a timeout
        if (millis()-time_ms_tx > 1000)
        {
          time_ms_tx = millis();
          bytesSend=client.write("<p>");
          //again, will most probably never reached.
          if(bytesSend==0)
            connected=false;
        }

        //Other direction of the ping. We check if we have recieved something every
        //3 seconds. If not we can assume tha we lost the connection and we will try to 
        //reconnect
        if(client.available())
        {
          //we received something so reset rx time and flush buffer
          time_ms_rx = millis();
          client.flush();
        }


        if( (millis()-time_ms_rx) > 3000)
        {
          //We have not recieved anything within 3 seconds.
          connected = false;
          time_ms_rx = millis();
          Serial.println("RX Timeout");
        }
      }
      Serial.println("Disconnected");
      client.stop();  
    }
  } 
}
