#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "NTP.h"                /*Network Time Protocol (required to get time and date from a timeserver somewhere on the web*/
#include "YoutubeApi.h"     /*Read YouTube Channel statistics from the YouTube API. Written by Brian Lough https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA */
#include "font_table.h"     /*the font for the display*/

#define API_KEY "<place_your_own_YouTube_API-KEY_here>"  /*key for accessing youtube*/
#define CHANNEL_ID "<place_your_own_CHANNEL-ID_here>" /*channel-id for your own channel*/

/*
 you can test if the above works by pasting it inside this URL and use it in a browser: 

 Make sure the following URL works for you in your browser (Change the key at the end!):
 https://www.googleapis.com/youtube/v3/channels?part=statistics&id=<CHANNEL_ID>&key=<API_KEY>
 */

/*------------------------------------------*/
/*           IO-pin definitions             */
/*------------------------------------------*/
#define TXD               1
#define RXD               3

#define STEP_0            5
#define STEP_1            4
#define STEP_2            0
#define STEP_3            2

#define SHIFT_REG_OE      12
#define SHIFT_REG_DAT     13
#define SHIFT_REG_CLK     14
#define SHIFT_REG_STCLK   16

#define UNUSED_IO         15 
/*------------------------------------------*/

#define REVERSE       0   /*this is an offset in the stepper table*/
#define FORWARD       1   /*this is an offset in the stepper table*/
#define FAST          3
#define SLOW          5
#define DISPLAYSIZE   16  /*the width size of the display in characters (each char=6 pixels wide)*/
#define FONTHEIGHT    40  /*the height of a dot, this value determines that charactersize*/
#define CHARHEIGHT    9   /*the number of pixels that a char is heigh  (8dots+empty dot for spacing)*/

//#define SPICLOCKFREQ  1000000  /*1MHz = the frequency of the SPI clock*/

/*============================================================================*/

void PrintString(void);
void CharDataShifter(unsigned char data);
void MoveStepper(unsigned int steps, unsigned char direction, unsigned char speed);
void DisableStepperMotor(void);
void DataShifter(unsigned char data);
void LEDtest(void);

/*----------------------------------------------------------------------------*/

const char* ssid = "Wifi_network";
const char* password = "wifi_key";

char data_string[DISPLAYSIZE+1]; /*defining the string as global makes the programmers life a little easier...*/

WiFiClientSecure client;
YoutubeApi api(API_KEY, client);

void setup()
{ 
  unsigned char timeout;
    
  pinMode(UNUSED_IO, OUTPUT);
  digitalWrite(UNUSED_IO, HIGH);

  pinMode(SHIFT_REG_DAT, OUTPUT);
  digitalWrite(SHIFT_REG_DAT, HIGH);
  pinMode(SHIFT_REG_CLK, OUTPUT);
  digitalWrite(SHIFT_REG_CLK, HIGH); 
  pinMode(SHIFT_REG_OE, OUTPUT);
  digitalWrite(SHIFT_REG_OE, HIGH);
  pinMode(SHIFT_REG_STCLK, OUTPUT);
  digitalWrite(SHIFT_REG_STCLK, HIGH);

  pinMode(STEP_0, OUTPUT);
  digitalWrite(STEP_0, LOW); 
  pinMode(STEP_1, OUTPUT);
  digitalWrite(STEP_1, LOW); 
  pinMode(STEP_2, OUTPUT);
  digitalWrite(STEP_2, LOW); 
  pinMode(STEP_3, OUTPUT);
  digitalWrite(STEP_3, LOW); 

  pinMode(A0, INPUT);                           /*set ADC input to external channel*/

  //SPI.beginTransaction(IOexpander_SPI_settings); /*The SPI is configured to use the clock, data order (MSBFIRST or LSBFIRST) and data mode (SPI_MODE0, SPI_MODE1, SPI_MODE2, or SPI_MODE3). The clock speed is the max. speed the device can accept*/
  //SPI.setFrequency(SPICLOCKFREQ);
  //SPI.endTransaction();

  
  Serial.begin(115200);
  Serial.println();
  
  Serial.println("\r\n--== Glow In The Dark Display ==--");
  Serial.println("Firmware version:" __DATE__ );
  Serial.print("PGM flash size =");
  Serial.println(ESP.getFlashChipSize()); /*this will only give the correct results if the proper settings have been made. To do so simply select the proper board in the Arduino IDE: Tools -> board*/
  Serial.print("Free heap size ="); /*show available RAM*/
  Serial.println(ESP.getFreeHeap()); /*show available RAM*/

  //LEDtest();  /*drive all LED's to make it easy to check if they are all working*/  

  Serial.print("connecting to ");
  Serial.println(ssid);
 
  WiFi.disconnect();                /*these two lines make the system connect a lot better/faster to the network. If left out it sometimes never connects...*/
  WiFi.persistent(false);           /*Do not memorise new wifi connections*/
  //ESP.flashEraseSector(0x3fe); // Erase remembered connection info. (Only do this once).
  WiFi.begin(ssid, password);

  timeout = 30; /*timeout time, if we can't connect to the network, just give up*/
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
    if(timeout-- == 0)
    {
      while(1)
      {
        MoveStepper(3.6*CHARHEIGHT*FONTHEIGHT, REVERSE, FAST);  /*move roll to start previous position*/
        sprintf(data_string,"  ... --- ...   ");
        PrintString();             
        sprintf(data_string," no  connection ");
        PrintString(); 
        MoveStepper(1.6*CHARHEIGHT*FONTHEIGHT, FORWARD, FAST);   /*move the roll into a visible position*/
        DisableStepperMotor();
        delay(30000);
      }
    }
  }
  
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());

  sprintf(data_string,"IP address:     ");
  PrintString();       
  sprintf(data_string,"%03d.%03d.%03d.%03d", WiFi.localIP()[0],WiFi.localIP()[1],WiFi.localIP()[2],WiFi.localIP()[3]);  /*the IP address is not a string, but a sort of array, so here we copy the value into a printable format*/
  PrintString();       
  MoveStepper(1.6*CHARHEIGHT*FONTHEIGHT, FORWARD, FAST); 
  DisableStepperMotor();

  NTP_init("time.nist.gov", 60*60);  /*initialize NTP functionality (in order to get time and date from a timeserver*/
  NTP_statemachine();           /*get or update the time*/  
}


