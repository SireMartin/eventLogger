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

unsigned char getByte(bool updateChecksum = true)
{
  unsigned char buffer = Serial.read();
  if(0x7E == buffer)
  {
    checksum = 0;
  }
  else if(0x7D == buffer)
  {
    buffer = Serial.read();
    buffer ^= 0x20;
  }
  if(updateChecksum)
  {
    checksum += buffer;
  }
  return buffer;
}

void setup()
{
  Serial.begin(9600);
}

void loop()
{
  //printf("MySQL client version: %s\n", mysql_get_client_info());
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
      printf("Incoming msg frameType 0x%X and length %d bytes\n", frameType, msgLength.value);

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
        printf("sender 64 bit address H = %X L = %X\n", senderH.value, senderL.value);

        UShort senderMY;
        senderMY.bytes[1] = getByte();
        senderMY.bytes[0] = getByte();
        printf("sender 16 bit address = %X\n", senderMY.value);

        unsigned char receiveOptions = getByte();

        //substract msglength with 64, 16 bit address and receive options
        unsigned char payload[msgLength.value - 12];
        printf("payload length = %d\n", msgLength.value - 12);
        for(int i = 0; i < msgLength.value - 12; ++i)
        {
          payload[i] = getByte();
        }

        unsigned char msgChecksum = getByte(false);
        printf("calculated checksum = 0x%X / msg checksum = 0x%X\n", 0xFF - checksum, msgChecksum);
        if(0xFF - checksum == msgChecksum)
        {
          //msg ok : parse payload
          UFloat temperature, milliVolt;
          unsigned char device;
          temperature.bytes[0] = payload[0];
          temperature.bytes[1] = payload[1];
          temperature.bytes[2] = payload[2];
          temperature.bytes[3] = payload[3];
          milliVolt.bytes[0] = payload[4];
          milliVolt.bytes[1] = payload[5];
          milliVolt.bytes[2] = payload[6];
          milliVolt.bytes[3] = payload[7];
          device = payload[8];
          printf("device = %hhu / temp = %f / mV = %f\n", device, temperature.value, milliVolt.value);

          //write to db
          //-----------

          /*time_t currentTime = time(NULL);
          tm *localTime = localtime(&currentTime);
          char strLocalTime[20];
          strftime(strLocalTime, 20, "%Y-%m-%d %H:%M:%S", localTime);
          printf("%s\n", strLocalTime);*/

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
          sprintf(sqlStmt, "insert into events(msg_nr, mod_timestamp, data_seq, data_type, data) values (3, now(), 1, 'F', %f)", temperature.value);
          if(mysql_query(pConn, sqlStmt))
          {
            printf("Failed to insert values into table events, try next one...\n");
          }
          sprintf(sqlStmt, "insert into events(msg_nr, mod_timestamp, data_seq, data_type, data) values (3, now(), 2, 'F', %f)", milliVolt.value);
          if(mysql_query(pConn, sqlStmt))
          {
            printf("Failed to insert values into table events, try next one...\n");
          }
          sprintf(sqlStmt, "insert into events(msg_nr, mod_timestamp, data_seq, data_type, data) values (3, now(), 3, 'I', %hhu)", device);
          if(mysql_query(pConn, sqlStmt))
          {
            printf("Failed to insert values into table events, try next one...\n");
          }
          mysql_close(pConn);
        }
      }
    }
  }
  usleep(10000);
}

int main ()
{
	setup();
	while(1){
		loop();
	}
	return 0;
}
