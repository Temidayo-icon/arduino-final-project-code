
// EmonLibrary examples openenergymonitor.org, Licence GNU GPL V3
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "EmonLib.h"                   // Include Emon Library
#include <SoftwareSerial.h>

#define ANALOG_IN_PIN A1

SoftwareSerial mySerial = SoftwareSerial(8,7);
EnergyMonitor emon1;                   // Create an instance

String apn = "web.gprs.mtnnigeria.net"; //APN
String apn_u = "";                     //APN-Username
String apn_p = "";                    //APN-Password
String url = "http://dayofinalproject.herokuapp.com/api/post/";  //URL for HTTP-POST-REQUEST
String json;   //String for the json Paramter (e.g. jason)


// Set the pins for the relays
int relay1Pin = 2;
int relay2Pin = 3;
int relay3Pin = 4;
int relay4Pin = 5;

int load1, load2, load3, load4;

double units = 10; // IN UNITS IN WH
int measurements = 60;
int numMeasurements = 0;
int kwmUsed = 0;
float kw = 0;
int instantaneousPower = 0;
float initial_current =0.0;
bool first_run = true;

String phone_no1 = "+2349159774476"; //phone number to sms
String RxString =""; //will hod the incoming string from the GSM shield
char          RxChar    = ' ';
int           Counter   = 0;
String        GSM_Nr    = "+2349159774476";
String        GSM_Msg   = "";


// Floats for ADC voltage & Input voltage
float adc_voltage = 0.0;
float in_voltage = 0.0;
float ac_voltage = 0.0;

// Floats for resistor values in divider (in ohms)
float R1 = 30000.0;
float R2 = 7500.0; 

// Float for Reference Voltage
float ref_voltage = 5.0;

// Integer for ADC value
int adc_value = 0;
int samples = 0;

void setup()
{  
  Serial.begin(9600);
  mySerial.begin(9600); // the GPRS baud rate   
  emon1.current(0, 130.1);     
  delay(300);    

   // Set the relay pins as output
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);
  pinMode(relay3Pin, OUTPUT);
  pinMode(relay4Pin, OUTPUT);

// Turn off all appliances at startup
  digitalWrite(relay1Pin, LOW);
  digitalWrite(relay2Pin, LOW);
  digitalWrite(relay3Pin, LOW);
  digitalWrite(relay4Pin, LOW);    

Serial.println("Initializing....");
initModule("AT","OK",1000);                //Scan for GSM Module
initModule("AT+CPIN?","READY",1000);       //this command is used to check whether SIM card is inserted in GSM Module or not
initModule("AT+CMGF=1","OK",1000);         //Set SMS mode to ASCII
initModule("AT+CNMI=2,2,0,0,0","OK",1000); //Set device to read SMS if available and print to serial
Serial.println("Initialized Successfully"); 

load1 = EEPROM.read(1);
load2 = EEPROM.read(2);
load3 = EEPROM.read(3);
load4 = EEPROM.read(4);

relays();

delay(100);
}

void loop()
{

  if (first_run){
    initial_current = emon1.calcIrms(1480);
  }

  first_run = false;

  double Irms = emon1.calcIrms(1480);  // Calculate Irms only
  Irms = Irms < initial_current ? 0 : Irms - initial_current;
  
  instantaneousPower = Irms*230.0;
  numMeasurements ++;
  
  kwmUsed = kwmUsed + instantaneousPower;
  
  adc_value = analogRead(ANALOG_IN_PIN);
  adc_voltage  = (adc_value * ref_voltage) / 1024.0; 
  in_voltage = adc_voltage / (R2/(R1+R2)); 
  ac_voltage = map(in_voltage, 0, 15.27, 0, 230);

  kw = Irms * ac_voltage;
  
  Serial.print("AC Voltage = ");
  Serial.print(ac_voltage, 2);
  Serial.print("AC Current = ");
  Serial.print(Irms, 2);
  Serial.print(" w = ");
  Serial.println(kw, 2);
  

  if (samples > 10){
      postToServer(ac_voltage, Irms, kw);
      samples++;
  }

  samples++;


// scan for data from software serial port
  //-----------------------------------------------
  RxString = "";
  Counter = 0;
  while(mySerial.available()){
    delay(1);  // short delay to give time for new data to be placed in buffer
    // get new character
    RxChar = char(mySerial.read());
    //add first 200 character to string
    if (Counter < 200) {
      RxString.concat(RxChar);
      Counter = Counter + 1;
    }
  }
  // Is there a new SMS?
  //-----------------------------------------------
  if (Received(F("CMT:")) ) GetSMS();

if(GSM_Nr==phone_no1){
  
if(GSM_Msg=="load1on") {load1=HIGH; sendSMS(GSM_Nr,"Ok Load 1 is On");}
if(GSM_Msg=="load1off"){load1=LOW; sendSMS(GSM_Nr,"Ok Load 1 is Off");}

if(GSM_Msg=="load2on") {load2=HIGH; sendSMS(GSM_Nr,"Ok Load 2 is On");}
if(GSM_Msg=="load2off"){load2=LOW; sendSMS(GSM_Nr,"Ok Load 2 is Off");}

if(GSM_Msg=="load3on") {load3=HIGH; sendSMS(GSM_Nr,"Ok Load 3 is On");}
if(GSM_Msg=="load3off"){load3=LOW; sendSMS(GSM_Nr,"Ok Load 3 is Off");}

if(GSM_Msg=="load4on") {load4=HIGH; sendSMS(GSM_Nr,"Ok Load 4 is On");}
if(GSM_Msg=="load4off"){load4=LOW; sendSMS(GSM_Nr,"Ok Load 4 is Off");}

if(GSM_Msg=="allon") {load1=HIGH, load2=HIGH, load3=HIGH, load4=HIGH; sendSMS(GSM_Nr,"Ok All Load is On");}
if(GSM_Msg=="alloff"){load1=LOW, load2=LOW, load3=LOW, load4=LOW; sendSMS(GSM_Nr,"Ok All Load is Off");}


eeprom_write();
relays();
}
GSM_Nr="";
GSM_Msg="";
}