void loop()
{
  unsigned char lp;
  unsigned long subscr = 0;
  unsigned long views = 0;
  unsigned char minutes = 0;

  while(1)
  {
    yield();    /*pet the watchdog*/
    NTP_statemachine();           /*get and/or update the time*/  
      
    if(NTP_struct.minute != minutes) /*do something every minute*/
    {
      minutes = NTP_struct.minute;
      Serial.println("calling api");
      if(api.getChannelStatistics(CHANNEL_ID))
      {
        Serial.print("Subscriber Count: ");
        Serial.println(api.channelStats.subscriberCount);
        subscr = api.channelStats.subscriberCount; 
        
        Serial.print("View Count: ");
        Serial.println(api.channelStats.viewCount);
        views = api.channelStats.viewCount;
        
    //    Serial.print("Comment Count: ");
    //    Serial.println(api.channelStats.commentCount);
    //    Serial.print("Video Count: ");
    //    Serial.println(api.channelStats.videoCount);
      }
    }

      
    if((NTP_struct.second % 30) == 0) /*do something every .. seconds*/
    {
      MoveStepper(3.6*CHARHEIGHT*FONTHEIGHT, REVERSE, FAST);  /*move roll to start previous position*/

      if(views > 0) /*only print subscribers and views, when this info is available*/
      {
        sprintf(data_string,"S:%4ld  V:%6ld",subscr,views);
      }
      else
      { /*youtube info not available, print something else*/
        sprintf(data_string,"S:      V:");    
      }
      PrintString();

      if(NTP_struct.year > 0)
      {
        sprintf(data_string,"%04d-%02d-%02d %02d:%02d",NTP_struct.year, NTP_struct.month,NTP_struct.day,NTP_struct.hour,NTP_struct.minute);            
      }
      else
      { /*time not available, print something else*/
        sprintf(data_string,"    -  -     :",NTP_struct.year, NTP_struct.month,NTP_struct.day,NTP_struct.hour,NTP_struct.minute);                            
      }
      PrintString();
  
      MoveStepper(1.6*CHARHEIGHT*FONTHEIGHT, FORWARD, FAST);   /*move the roll into a visible position*/
      DisableStepperMotor();
    }
  }
}




////############################################################################

