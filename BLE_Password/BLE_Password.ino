//Name: David Walshe
//Date: 24/06/2017
//Version: 3.2.2

//##################################################################################################################################################
//Code
//##################################################################################################################################################
//===================================================================================================
//Include Libraries
//===================================================================================================
#include <CurieBLE.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <RFM69.h>
#include <SPI.h>
#include <CurieTime.h>
#include <EEPROM.h>

//===================================================================================================
//Declare Defines
//===================================================================================================
#define DEBUG 1
#define DEMO 1
#define UNIT 1

// RFM69 frequency, uncomment the frequency of your module:
#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY     RF69_915MHZ

// AES encryption (or not):
#define ENCRYPT       true // Set to "true" to use encryption
#define ENCRYPTKEY    "WEREFIRED_170517" // Use the same 16-byte key on all nodes

// Use ACKnowledge when sending messages (or not):
#define USEACK        true // Request ACKs or not

//EEPROM MEMORY ADDRESSES
#define NODE_ADDR  0
#define TO_NODE_ADDR  1
#define NETWORK_ADDR  2
#define ROOM_ADDR 3


//===================================================================================================
//Constant variable declaration
//===================================================================================================
int colorRGB[3] = {255, 255, 255};

//===================================================================================================
//Class Object Declaration
//===================================================================================================
rgb_lcd lcd;

BLEPeripheral blePeripheral;  // BLE Peripheral Device (the board you're programming)
BLEService ledService("19B10000-E8F2-537E-4F6C-D104768A1214"); // BLE LED Service

// BLE LED Switch Characteristic - custom 128-bit UUID, read and writable by central
BLEUnsignedCharCharacteristic switchCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite);

// Create a library object for our RFM69HCW module:
RFM69 radio;

//===================================================================================================
//Fucntion Prototypes
//===================================================================================================
double getTemperature();
void sendDataOverRadio();
void changeRoomID(String text, String *ID, BLECentral *ble);
void changeRadioID(String text, uint8_t *ID, BLECentral *ble);
boolean isStringNumber(char *charArray, byte len);
void getBleData(String *string, byte len, boolean printout, BLECentral *ble);
bool verifyPassword(BLECentral *ble);
void lcd_standby();
void lcd_print(String text, byte line, boolean clr);
void set_EEPROM(int addr, byte data);
void get_EEPROM(int addr, byte *data);
String print2digits(int number);

//===================================================================================================
//Global variable declarations
//===================================================================================================
uint8_t NETWORK_ID = 1;
uint8_t NODE_ID = 1;
uint8_t TO_NODE_ID = 1;
String ROOM_ID = "1";
uint8_t ROOM_ID_INT = 1;
String TIME = "14-35";
volatile bool sendDataFlag = 0;

//===================================================================================================
//Enum declarations
//===================================================================================================
enum STATE {INIT, CONFIG, HELP};

//##################################################################################################################################################
//ISR - Interrupt Service Routines
//##################################################################################################################################################

//===================================================================================================
//Function: ISR_setSendDataFlag
//Trigger: FALLING Edge pulse on pin 6
//This function is to set a flag to send the data in the main loop
//===================================================================================================
void ISR_setSendDataFlagPress()
{
  sendDataFlag = 1;
  detachInterrupt(digitalPinToInterrupt(7));
}


