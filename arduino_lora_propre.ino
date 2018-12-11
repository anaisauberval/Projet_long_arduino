#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_ADS1015.h> 
#include <SPI.h> 
#include "SX1272.h" 

#define ONE_WIRE_BUS_1 14
#define ONE_WIRE_BUS_2 7

#define ETSI_EUROPE_REGULATION
#define BAND868

#define MAX_DBM 14 // Relatif à ETSI_EUROPE_REGULATION
const uint32_t DEFAULT_CHANNEL=CH_10_868; //Relatif à BAND868

// uncomment if your radio is an HopeRF RFM92W, HopeRF RFM95W, Modtronix inAir9B, NiceRF1276
// or you known from the circuit diagram that output use the PABOOST line instead of the RFO line
#define PABOOST

#define WITH_APPKEY
//if you are low on program memory, comment STRING_LIB to save about 2K
#define STRING_LIB
#define LOW_POWER
#define LOW_POWER_HIBERNATE
#define WITH_ACK

// CHANGE HERE THE LORA MODE, NODE ADDRESS 
#define LORAMODE  1
#define node_addr 6

// CHANGE HERE THE TIME IN MINUTES BETWEEN 2 READING & TRANSMISSION
unsigned int idlePeriodInMin = 2;

#ifdef WITH_APPKEY
///////////////////////////////////////////////////////////////////
// CHANGE HERE THE APPKEY, BUT IF GW CHECKS FOR APPKEY, MUST BE
// IN THE APPKEY LIST MAINTAINED BY GW.
uint8_t my_appKey[4]={5, 6, 7, 8};
///////////////////////////////////////////////////////////////////
#endif

///////////////////////////////////////////////////////////////////
// IF YOU SEND A LONG STRING, INCREASE THE SIZE OF MESSAGE
uint8_t message[50];
///////////////////////////////////////////////////////////////////

#define PRINTLN                   Serial.println("")
#define PRINT_CSTSTR(fmt,param)   Serial.print(F(param))
#define PRINT_STR(fmt,param)      Serial.print(param)
#define PRINT_VALUE(fmt,param)    Serial.print(param)
#define FLUSHOUTPUT               Serial.flush();

#define DEFAULT_DEST_ADDR 1

#ifdef WITH_ACK
#define NB_RETRIES 2
#endif

#define TEMP_SCALE  5000.0

#define LOW_POWER_PERIOD 8
// you need the LowPower library from RocketScream
// https://github.com/rocketscream/Low-Power
#include "LowPower.h"
unsigned int nCycle = idlePeriodInMin*60/LOW_POWER_PERIOD;

unsigned long nextTransmissionTime=0L;
int loraMode=LORAMODE;

OneWire oneWire_in(ONE_WIRE_BUS_1);
OneWire oneWire_out(ONE_WIRE_BUS_2);

DallasTemperature sensor_inhouse(&oneWire_in);
DallasTemperature sensor_outhouse(&oneWire_out);

Adafruit_ADS1015 ads1015;

void setup() {
  int e;

  Serial.begin(38400);  

  // Print a start message
  PRINT_CSTSTR("%s","Simple LoRa temperature sensor\n");

  #ifdef __AVR_ATmega328P__
    PRINT_CSTSTR("%s","ATmega328P detected\n");
  #endif 

  // Power ON the module
  sx1272.ON();

    // Set transmission mode and print the result
  e = sx1272.setMode(loraMode);
  PRINT_CSTSTR("%s","Setting Mode: state ");
  PRINT_VALUE("%d", e);
  PRINTLN;

    // enable carrier sense
  sx1272._enableCarrierSense=true;
  #ifdef LOW_POWER
    // TODO: with low power, when setting the radio module in sleep mode
    // there seem to be some issue with RSSI reading
   sx1272._RSSIonSend=false;
  #endif   

     // Select frequency channel
  e = sx1272.setChannel(DEFAULT_CHANNEL);
  PRINT_CSTSTR("%s","Setting Channel: state ");
  PRINT_VALUE("%d", e);
  PRINTLN;

  sx1272._needPABOOST=true;

    e = sx1272.setPowerDBM((uint8_t)MAX_DBM);
  PRINT_CSTSTR("%s","Setting Power: state ");
  PRINT_VALUE("%d", e);
  PRINTLN;

    // Set the node address and print the result
  e = sx1272.setNodeAddress(node_addr);
  PRINT_CSTSTR("%s","Setting node addr: state ");
  PRINT_VALUE("%d", e);
  PRINTLN;

  // Set CRC off
  //e = sx1272.setCRC_OFF();
  //PRINT_CSTSTR("%s","Setting CRC off: state ");
  //PRINT_VALUE("%d", e);
  //PRINTLN;
    
  // Print a success message
  PRINT_CSTSTR("%s","SX1272 successfully configured\n");

   sensor_inhouse.begin();
   sensor_outhouse.begin();

   ads1015.setGain(GAIN_SIXTEEN);
   ads1015.begin();

  delay(500);  
}

#ifndef STRING_LIB