void PrintString(void)
{
    unsigned char lp;   /*counter for the lines of the character*/
    unsigned char c;    /*counter for the string*/
    unsigned char data;
    uint pntr;
    unsigned char eos;  /*end of string*/
        
    for(lp=0;lp<8;lp++)     /*the horizontal lines*/
    {
        eos = false;
        for(c=0;c<DISPLAYSIZE;c++)   /*scan the string*/
        {
            {
                data = data_string[c];
                if(data == 0)
                {
                    eos = true; /*end of string detected*/
                }
                
                if((eos == true) || (lp >=7)) /*when the string has ended or when we are done processing the string (which consists of 7 lines)*/
                {
                    CharDataShifter(0x00);      /*all LEDs off*/
                }
                else
                {
                    data = data - 0x20;         /*correct for the table missing the first 32 chars of the real ASCII table*/
                    pntr = (data * 7) + lp;     /*7 is the number of bytes used by a character in the table*/
                    CharDataShifter(pgm_read_byte(font+pntr));    /*the data send to the shift register*/
                }             
            }
        }
        /*all data has been shifted to the latches, now send the data from the latch to the LED*/
        delayMicroseconds(50);              /*a small delay*/
        digitalWrite(SHIFT_REG_STCLK, LOW); /*storage register clock*/
        delayMicroseconds(50);              /*a small delay*/
        digitalWrite(SHIFT_REG_STCLK, HIGH);  /*storage register clock*/       

        digitalWrite(SHIFT_REG_OE, LOW);  /*enable outputs*/       
        MoveStepper(FONTHEIGHT, FORWARD, SLOW); /*move the roll so that this line will be written to the roll*/    
        digitalWrite(SHIFT_REG_OE, HIGH);  /*disable outputs*/       
    }     
    MoveStepper(FONTHEIGHT, FORWARD, SLOW); /*an empty line for spacing*/    
}

/*drive the shiftregister that drives the LEDs (use only ..bits of a byte otherwise the char would be too wide)*/
void CharDataShifter(unsigned char data)
{
    unsigned char lp;
    
    for(lp=6;lp>0;lp--) /*characters that are 6 bits wide*/
    {   
        if(data & 0x01)
        {
          digitalWrite(SHIFT_REG_DAT, HIGH);
        }
        else
        {
          digitalWrite(SHIFT_REG_DAT, LOW);
        }
        
        data = data >> 1;           /*shift data 1 bit*/       
        //Shiftreg_DS_Write((data & 0x80)>>7);
        //data = data << 1;           /*shift data 1 bit*/       
        
        delayMicroseconds(50);              /*a small delay*/
        digitalWrite(SHIFT_REG_CLK, LOW);   /*shift register clock*/
        delayMicroseconds(50);              /*a small delay*/
        digitalWrite(SHIFT_REG_CLK, HIGH);  /*shift register clock*/
    }     
}

/*drive the stepper motor that moves the roller, use the "half-step" coil driving pattern*/
void MoveStepper(unsigned int steps, unsigned char dir, unsigned char spd)
{
  static unsigned char state = 0;
  unsigned int lp;
    
  for(lp=steps;lp>0;lp--) /*number of sequences*/
  {
    if(dir == FORWARD)
    {
      state++;
      if(state>7)
      {
        state = 0;
      }
    }
    else
    {
      if(state>0)
      {
        state--;
      }
      else
      {
        state = 7;
      }
    }

    //Serial.print("state=");
    //Serial.println(state);

    switch(state)
    {
      case 0: {digitalWrite(STEP_3, LOW);  digitalWrite(STEP_2, LOW);  digitalWrite(STEP_1, HIGH); digitalWrite(STEP_0, LOW);  break;}        /*2*/
      case 1: {digitalWrite(STEP_3, LOW);  digitalWrite(STEP_2, HIGH); digitalWrite(STEP_1, HIGH); digitalWrite(STEP_0, LOW);  break;}        /*6*/
      case 2: {digitalWrite(STEP_3, LOW);  digitalWrite(STEP_2, HIGH); digitalWrite(STEP_1, LOW);  digitalWrite(STEP_0, LOW);  break;}        /*4*/
      case 3: {digitalWrite(STEP_3, LOW);  digitalWrite(STEP_2, HIGH); digitalWrite(STEP_1, LOW);  digitalWrite(STEP_0, HIGH); break;}        /*5*/                        
      case 4: {digitalWrite(STEP_3, LOW);  digitalWrite(STEP_2, LOW);  digitalWrite(STEP_1, LOW);  digitalWrite(STEP_0, HIGH); break;}        /*1*/
      case 5: {digitalWrite(STEP_3, HIGH); digitalWrite(STEP_2, LOW);  digitalWrite(STEP_1, LOW);  digitalWrite(STEP_0, HIGH); break;}        /*9*/
      case 6: {digitalWrite(STEP_3, HIGH); digitalWrite(STEP_2, LOW);  digitalWrite(STEP_1, LOW);  digitalWrite(STEP_0, LOW);  break;}        /*8*/
      case 7: {digitalWrite(STEP_3, HIGH); digitalWrite(STEP_2, LOW);  digitalWrite(STEP_1, HIGH); digitalWrite(STEP_0, LOW);  break;}        /*10*/        
      default:{digitalWrite(STEP_3, LOW);  digitalWrite(STEP_2, LOW);  digitalWrite(STEP_1, LOW);  digitalWrite(STEP_0, LOW);  break;}        /*ALL COILS OFF*/
    }
    
    delay(spd);
  }   
}