//##################################################################################################################################################
//Main Setup
//##################################################################################################################################################
void setup() {
  
  //===================================================================================================
  //Interrupt Setup
  //===================================================================================================
  detachInterrupt(digitalPinToInterrupt(7));
  attachInterrupt(digitalPinToInterrupt(4), ISR_setSendDataFlagPress, FALLING);
  interrupts();
  //===================================================================================================
  //Serial Setup
  //===================================================================================================
  Serial.begin(9600);
  setTime(00, 00, 00, 24, 07, 2017);
  
  //===================================================================================================
  //Pin Setup
  //===================================================================================================
  pinMode(7, INPUT);
  //===================================================================================================
  //LCD Setup
  //===================================================================================================
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  colorRGB[0] = 0;
  colorRGB[1] = 255;
  colorRGB[2] = 0;
  lcd.setRGB(colorRGB[0], colorRGB[1], colorRGB[2]);

  //===================================================================================================
  //BLE Setup
  //===================================================================================================
  // set advertised local name and service UUID:
  switch(UNIT)
  {
    case 1: blePeripheral.setLocalName("ES_DEMO_UNIT"); break;
    case 2: blePeripheral.setLocalName("ES_ROOM_U2"); break;
    case 3: blePeripheral.setLocalName("ES_ROOM_U3"); break;
    default: blePeripheral.setLocalName("ES_DEMO_UNIT"); break;
  }
  blePeripheral.setAdvertisedServiceUuid(ledService.uuid());

  // add service and characteristic:
  blePeripheral.addAttribute(ledService);
  blePeripheral.addAttribute(switchCharacteristic);

  // set the initial value for the characeristic:
  switchCharacteristic.setValue(0);

  // begin advertising BLE service:
  blePeripheral.begin();

  //===================================================================================================
  //RFM69 Radio Setup
  //===================================================================================================
  lcd.print("Reading EEPROM");
  get_EEPROM(NETWORK_ADDR, &NETWORK_ID);
  get_EEPROM(NODE_ADDR, &NODE_ID);
  get_EEPROM(TO_NODE_ADDR, &TO_NODE_ID);
  get_EEPROM(ROOM_ADDR, &ROOM_ID_INT);
  if(ROOM_ID_INT != 0 && ROOM_ID_INT != 255)
  {
     ROOM_ID = "";
     ROOM_ID += ROOM_ID_INT;
  }
  delay(1000);
  lcd.clear();
  lcd.print("Done");
  delay(1000);
  // Initialize the RFM69HCW:
  radio.initialize(FREQUENCY, NODE_ID, NETWORK_ID);
  radio.setHighPower(); // Always use this for RFM69HCW

  // Turn on encryption if desired:
  if (ENCRYPT)
    radio.encrypt(ENCRYPTKEY);
}



//##################################################################################################################################################
//Main Loop
//##################################################################################################################################################
void loop() {
  //===================================================================================================
  //Enumeration setup
  //===================================================================================================
  enum STATE state;
  state = CONFIG;

  //===================================================================================================
  //Local Variable Declaration for Main Loop
  //===================================================================================================
  String cmd = "";
  bool all_OK = 1;

  while (1)
  {
    // listen for BLE peripherals to connect:
    BLECentral central = blePeripheral.central();

    // if a central is connected to peripheral:
    if (central) {
      if (DEBUG)
      {
        // print the central's MAC address:
        Serial.print("Connected to central: ");
        Serial.println(central.address());
      }
      if(all_OK == 1)
      {
          colorRGB[0] = 255;
          colorRGB[1] = 255;
          colorRGB[2] = 255;
          lcd.setRGB(colorRGB[0], colorRGB[1], colorRGB[2]);
      }
      // while the central is still connected to peripheral:
      bool alertPresent = 0; 
      while (central.connected()) {
        //If the user has not logged in using the correct passward, enter login mode
        switch (state)
        {
          case INIT:
            state = CONFIG;
            break;
          case HELP:
              TIME = "";
              TIME += print2digits(hour());
              TIME += ":";
              TIME += print2digits(minute());
              TIME += ":";
              TIME += print2digits(second());
              sendDataOverRadio();
              colorRGB[0] = 255;
              colorRGB[1] = 0;
              colorRGB[2] = 0;
              all_OK = 0;
              lcd_print("Phone Activated", 0, 1);
              lcd_print("Message Sent", 1, 0);
              lcd.setRGB(colorRGB[0], colorRGB[1], colorRGB[2]);
              delay(1000);
              state = CONFIG;
          case CONFIG:
            cmd = "";
            Serial.println("cmd");
            if(alertPresent == 0 && all_OK == 1)
            {
                lcd_print("User Connected ", 0, 1);
                lcd.setCursor(0, 1);
            }
            while (
              cmd.indexOf("NET") == -1 &&
              cmd.indexOf("NODE") == -1 &&
              cmd.indexOf("TO") == -1 &&
              cmd.indexOf("ROOM") == -1 &&
              cmd.indexOf("EXIT") == -1 &&
              cmd.indexOf("HELP") == -1 &&
              central.connected()
            )
            {
              getBleData(&cmd, cmd.length(), 1, &central);
            }
            if ((cmd.indexOf("NET") > -1) && (alertPresent == 0))
            {
              changeRadioID("NET ID: ", &NETWORK_ID, &central);
              radio.setNetwork(NETWORK_ID);
              set_EEPROM(NETWORK_ADDR, NETWORK_ID);
              state = CONFIG;
            }
            else if ((cmd.indexOf("NODE") > -1) && (alertPresent == 0))
            {
              changeRadioID("NODE ID: ", &NODE_ID, &central);
              radio.setAddress(NODE_ID);
              set_EEPROM(NODE_ADDR, NODE_ID);
              state = CONFIG;
            }
            else if ((cmd.indexOf("TO") > -1) && (alertPresent == 0))
            {
              changeRadioID("NODE_TO ID: ", &TO_NODE_ID, &central);
              set_EEPROM(TO_NODE_ADDR, TO_NODE_ID);
              state = CONFIG;
            }
            else if ((cmd.indexOf("ROOM") > -1) && (alertPresent == 0))
            {
              changeRoomID("ROOM ID: ", &ROOM_ID, &central);
              set_EEPROM(ROOM_ADDR, ROOM_ID.toInt());
              state = CONFIG;
            }
            else if (cmd.indexOf("HELP") > -1)
            {
              alertPresent = 1; 
              state = HELP;
            }
            else if (alertPresent == 1)
            {
              state = CONFIG;
            }
            break;
          default:
            lcd_print("FAULT", 0, 1);
            delay(1000);
            break;
        }
      }
      state = INIT;
    }
    if(sendDataFlag)
    {
      TIME = "";
      TIME += print2digits(hour());
      TIME += ":";
      TIME += print2digits(minute());
      TIME += ":";
      TIME += print2digits(second());
      sendDataFlag = 0;
      sendDataOverRadio();
      all_OK = 0;
      colorRGB[0] = 255;
      colorRGB[1] = 0;
      colorRGB[2] = 0;
      lcd_print("Press detected", 0, 1);
      lcd_print("Message Sent", 1, 0);
      lcd.setRGB(colorRGB[0], colorRGB[1], colorRGB[2]);
    }
    if(all_OK)
    {
      lcd_standby();
    }
  }
}