void resetMeasures(){

  kwmUsed = 0;
  numMeasurements = 0;
  
  };

void postToServer(float voltage, float current, float power){
  mySerial.println("AT");
  runsl();//Print GSM Status an the Serial Output;
  delay(4000);
  mySerial.println("AT+CCLK?"); //CHECK STATUS OF RTC ONSTARTUP SYNCING
  runsl();
  delay(100);
  mySerial.println("AT+SAPBR=3,1,Contype,GPRS");
  runsl();
  delay(100);
  mySerial.println("AT+SAPBR=3,1,APN," + apn);
  runsl();
  delay(100);
  mySerial.println("AT+SAPBR =1,1");
  runsl();
  delay(100);
  mySerial.println("AT+SAPBR=2,1");
  runsl();
  delay(2000);
  mySerial.println("AT+HTTPINIT");
  runsl();
  delay(100);
  mySerial.println("AT+HTTPPARA=CID,1");
  runsl();
  delay(100);
  mySerial.println("AT+HTTPPARA=URL," + url);
  runsl();
  delay(100);
  mySerial.println("AT+HTTPPARA=CONTENT,application/json");
  runsl();
  delay(100);
  mySerial.println("AT+HTTPDATA=192,10000");
  runsl();
  delay(100);
  
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root["power"] = power;
  root["current"] = current;
  root["voltage"] = voltage;
  
  String place_holder;
  serializeJson(doc, place_holder);
  json = place_holder;
  mySerial.println(json);
  runsl();
  
  delay(10000);
  mySerial.println("AT+HTTPACTION=1");
  runsl();
  delay(5000);
  mySerial.println("AT+HTTPREAD");
  runsl();
  delay(100);
  mySerial.println("AT+HTTPTERM");
  runsl(); 
  delay(100); 
}

//Print GSM Status
void runsl() {
  while (mySerial.available()) {
    Serial.write(mySerial.read());
  }

}

void eeprom_write(){
EEPROM.write(1,load1);
EEPROM.write(2,load2);
EEPROM.write(3,load3);
EEPROM.write(4,load4);  
}

void relays(){  
digitalWrite(relay1Pin, load1); 
digitalWrite(relay2Pin, load2); 
digitalWrite(relay3Pin, load3); 
digitalWrite(relay4Pin, load4); 
}

// Send SMS 
void sendSMS(String number, String msg){
mySerial.print("AT+CMGS=\"");mySerial.print(number);mySerial.println("\"\r\n"); //AT+CMGS=”Mobile Number” <ENTER> - Assigning recipient’s mobile number
delay(500);
mySerial.println(msg); // Message contents
delay(500);
mySerial.write(byte(26)); //Ctrl+Z  send message command (26 in decimal).
delay(5000);  
}

void GetSMS() {
  //Get SMS number
  //================================================
  GSM_Nr  = RxString;
  //get number
  int t1 = GSM_Nr.indexOf('"');
  GSM_Nr.remove(0,t1 + 1);
  t1 = GSM_Nr.indexOf('"');
  GSM_Nr.remove(t1);
   
  // Get SMS message
  //================================================
  GSM_Msg = RxString;
  t1 = GSM_Msg.indexOf('"');
  GSM_Msg.remove(0,t1 + 1);
  t1 = GSM_Msg.indexOf('"');
  GSM_Msg.remove(0,t1 + 1);
  t1 = GSM_Msg.indexOf('"');
  GSM_Msg.remove(0,t1 + 1);
  t1 = GSM_Msg.indexOf('"');
  GSM_Msg.remove(0,t1 + 1);
  t1 = GSM_Msg.indexOf('"');
  GSM_Msg.remove(0,t1 + 1);
  t1 = GSM_Msg.indexOf('"');
  GSM_Msg.remove(0,t1 + 1);
  GSM_Msg.remove(0,1);
  GSM_Msg.trim();

Serial.print("Number:"); Serial.println(GSM_Nr);
Serial.print("SMS:"); Serial.println(GSM_Msg);
}

// Search for specific characters inside RxString 
boolean Received(String S) {
  if (RxString.indexOf(S) >= 0) return true; else return false;
}
// Init GSM Module 
void initModule(String cmd, char *res, int t){
while(1){
    Serial.println(cmd);
    mySerial.println(cmd);
    delay(100);
    while(mySerial.available()>0){
       if(mySerial.find(res)){
        Serial.println(res);
        delay(t);
        return;
       }else{Serial.println("Error");}}
    delay(t);
  }
}


//void ShowSerialData()
//{
//  while(gprsSerial.available()!=0)
//  Serial.println(gprsSerial.read());
//  delay(5000); 
//}
//  
