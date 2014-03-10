#include <iostream>
#include <cstdio>
#include <stdio.h>
#include <unistd.h>
#include <my_global.h>
#include <mysql.h>
#include <ctime>
#include "arduPi.h"

/*********************************************************
 *  IF YOUR ARDUINO CODE HAS OTHER FUNCTIONS APART FROM  *
 *  setup() AND loop() YOU MUST DECLARE THEM HERE        *
 * *******************************************************/

/**************************
 * YOUR ARDUINO CODE HERE *
 * ************************/

union UShort
{
  unsigned short value;
  unsigned char bytes[2];
};

union UInt
{
  unsigned int value;
  unsigned char bytes[4];
};

union UFloat
{
  float value;
  unsigned char bytes[4];
};

//global variables
unsigned char checksum;
FILE *pOut;

unsigned char getByte(bool updateChecksum = true)
{
  unsigned char buffer = Serial.read();
  if(pOut != NULL)
    fputc(buffer, pOut);
  if(0x7E == buffer)
  {
    checksum = 0;
  }
  else if(0x7D == buffer)
  {
    buffer = Serial.read();
    if(pOut != NULL)
      fputc(buffer, pOut);
    buffer ^= 0x20;
  }
  if(updateChecksum)
  {
    checksum += buffer;
  }
  fflush(pOut);
  return buffer;
}

void setup()
{
  Serial.begin(9600);
  pOut = fopen("/home/pi/projects/eventLogger/out.txt", "w");
}

void loop()
{
  //printf("MySQL client version: %s\n", mysql_get_client_info());
  //printf("%d\n", Serial.available());
  while(Serial.available())
  {
    if(0x7E == getByte(false))
    {
      unsigned char lengthMSB = getByte(false);
      unsigned char lengthLSB = getByte(false);
      UShort msgLength;
      msgLength.bytes[1] = lengthMSB;
      msgLength.bytes[0] = lengthLSB;

      unsigned char frameType = getByte();
      //printf("Incoming msg frameType 0x%X and length %d bytes\n", frameType, msgLength.value);

      if(0x90 == frameType)
      {
        UInt senderH, senderL;
        for(int i = 3; i >= 0; --i)
        {
          senderH.bytes[i] = getByte();
        }
        for(int i = 3; i >= 0; --i)
        {
          senderL.bytes[i] = getByte();
        }
        //printf("sender 64 bit address H = %X L = %X\n", senderH.value, senderL.value);

        UShort senderMY;
        senderMY.bytes[1] = getByte();
        senderMY.bytes[0] = getByte();
        //printf("sender 16 bit address = %X\n", senderMY.value);

        unsigned char receiveOptions = getByte();

        //substract msglength with 64, 16 bit address and receive options
        unsigned char payload[msgLength.value - 12];
        //printf("payload length = %d\n", msgLength.value - 12);
        for(int i = 0; i < msgLength.value - 12; ++i)
        {
          payload[i] = getByte();
        }

        unsigned char msgChecksum = getByte(false);
        //printf("calculated checksum = 0x%X / msg checksum = 0x%X\n", 0xFF - checksum, msgChecksum);
        if(0xFF - checksum == msgChecksum)
        {
          //msg ok : parse payload
          UFloat temperature, milliVolt;
          unsigned char device, eventType, objectState;
          eventType = payload[0];
          if(eventType == 0x1)
          {
            temperature.bytes[0] = payload[1];
            temperature.bytes[1] = payload[2];
            temperature.bytes[2] = payload[3];
            temperature.bytes[3] = payload[4];
            milliVolt.bytes[0] = payload[5];
            milliVolt.bytes[1] = payload[6];
            milliVolt.bytes[2] = payload[7];
            milliVolt.bytes[3] = payload[8];
            device = payload[9];
            printf("device = %hhu / temp = %f / mV = %f\n", device, temperature.value, milliVolt.value);
          }
          else if(eventType == 0x2)
          {
			objectState = payload[1];
			printf("deur kot state = %hhd\n", objectState);
		  }

          //connect to db
          MYSQL *pConn = mysql_init(NULL);
          if(!pConn)
          {
            printf("Unable to init DB connection, give up and try next iteration\n");
            return;
          }
          if(!mysql_real_connect(pConn, "localhost", "eventlogger", "***", "events", 0, NULL, 0))
          {
            printf("Unable connect to DB, give up and try next iteration\n");
            return;
          }
          //construct sql query
          char sqlStmt[200];
          if(eventType == 0x1)	//periodieke meetwaarden
          {
			sprintf(sqlStmt, "insert into eventLog(event_type, value_int1, value_float1, value_float2, mod_timestamp) values (%hhu, %hhu, %f, %f, now())", eventType, device, milliVolt.value, temperature.value);
		  }
		  else if(eventType == 0x2)	//deur kot state changed
		  {
		    sprintf(sqlStmt, "insert into eventLog(event_type, value_int1, mod_timestamp) values (%hhu, %hhu, now())", eventType, objectState);  
		  }
          if(mysql_query(pConn, sqlStmt))
          {
            printf("Failed to insert values into table events, try next one...\n");
          }
          mysql_close(pConn);
        }
      }
      //std::cout << std::endl;
    }
  }
  usleep(1000000);
}

int main ()
{
	setup();
	while(1){
		loop();
	}
	return 0;
}