/*motor OFF to save power*/
void DisableStepperMotor(void)
{
  digitalWrite(STEP_0, LOW); 
  digitalWrite(STEP_1, LOW); 
  digitalWrite(STEP_2, LOW); 
  digitalWrite(STEP_3, LOW); 
}


/*drive the shiftregister that drives the LEDs use all 8 bits*/
void DataShifter(unsigned char data)
{
  unsigned char lp;
  
  for(lp=8;lp>0;lp--) /*characters that are 6 bits wide*/
  {       
    if(data & 0x01)
    {
      digitalWrite(SHIFT_REG_DAT, HIGH);
    }
    else
    {
      digitalWrite(SHIFT_REG_DAT, LOW);
    }

    data = data >> 1;           /*shift data 1 bit*/       
      
    delayMicroseconds(50);              /*a small delay*/
    digitalWrite(SHIFT_REG_CLK, LOW);   /*shift register clock*/
    delayMicroseconds(50);              /*a small delay*/
    digitalWrite(SHIFT_REG_CLK, HIGH);  /*shift register clock*/
  }
}

void LEDtest(void)
{   
  unsigned char lp;   /*counter for the lines of the character*/    
  /*first reset the buffer by shifting in 0's*/

  Serial.println("starting LED-test (also tests PSU, becasue this draws max. current)");
  
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/
  
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/
  
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/
  DataShifter(0xff);      /*all LEDs on*/

  /*all data has been shifted to the latches, now send the data from the latch to the LED*/
  delayMicroseconds(100);               /*a small delay*/
  digitalWrite(SHIFT_REG_STCLK, LOW);   /*storage register clock*/
  delayMicroseconds(100);               /*a small delay*/
  digitalWrite(SHIFT_REG_STCLK, HIGH);  /*storage register clock*/  

  digitalWrite(SHIFT_REG_OE, LOW);      /*enable outputs*/       
  MoveStepper(10, FORWARD, FAST);       /*move the roll so that this line will be written to the roll*/  

  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/
  
  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/
  
  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/
  DataShifter(0x00);      /*all LEDs on*/

 /*all data has been shifted to the latches, now send the data from the latch to the LED*/
  delayMicroseconds(100);               /*a small delay*/
  digitalWrite(SHIFT_REG_STCLK, LOW);   /*storage register clock*/
  delayMicroseconds(100);               /*a small delay*/
  digitalWrite(SHIFT_REG_STCLK, HIGH);  /*storage register clock*/  

  Serial.println("starting LED-test (one at a time)");
           
  /*shift 1 a single 1, make that shift through all, therefore only one LED is active during each step*/
  digitalWrite(SHIFT_REG_DAT, HIGH);  /*the 1, that's shifted inwards*/
  for(lp=0;lp<96;lp++)     /*the number of horizontal lines*/
  {
    yield();  /*pet the watchdog*/
    delayMicroseconds(50);              /*a small delay*/
    digitalWrite(SHIFT_REG_CLK, LOW);   /*shift register clock*/
    delayMicroseconds(50);              /*a small delay*/
    digitalWrite(SHIFT_REG_CLK, HIGH);  /*shift register clock*/        

    /*all data has been shifted to the latches, now send the data from the latch to the LED*/
    delayMicroseconds(50);              /*a small delay*/
    digitalWrite(SHIFT_REG_STCLK, LOW); /*storage register clock*/
    delayMicroseconds(50);              /*a small delay*/
    digitalWrite(SHIFT_REG_STCLK, HIGH);  /*storage register clock*/ 
    
    digitalWrite(SHIFT_REG_OE, LOW);    /*enable outputs*/       
    MoveStepper(10, FORWARD, SLOW);      /*move the roll so that this line will be written to the roll*/
    digitalWrite(SHIFT_REG_OE, HIGH);   /*disable outputs*/       
    digitalWrite(SHIFT_REG_DAT, LOW);   /*the zero(s) that follow...*/    
  }       
  MoveStepper(50, FORWARD, FAST);       /*move the roll so that this line will be written to the roll*/  
  Serial.println("LED-test done");       
}
