#include <dht11.h>
#include "temp.h"


dht11 DHT11;
#define DHT11PIN  DHT11_Data
void GetTempHumi(int *Temp,int *Humi)
{
 int chk = DHT11.read(DHT11PIN);
  switch (chk)
  {
    case DHTLIB_OK: 
      //          Serial.println("OK"); 
                break;
    case DHTLIB_ERROR_CHECKSUM: 
       //         Serial.println("Checksum error"); 
                break;
    case DHTLIB_ERROR_TIMEOUT: 
       //         Serial.println("Time out error"); 
                break;
    default: 
        //        Serial.println("Unknown error"); 
                break;
  }
  *Temp = DHT11.temperature;
  *Humi = DHT11.humidity;
/*   Serial.println(DHT11.temperature,DEC);
   Serial.println(DHT11.humidity,DEC);  */
}