char *ftoa(char *a, double f, int precision)
{
 long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};
 
 char *ret = a;
 long heiltal = (long)f;
 itoa(heiltal, a, 10);
 while (*a != '\0') a++;
 *a++ = '.';
 long desimal = abs((long)((f - heiltal) * p[precision]));
 if (desimal < p[precision-1]) {
  *a++ = '0';
 } 
 itoa(desimal, a, 10);
 return ret;
}

#endif

void loop() {
  // put your main code here, to run repeatedly:
  long startSend;
  long endSend;
  uint8_t app_key_offset=0;
  int e;
  float temp;
  int16_t results;
  int16_t results_w;
  
  #ifndef LOW_POWER
    // 600000+random(15,60)*1000
    //if (millis() > nextTransmissionTime) {
  #endif

   temp = 0.0;
   float temp_1 = 0.0; 
   int value;
      

   Serial.print("Requesting temperatures...");

   sensor_inhouse.requestTemperatures();
   sensor_outhouse.requestTemperatures();

   results = ads1015.readADC_Differential_0_1();
   results_w = results * 0.125 * 0.1;

   temp = sensor_inhouse.getTempCByIndex(0);
   temp_1 = sensor_outhouse.getTempCByIndex(0);
      
   //temp = value ;
   Serial.println();
   Serial.println (" TEMPERATURES ENVOYEES : ");
   Serial.print(temp);Serial.println(temp_1);
   Serial.println();
   Serial.println (" LUMINOSITE ENVOYEE : ");
   Serial.println(results);Serial.println(" En W/m2 : "); Serial.println(results_w);

   PRINT_CSTSTR("%s","Mean temp is ");
   // temp = temp/5;
   PRINT_VALUE("%f ", temp);
   PRINT_VALUE("%f ", temp_1);
   PRINTLN;

  #ifdef WITH_APPKEY
        app_key_offset = sizeof(my_appKey);
        // set the app key in the payload
        memcpy(message,my_appKey,app_key_offset);
  #endif

  uint8_t r_size;
      
  //r_size=sprintf((char*)message+app_key_offset,"\\!#%d#TC/%s %s %s",field_index,String(temp).c_str(),String(temp_1).c_str(),String(results_w).c_str());
   r_size=sprintf((char*)message+app_key_offset,"\\%f %f %f",temp,temp_1,results_w);

         PRINT_CSTSTR("%s","Sending ");
      PRINT_STR("%s",(char*)(message+app_key_offset));
      PRINTLN;
      
      PRINT_CSTSTR("%s","Real payload size is ");
      PRINT_VALUE("%d", r_size)
      PRINTLN;
      
      int pl=r_size+app_key_offset;
      
      sx1272.CarrierSense();
      
      startSend=millis();

      sx1272.setPacketType(PKT_TYPE_DATA | PKT_FLAG_DATA_WAPPKEY);

      #ifdef WITH_ACK
      int n_retry=NB_RETRIES;
      
      do {
        e = sx1272.sendPacketTimeoutACK(DEFAULT_DEST_ADDR, message, pl);

        if (e==3)
          PRINT_CSTSTR("%s","No ACK");
        
        n_retry--;
        
        if (n_retry)
          PRINT_CSTSTR("%s","Retry");
        else
          PRINT_CSTSTR("%s","Abort"); 
          
      } while (e && n_retry);    
      #endif

      endSend=millis();

            PRINT_CSTSTR("%s","LoRa pkt size ");
      PRINT_VALUE("%d", pl);
      PRINTLN;
      
      PRINT_CSTSTR("%s","LoRa pkt seq ");
      PRINT_VALUE("%d", sx1272.packet_sent.packnum);
      PRINTLN;
    
      PRINT_CSTSTR("%s","LoRa Sent in ");
      PRINT_VALUE("%ld", endSend-startSend);
      PRINTLN;
          
      PRINT_CSTSTR("%s","LoRa Sent w/CAD in ");
      PRINT_VALUE("%ld", endSend-sx1272._startDoCad);
      PRINTLN;

      PRINT_CSTSTR("%s","Packet sent, state ");
      PRINT_VALUE("%d", e);
      PRINTLN;

      PRINT_CSTSTR("%s","Remaining ToA is ");
      PRINT_VALUE("%d", sx1272.getRemainingToA());
      PRINTLN;

      PRINT_CSTSTR("%s","Switch to power saving mode\n");

      e = sx1272.setSleepMode();

      if (!e)
        PRINT_CSTSTR("%s","Successfully switch LoRa module in sleep mode\n");
      else  
        PRINT_CSTSTR("%s","Could not switch LoRa module in sleep mode\n");
        
      FLUSHOUTPUT

      delay(10000);

      nCycle = idlePeriodInMin*60/LOW_POWER_PERIOD; //+ random(2,4);

      for (int i=0; i<nCycle; i++) {  
        // ATmega328P, ATmega168, ATmega32U4
          LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

          // use the delay function
          delay(LOW_POWER_PERIOD*1000);

          PRINT_CSTSTR("%s",".");
          FLUSHOUTPUT
          delay(10);  
      }

      delay(50);
    
}
