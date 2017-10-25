//Name: David Walshe
//Date: 16/08/2017
//Verison: 2.1.3

//##################################################################################################################################################
//Code
//##################################################################################################################################################
//===================================================================================================
//Include Libraries
//===================================================================================================
#include <RFM69.h>
#include <SPI.h>

//===================================================================================================
//Declare Defines
//===================================================================================================
#define NETWORKID     1   // Must be the same for all nodes (0 to 255)
#define MYNODEID      1   // My node ID (0 to 255)
#define TONODEID      1   // Destination node ID (0 to 254, 255 = broadcast)

// RFM69 frequency, uncomment the frequency of your module:
#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY     RF69_915MHZ

// AES encryption (or not):
#define ENCRYPT       true // Set to "true" to use encryption
#define ENCRYPTKEY    "WEREFIRED_170517" // Use the same 16-byte key on all nodes

// Use ACKnowledge when sending messages (or not):
#define USEACK        true // Request ACKs or not

//===================================================================================================
//Constant variable declaration
//===================================================================================================
const uint8_t TEST_BUTTON = 7;
const uint8_t TEST_LED = 6;         //GREEN
const uint8_t RECEIVE_LED = 5;      //BLUE
const uint8_t TRANSMIT_LED = 4;     //RED
const uint8_t READY_LED = 3;        //YELLOW


//===================================================================================================
//Class Object Declaration
//===================================================================================================
// Create a library object for our RFM69HCW module:
RFM69 radio;

//===================================================================================================
//Fucntion Prototypes
//===================================================================================================
void Blink(byte PIN, int DELAY_MS);
void transferViaJSON(String roomInfo, String statusInfo, String timeStamp, String tempInfo, String smokeDetect);
void addToJSONs(String *string, String nameTag, String dataTag);
void addToJSON(String *string, String nameTag, String dataTag);

//===================================================================================================
//Global variable declarations
//===================================================================================================
String dataStr = "";


//##################################################################################################################################################
//Main Setup
//##################################################################################################################################################
void setup()
{
  //===================================================================================================
  //Serial Setup
  //===================================================================================================
  Serial.begin(9600);

  //===================================================================================================
  //RFM69 Radio Setup
  //===================================================================================================
  radio.initialize(FREQUENCY, MYNODEID, NETWORKID);
  radio.setHighPower(); // Always use this for RFM69HCW

  // Turn on encryption if desired:
  if (ENCRYPT)
    radio.encrypt(ENCRYPTKEY);

  //===================================================================================================
  //Pin Setup
  //===================================================================================================
  pinMode(READY_LED, OUTPUT);
  pinMode(RECEIVE_LED, OUTPUT);
  pinMode(TRANSMIT_LED, OUTPUT);
  pinMode(TEST_LED, OUTPUT);
  pinMode(TEST_BUTTON, INPUT);
  digitalWrite(READY_LED, HIGH);
}


//##################################################################################################################################################
//Main Loop
//##################################################################################################################################################
void loop()
{
  //===================================================================================================
  //Local Variable Declaration for Main Loop
  //===================================================================================================
  static char sendbuffer[62];
  static int sendlength = 0;

  //===================================================================================================
  //Packet received from sender
  //===================================================================================================
  if (radio.receiveDone()) // Got one!
  {
    Blink(RECEIVE_LED, 100);
    // Print out the information

    // The actual message is contained in the DATA array,
    // and is DATALEN bytes in size:
    dataStr = "";
    for (byte i = 0; i < radio.DATALEN; i++)
    {
      dataStr += (char)radio.DATA[i];
    }
    dataStr += ",1,1,1";

    // Send an ACK if requested.
    // (You don't need this code if you're not using ACKs.)

    if (radio.ACKRequested())
    {
      radio.sendACK();
      //Serial.println("ACK sent");
    }
    Blink(LED, 10);
    String strData[10];
    char datach[dataStr.length()+1];
    dataStr.toCharArray(datach, dataStr.length());
    char *token;
    token = strtok(datach, ",");
    strData[0] = token;
    for (int i = 1; i < 5; i++)
    {
      token = strtok(NULL, ",");
      strData[i] = token;
    }
    transferViaJSON(strData[0], strData[1], strData[2], strData[3], strData[4]);
    Blink(TRANSMIT_LED, 100);
  }

  //===================================================================================================
  //Serial driver for validating if PySerial connection can parse JSON into the database correctly. 
  //===================================================================================================
  int testFunc = 0;
  for(int i = 0; i < 3; i++)
  {
    if(digitalRead(TEST_BUTTON) == HIGH)
    {
        testFunc++;
        Blink(TEST_LED, 100);
        delay(1000);
    }
    else
    {
      break;
    }
  }
  if(testFunc > 0)
  {
    String room = ""; 
    switch(testFunc)
    {
       case 1: room="1"; break;
       case 2: room="6"; break;
       case 3: room="11"; break;
       default: room="1"; break;
    }
    Blink(TEST_LED, 50);
    delay(50);
    Blink(TEST_LED, 50);
    delay(50);
    Blink(TEST_LED, 50);
    delay(50);
    randomSeed(analogRead(0));
    int tempOutput = random(18, 99);
    int tempOutputDecimal = random(0, 100);
    String tempStr = "";
    tempStr += tempOutput;
    tempStr += ".";
    tempStr += tempOutputDecimal;
    transferViaJSON(room, "1", "12:00:00", tempStr, "1");
    while(digitalRead(TEST_BUTTON) == HIGH);
  }
}


//##################################################################################################################################################
//Function Declarations
//##################################################################################################################################################

//===================================================================================================
//Function: Blink
//Utility function used to blink a LED for UI purposes
//===================================================================================================
void Blink(byte PIN, int DELAY_MS)
// Blink an LED for a given number of ms
{
  digitalWrite(PIN, HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN, LOW);
}

//===================================================================================================
//Function: transferViaJSON
//Collects and assembles a valid JSON string to be parsed by the Python serial script running on 
//a host machine.
//===================================================================================================
void transferViaJSON(String roomInfo, String statusInfo, String timeStamp,
                     String tempInfo, String smokeDetect)
{
  String string = "{ ";
  addToJSON(&string, "room", roomInfo);
  string += ", ";
  addToJSON(&string, "status", statusInfo);
  string += ", ";
  addToJSONs(&string, "timestamp", timeStamp);
  string += ", ";
  addToJSONs(&string, "temp", tempInfo);
  string += ", ";
  addToJSONs(&string, "smoke", smokeDetect);
  string += " }";
  Serial.println(string);
}

//===================================================================================================
//Function: addToJSONs
//This function formats tag variables into a JSON format, also append a "\"" to the end of the string
//to keep in line with JSON formatting
//===================================================================================================
void addToJSONs(String *string, String nameTag, String dataTag)
{
  String temp = "";
  temp += "\"";
  temp += nameTag;
  temp += "\":\"";
  temp += dataTag;
  temp += "\"";
  *string += temp;
}

//===================================================================================================
//Function: addToJSON
//This function formats tag variables into a JSON format
//===================================================================================================
void addToJSON(String *string, String nameTag, String dataTag)
{
    String temp = "";
    temp += "\"";
    temp += nameTag;
    temp += "\":";
    temp += dataTag;
    *string += temp;
}