//##################################################################################################################################################
//Function Declarations
//##################################################################################################################################################


//===================================================================================================
//Function: print2digits
//This function is used to printout a number in a 2 character slot, numbers under 10 are proceeded
//with a "0"
//===================================================================================================
String print2digits(int number) 
{
  String str = "";
  if (number >= 0 && number < 10) {
    str = "0";
  }
  str += number;
  return str;  
}

//===================================================================================================
//Function: set_EEPROM
//This function is used to read from an address in EEPROM assess that it is valid and 
//assign it to a passed pointer.
//===================================================================================================
void get_EEPROM(int addr, byte *data)
{
   int temp = EEPROM.read(addr);
   if(temp != 0 && temp != 255)
   {
      *data = temp;
   }
}

//===================================================================================================
//Function: set_EEPROM
//This function is used to set a specified address in EEPROM to a user passed value.
//===================================================================================================
void set_EEPROM(int addr, byte data)
{
        EEPROM.write(addr, data); 
        lcd.clear();
        lcd.print("EEPROM set");
        delay(1000);
}

//===================================================================================================
//Function: lcc_print
//This function is used to clear the lcd display and print a string of less then 17 characters to
//the display after clearing.
//===================================================================================================
void lcd_print(String text, byte line, boolean clr)
{
  if (text.length() < 16)             // Check if str in suitable for lcd printout
  {
    if (clr == 1)
    {
      lcd.clear();                     //Clear lcd screen, retaining color setting
    }
    lcd.setCursor(0, line);          //Move cursor to line 0 or 1 depending on user ARGV[1]
    lcd.print(text);                 //Print the text to the specified line
  }
  else
  {
    lcd.clear();                     //Clear lcd screen, retaining color setting
    lcd.setCursor(0, line);          //Move cursor to line 0 or 1 depending on user ARGV[1]
    lcd.print("LCD SIZE ERROR");     //Print the text to the specified line
  }
}

//===================================================================================================
//Function: lcd_standby
//This function is used to print key config data to the LCD screen, such as NET, NODE, TO_NODE and
//Apartent number information.
//===================================================================================================
void lcd_standby()
{
  String printout = "NET:";
  printout += NETWORK_ID;
  printout += " NODE:";
  printout += NODE_ID;
  lcd_print(printout, 0, 1);
  printout = "TO:";
  printout += TO_NODE_ID;
  printout += " ROOM:";
  printout += ROOM_ID;
  lcd_print(printout, 1, 0);
  colorRGB[0] = 0;
  colorRGB[1] = 255;
  colorRGB[2] = 0;
  lcd.setRGB(colorRGB[0], colorRGB[1], colorRGB[2]);
  delay(1000);
}

//===================================================================================================
//Function: verifyPassword
//This function is used to check if a user input matches a predifined password string. It only
//returns if the password is correct.
//===================================================================================================
bool verifyPassword(BLECentral *ble)
{
  lcd_print("Password: ", 0, 1);
  lcd.setCursor(0, 1);
  String pwd = "";
  while (pwd.indexOf("INTEL") == -1 && pwd.indexOf("HELP") == -1 && ble->connected())
  {
    // if the remote device wrote to the characteristic, read in the value sent

    getBleData(&pwd, pwd.length(), 1, ble);                                 //Get data from BLE client(phone)
    if (pwd.length() > 5)                             //If the password is over the defined password size then the user has entered it wrong
    {
      pwd = "";                             //Clear the password
      delay(150);                           //Small delay to see the last char sent via the client device
      lcd_print("Password Error", 0, 1);       //Info user of incorrect password entry
      delay(1000);
      lcd_print("Password: ", 0, 1);          //Reset LCD for next user input stream.
      lcd.setCursor(0, 1);
    }
  }
  if (ble->connected())
  {
    if(pwd.indexOf("INTEL") > -1) 
    {
        delay(150);                               //Small delay to see the last char sent via the client device
        lcd_print("ACCESS GRANTED", 0, 0);           //Notify user of access
        delay(1000);
        lcd_print("Welcome", 1, 1);
        return 0; 
    }
    else if(pwd.indexOf("HELP") > -1)
    {
        return 1;
    }
  }
  return 0;
}

//===================================================================================================
//Function: getBleData
//This function is used to obtain data from a BLE client and save the data in a string pointer to be
//passed back
//===================================================================================================
void getBleData(String *string, byte len, boolean printout, BLECentral *ble)
{
  while (switchCharacteristic.written() == false && ble->connected());      //Wait for a value to be received
  if (ble->connected())
  {
    *string += (char)switchCharacteristic.value();        //Add the value received to the str pointer for checking
    if (printout)
    {
      lcd.print(string->charAt(len));                 //Print the char received from the str, to ensure data saving was correct.
    }
  }
}

//===================================================================================================
//Function: isStringNumber
//This function is used to check if a string only contains numbers, if yes, return a 1, if not,
//return 0
//===================================================================================================
boolean isStringNumber(char *charArray, byte len)
{

  for (uint16_t i = 0; i < len; i++)
  {
    if (*(charArray + i) == '0' || *(charArray + i) == '1' ||
        *(charArray + i) == '2' || *(charArray + i) == '3' ||
        *(charArray + i) == '4' || *(charArray + i) == '5' ||
        *(charArray + i) == '6' || *(charArray + i) == '7' ||
        *(charArray + i) == '8' || *(charArray + i) == '9'
       )
    {}
    else
    {
      return 0;
    }
  }
  return 1;
}


//===================================================================================================
//Function: changeRadioID
//This function is used to change the network and node variable for the RMF69
//===================================================================================================
void changeRadioID(String text, uint8_t *ID, BLECentral *ble)
{
  String printout = text;
  printout += *ID;
  lcd_print(printout, 0, 1);
  String userInput = "";
  lcd.setCursor(0, 1);
  while (userInput.indexOf("#") == -1 && ble->connected())
  {
    getBleData(&userInput, userInput.length(), 1, ble);
    delay(100);
  }
  if (ble->connected())
  {
    char buf[userInput.length()];
    userInput.toCharArray(buf, userInput.length());
    if (isStringNumber(buf, userInput.length() - 1))
    {
      if((userInput.toInt() < 256) && (userInput.toInt() > -1))
      {
        *ID = userInput.toInt();
        printout = text;
        printout += *ID;
      }
      else
      {
         printout = "Invalid ID";
         printout += *ID;
      }
      lcd.setCursor(0, 1);
      lcd.print(printout);
      delay(500);
    }
    else
    {
      lcd.setCursor(0, 1);
      lcd.print("Invalid ID");
      delay(1000);
    }
  }
}

//===================================================================================================
//Function: changeRadioID
//This function is used to change the network and node variable for the RMF69
//===================================================================================================
void changeRoomID(String text, String *ID, BLECentral *ble)
{
  String printout = text;
  printout += *ID;
  lcd_print(printout, 0, 1);
  String userInput = "";
  lcd.setCursor(0, 1);
  while (userInput.indexOf("#") == -1 && ble->connected())
  {
    getBleData(&userInput, userInput.length(), 1, ble);
    delay(100);
  }
  if (ble->connected())
  {
    *ID = userInput.substring(0, userInput.length() - 1);
    printout = text;
    printout += *ID;
    lcd.setCursor(0, 1);
    lcd.print(printout);
    delay(500);
  }
}


//===================================================================================================
//Function: sendDataOverRadio
//This function is used to send data over the RFM69. 
//===================================================================================================
void sendDataOverRadio()
{
  //===================================================================================================
  //Setup Packet Variables
  //===================================================================================================
  char sendbuffer[62];      //Create data buffer for RFM69 max packet transfer
  int sendlength = 20;      //This is the length of the valid data inside the packet to send
  int counter = 0;          //Counter variable to add data to the packet buffer in sequence

  //===================================================================================================
  //Add required data to packet for transmission
  //===================================================================================================
  // Add the Apartment Number to the packet
  for (uint16_t i = 0; i < ROOM_ID.length(); i++)
  {
    sendbuffer[counter++] = ROOM_ID.charAt(i);
  }
  sendbuffer[counter++] = ',';          //Add comma to seperate the data values
  sendbuffer[counter++] = '1';          //Add the room status code to the packet
  sendbuffer[counter++] = ',';          //Add comma to seperate the data values
  // Add the time stamp to the packet
  for (uint16_t i = 0; i < TIME.length(); i++)
  {
    sendbuffer[counter++] = TIME.charAt(i);
  }
  sendbuffer[counter++] = ',';                //Add comma to seperate the data values
  
  String temperatureStr = "";
  if(DEMO == 0)
  {
      temperatureStr += 21;
      for (uint16_t i = 0; i < temperatureStr.length(); i++)
      {
          sendbuffer[counter++] = temperatureStr.charAt(i);  //Increment through the string to add the RAW temp to the send packet
      }
  }
  else
  {
      temperatureStr += getTemperature();              //Convert the value to a string for sending
      for (uint16_t i = 0; i < temperatureStr.length(); i++)
      {
          sendbuffer[counter++] = temperatureStr.charAt(i);  //Increment through the string to add the RAW temp to the send packet
      }
  }
  
  sendbuffer[counter++] = ',';                //Add comma to seperate the data values
  randomSeed(analogRead(A3));
  int smokeDetect = random(0, 2);           //Get the RAW smoke value
  String smokeStr =  "";
  smokeStr += smokeDetect;                    //Convert the value to a string for sending
  for (uint16_t i = 0; i <= smokeStr.length(); i++)
  {
      sendbuffer[counter++] = smokeStr.charAt(i);  //Increment through the string to add the RAW temp to the send packet
  }
  //sendbuffer[counter++] = ',';                //Add comma to seperate the data values
  
  //===================================================================================================
  //Send packet to receiving Node
  //===================================================================================================
  if (USEACK)   //If the user want to use acknowledgements then run this code
  {
    //If the radio receives an acknowledgement then print a msg received msg and blink the led briefly
    if (radio.sendWithRetry(TO_NODE_ID, sendbuffer, counter/*sendLength*/))
    {
      lcd_print("Msg Received !", 0, 1);
    }
    else
    {
      lcd_print("no ACK received", 0, 1);
    }
  }
  else // don't use ACK
  {
    radio.send(TO_NODE_ID, sendbuffer, sendlength);
  }
}

//===================================================================================================
//Function: getTemperature
//This function is used to sample, signal condition and return readings from a TMP36 
//temperature sensor
//===================================================================================================
double getTemperature()
{
    double averageTemp = 0;
    int samples = 1000;
    for(uint16_t i = 0; i < samples; i++)
    {
        int a0 = analogRead(A0);                    //Get the RAW temperature value
        float temperature = (((float)a0/1024)*3.3); //Convert to voltage
        temperature -= 0.500;                       //Remove offset
        temperature /= 0.012;                       //Scale voltage to temp output
        averageTemp += temperature;
    }
    averageTemp /= samples;
    return averageTemp;
}
