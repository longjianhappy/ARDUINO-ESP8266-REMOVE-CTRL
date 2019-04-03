#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <string.h>
#include "temp.h"

//！！！！！！注意！需要安装ESP8266开发板,即Arduino IDE for ESP8266，方法详见http://www.geek-workshop.com/thread-26170-1-1.html
//V2.0,LC编写，2017/08

#define LED     2      //板载LED灯
#define RUN_LED     2      //板载LED灯
#define KEY    0   //按键
#define LED_ON  0
#define LED_OFF  1

#define PWR_LED  16
#define HDD_LED  12

#define OPEN    HIGH
#define CLOSE    LOW

#define   WIFI_LINK_NO    0x00      //未连接路由器
#define   WIFI_LINK_ING    0x01      //正在连接路由器
#define   WIFI_SMART_LINK_ING    0x02      //正在连接路由器
#define   WIFI_LINKED    0x03      //已在连接路由器

#define   LINK_SERVER_FLAG    0x10    //已连接服务器
#define   LINK_OTHER_FLAG     0x20    //

#define  LinkType_Normal    0
#define  LinkType_Reset    1
#define  LinkType_Reset    2

#define   LED_FREQ_0_5   0      //LED闪烁 0.5HZ
#define   LED_FREQ_1   1        //LED闪烁 1HZ
#define   LED_FREQ_2   2        //LED闪烁 2HZ
#define   LED_RATIO_1_4   0.25     //LED占空比
#define   LED_RATIO_1_2   0.5
#define   LED_RATIO_3_4   0.75

#define FlashSize     100      
#define EPROM_addr_Mode      0
#define FLASH_USER_START_ADDR   EPROM_addr_Mode+2
#define MyID_FlashAddr      FLASH_USER_START_ADDR     //我的ID 设备ID 10字节 
#define MyMM_FlashAddr      FLASH_USER_START_ADDR+10  //密码          16字节     
#define YouID_FlashAddr     FLASH_USER_START_ADDR+26  //目标ID1    10字节    
#define YouMM_FlashAddr     FLASH_USER_START_ADDR+36  //目标ID密码     16字节
#define Buad_FlashAddr      FLASH_USER_START_ADDR+52  //1字节

//使用通用接口

unsigned int localPort = 7005;    // 本地UDP端口
const char IP[] = "pro.fuhome.net";//未来之家服务器ip
const int port = 7005;            //未来之家服务器端口
int UARTBuad;   //串口波特率 

char ssid[] = "********";  //  WIFI名 SSID
char pass[] = "********";  // WiFi密码

char MyID[10];
char MyMM[16];
char Bid[4];//4个随机标志
char UserID[11]="0000000000";//用户ID存储

int cb;//接收数据长度
int t;//上一次发心跳包的机器运行的时间

WiFiUDP Udp;
byte packetBuffer[44];//接收数据缓冲区
unsigned char sendsj[62] = "000032A01"; //发送数据缓冲区，用于心跳包、上传数据和报警、返回信息
char sendsta[5]={0};//状态量变量
char pwrled=0;//电源灯
char hddled=0;//硬盘灯
int wendu=0,shidu=0;

char temp[36];//临时数据

unsigned long OS_NowTime = 0;
unsigned long DoTime1 = 0;
unsigned long DoTime2 = 0;
unsigned long LastTime = 0;
unsigned long JsTime=0;

char dotype=0;//1开机 2重启 3强制关机

char Timeflag_20ms = 0;
char Timeflag_100ms = 0;
char Timeflag_1s = 0;
char Timeflag_30s = 0;
char Timeflag_33s = 0;
char Timeflag_35s = 0;


typedef struct  wifistruct
{
  char *wifissid;
  char *wifikey;
  char ssidSetFlag;
  char KeySetFlag;
  char linkstatus;
  unsigned short LinkCount;//超时计数
 };

 wifistruct Mywifi;

 unsigned char WifiFastInitedFlag;

 unsigned char Timecount=0;
 unsigned char LEDfreq;    //闪烁周期
 unsigned char LEDratio;   //占空比

//初始化，ID为设备ID，MM为十六位加密密码
void csh(char* ID, char* MM)
{
  memmove(sendsj + 9, ID, 10);
  memmove(sendsj + 19, MM, 16);
}
//p：识别号，STA：数据
void senddata(char* p, char* STA)
{
  char L, L_Str[4], i, sjbz[] = "1234"; //sjbz:数据标识
  L = strlen(STA);
  if (L > 36)
  {
    return;
  }
  L += 43;
  dtostrf(L, 4, 0, L_Str);
  for (i = 0; i < 3; i++)
  {
    if (L_Str[i] == ' ')
    {
      L_Str[i] = '0';
    }
  }
  if (strcmp(p, "01") == 0)
  {
    memmove(sjbz, "1231", 4);
  }
  if (strcmp(p, "02") == 0)
  {
    memmove(sjbz, "1232", 4);
  }
  if (strcmp(p, "09") == 0)
  {
    memmove(sjbz, "1233", 4);
  }
  if (strcmp(p, "0B") == 0)
  {
    memmove(sjbz, "1234", 4);
  }
  memmove(sendsj, L_Str, 4);
  memmove(sendsj + 35, sjbz, 4);
  memmove(sendsj + 39, p, 2);
  memmove(sendsj + 41, STA, strlen(STA));
  memmove(sendsj + L - 2, "05", 2);
  Udp.begin(localPort);
  Udp.beginPacket(IP, port);
  Udp.write(sendsj, L);
  Udp.endPacket();
  
  Serial.print("SD:");
  Serial.write(sendsj, L);
  Serial.println();
}
void xtb(char* ztl)//发送心跳包
{
  senddata("01", ztl);
}

void bj(char* bjxx)//发送报警包
{
  senddata("02", bjxx);
}

//ReturnMsg函数，用于给用户返回信息，支持中文 UserID:用户ID;xx:信息
void ReturnMsg(char* UserID, char* xx)
{
  memset(temp, 0, 36);
  memmove(temp, UserID, 10);         //用户ID
  memmove(temp + 10, xx, strlen(xx));//信息
  senddata("09", temp);
}

//updata函数，用于上传数据 feelID:传感器ID;mun:数据;width:数据整数部分位数;prec:数据小数部分位数
void updata(char* UserID, char* feelID, float mun, unsigned char width, unsigned char prec)
{
  char data[10];//数据
  char L;
  char L_Str[] = "0"; //数据长度
  memset(temp, 0, 36);
  memset(data, 0, 10);
  dtostrf(mun, width, prec, data);
  L = strlen(data);
  dtostrf(L, 1, 0, L_Str);
  memmove(temp, UserID, 10);//用户ID
  temp[10] = '1';           //传感器类型
  memmove(temp + 11, feelID, 3); //编号
  temp[14] = L_Str[0];
  memmove(temp + 15, data, L); //数值
  senddata("0B", temp);
}
void in()//进入UDP处理函数
{
  unsigned char i=0;
  char com;//数据包类型
  //char UserID[11] = "0000000000"; //发命令的用户ID
  char ID[11] = "0000000000"; //要控制的设备ID
  char doit=0;//1执行，0是重复的
  char Bidtmp[4]={0};//临时bid
  char msg[30];//信息
  memset(packetBuffer, 0, 44);
  Udp.read(packetBuffer, cb);
  com = packetBuffer[8];


   Serial.print("RC:");
   Serial.write(packetBuffer, cb);
   Serial.println();
       
   
  switch (com)//判断数据包类型
  {
    case '1':
       Mywifi.linkstatus = LINK_SERVER_FLAG;   //心跳包有回复，已连上服务器
       Serial.println("LINK_SERVER_FLAG");
       Mywifi.LinkCount = 0;  
     break;
    case '8'://命令处理，命令：【msg】
      memmove(Bidtmp, packetBuffer + 9, 4);
      memmove(UserID, packetBuffer + 13, 10);
      memmove(ID, packetBuffer + 23, 10);
      memset(msg, 0, 30);
      memmove(msg, packetBuffer + 33, cb - 36);

      doit=0;
      for(i=0;i<4;i++)
      {
          if(Bid[i]!=Bidtmp[i])
          {
            doit=1;//bid不一样，命令可以需要执行
            }
        
       }
 
      memmove(Bid,Bidtmp,4);
      
      //串口显示
      Serial.print("BID:");//BID
      Serial.print(Bid);
      Serial.print(" UserID:");//UserID
      Serial.print(UserID);
      Serial.print(" ID:");//ID
      Serial.println(ID);
      Serial.print(" MSG:");//信息
      Serial.println(msg);
      

      //正常开机按钮1-2S
      dotype=0;
      if (strcmp(msg, "10011") == 0) //如果收到命令1号继电器触动
      {
        dotype=1;
        ReturnMsg(UserID, "开机操作");//给用户返回信息
        if(doit==1)
        {
            digitalWrite(4, LOW ); //控制GPIO 2为低电平
            DoTime1 = millis();//执行时间
        }
        
        Serial.println("PWR jdq1 open");
      }
      
      //正常重启按钮2-2S
      if (strcmp(msg, "10012") == 0) //如果收到命令2号继电器触动
      {   
        dotype=2;
        ReturnMsg(UserID, "重启操作");//给用户返回信息
         if(doit==1)
        {
            digitalWrite(5, LOW); //控制GPIO 2为高电平
            DoTime2 = millis();//执行时间
        }

        Serial.println("RST jdq2 open");
      }

      //强制关机按钮1-10S
      if (strcmp(msg, "10013") == 0) //如果收到命令1号继电器触动
      {
        dotype=3;
        ReturnMsg(UserID, "强制关机操作");//给用户返回信息
         if(doit==1)
        {
            digitalWrite(4, LOW ); //控制GPIO 2为低电平
            DoTime1 = millis();//执行时间
        }

        Serial.println("QPWR jdq1 open");
      }

      //强制重启按钮1-10S
      if (strcmp(msg, "10014") == 0) //如果收到命令1号继电器触动
      {
        dotype=4;
        ReturnMsg(UserID, "强制重启操作");//给用户返回信息
         if(doit==1)
        {
            digitalWrite(5, LOW ); //控制GPIO 2为低电平
            DoTime2 = millis();//执行时间
        }

        Serial.println("QRST jdq2 open");
      }
      //查看
    if (strcmp(msg, "1001S") == 0) //如果收到命令
      {
        pwrled=1-digitalRead(PWR_LED);
        hddled=1-digitalRead(HDD_LED);

        char backstrs[8]={0};
        backstrs[0]='P';
        backstrs[1]='W';
        backstrs[2]=pwrled+0x30;
        backstrs[3]='-';
        backstrs[4]='H';
        backstrs[5]='D';
        backstrs[6]=hddled+0x30;
        backstrs[7]=0;
        
        
        ReturnMsg(UserID, backstrs);//给用户返回信息

        Serial.println("SS");
      }
      
      break;

  }
}



/*
*函数名： ReFlash
*功能：读EEPROM ，全部变量
*输入：
*返回：
*/
void ReadFlash(void)
{
  int i;
 unsigned char EEPROMRead[50],Data_uchar;  
 EEPROM.begin(FlashSize);  
// RunModeFlage = EEPROM.read(EPROM_addr_Mode);
  for(i=0;i<50;i++)
    EEPROMRead[i] = EEPROM.read(i);
  EEPROM.end();
  memcpy(MyID,&EEPROMRead[MyID_FlashAddr],10);  //用户ID 10
  memcpy(MyMM,&EEPROMRead[MyMM_FlashAddr],16);      //用户密码 16
 
  Data_uchar = EEPROM.read(EPROM_addr_Mode);

   Serial.print("\r\n ID:");
  for(i=0;i<10;i++)
    Serial.print(MyID[i],0);
    
   Serial.print("\r\n MM:");
  for(i=0;i<16;i++)
    Serial.print(MyMM[i],0);
    
    Serial.print("\r\nUART Buad:");
    Serial.print(UARTBuad,DEC);
    Serial.print("\r\n");

    Serial.print("AT+ALL=?\r\n");        

}





//AT模式串口监听
void SysSetMode()
{
unsigned char Comdat[30];
unsigned char ComIndex,i;
int EEPROMWriteBuf[20];
unsigned char EEPROMWriteSize = 0;
unsigned int EEPROMAddr;
char CMD_ERR_Flag = 0;
int tempint;
unsigned int  tempuint;
unsigned long Tempu32;

ComIndex = 0;
EEPROMWriteSize = 0;
while (Serial.available() > 0)
  {
      Comdat[ComIndex] = Serial.read();
      if(ComIndex<30)
        ComIndex++;    
      delay(3);
  }
  if(ComIndex>0)
  {
     Serial.print("\r\nUART-R:");
//      Serial.write(ComIndex);
//    for(i=0;i<ComIndex;i++)
    {
       Serial.write(Comdat,ComIndex);//DEC
    }
    
     Serial.print("\r\n");
    //strncmpi(char *str1,char *str2,unsigned char maxlen)
    if((Comdat[0]=='A')&&(Comdat[1]=='T'))
    {
    
      if(strncmp((char *)&Comdat[2],(char *)"+ID=",4)==0) //ID设置 10位
        {
              for(int i=0;i<10;i++){
                EEPROMWriteBuf[i] = Comdat[6+i];
              }
              EEPROMWriteSize = 10;
              EEPROMAddr = MyID_FlashAddr;      
        }
        else if(strncmp((char *)&Comdat[2],(char *)"+MM=",4)==0) //密码设置 16位
        {
                for(int i=0;i<16;i++){
                  EEPROMWriteBuf[i] = Comdat[6+i];
                }
                EEPROMWriteSize = 16;
                EEPROMAddr = MyMM_FlashAddr;          
        }
           //Serial.print("\r\n");    
        else if(strncmp((char *)&Comdat[2],(char *)"+DFID",5)==0) //对方ID
        {
          
        }else if(strncmp((char *)&Comdat[2],(char *)"+DFMM",5)==0) //对方密码
        {
          
        }else if(strncmp((char *)&Comdat[2],(char *)"+BAUD",5)==0) //设置串口波特率
        {
          EEPROMWriteSize = 1;
          EEPROMAddr = Buad_FlashAddr;
          if(strncmp((char *)&Comdat[8],(char *)"4800",4)==0)  EEPROMWriteBuf[0] = 1;
          else if(strncmp((char *)&Comdat[8],(char *)"9600",4)==0)  EEPROMWriteBuf[0] = 2;
          else if(strncmp((char *)&Comdat[8],(char *)"14400",5)==0)  EEPROMWriteBuf[0] = 3;
          else if(strncmp((char *)&Comdat[8],(char *)"19200",5)==0)  EEPROMWriteBuf[0] = 4;
          else if(strncmp((char *)&Comdat[8],(char *)"38400",5)==0)  EEPROMWriteBuf[0] = 5;
          else if(strncmp((char *)&Comdat[8],(char *)"56000",5)==0)  EEPROMWriteBuf[0] = 6;
          else if(strncmp((char *)&Comdat[8],(char *)"115200",6)==0)  EEPROMWriteBuf[0] =7;
          else if(strncmp((char *)&Comdat[8],(char *)"128000",6)==0)  EEPROMWriteBuf[0] = 8;
          else CMD_ERR_Flag = 1;
        }   
        
        else if(strncmp((char *)&Comdat[2],(char *)"+ALL=?",6)==0)
          {
          
           ReadFlash();
          }
         
          //wifistatus
        if(CMD_ERR_Flag) 
        Serial.print("CMD ERR\r\n");
        else if((Mywifi.ssidSetFlag == 1)&&(Mywifi.KeySetFlag == 1))
        {
          WiFi.begin(Mywifi.wifissid, Mywifi.wifikey);
          Serial.println("Wait for Connect...");
          Mywifi.linkstatus = 0;
        }
        else if(EEPROMWriteSize>0)
        {
            if(EEROM_Write(EEPROMAddr,EEPROMWriteBuf,EEPROMWriteSize))
            {
              Serial.print("Data Saved\r\n");
              EEPROMWriteSize = 0;        
             //    delay(500);
             // ReFlash();
            }
        }        
    }
     memset(Comdat, 0,30);//清楚缓存
    }

   }

   /*
*函数名： EEROM_Write
*功能：写EEPROM 
*输入：FirstAddr写入的首地址
*     Data待写入的数据指针
*     DataSize写入数据的字节数
*返回：0：写入失败  1：写完成
*/
char EEROM_Write(unsigned int FirstAddr,int *Data,unsigned char DataSize)
{
  if((FirstAddr+DataSize)>FlashSize)  return 0;
  if(DataSize==0)    return 0;
  EEPROM.begin(FlashSize);
  for(int i=0;i<DataSize;i++)
    EEPROM.write(FirstAddr+i,*Data++);
  EEPROM.commit();
   EEPROM.end(); 
   return 1; 
}




//获取wifi的状态
void GetWIFIStatus()
{
  unsigned char  SuccessedFlag;
  static unsigned char Printflag = 1;
  static unsigned int TimeCount = 0, UlinkCount = 0;
  SuccessedFlag = 0;
  //如果正在连接
  if (Mywifi.linkstatus == WIFI_LINK_ING)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Udp.begin(localPort);
      Mywifi.linkstatus = WIFI_LINKED;
      Serial.println("WIFI_LINKED");
      SuccessedFlag = 1;
    } 
    else
    {
      if ((TimeCount % 10) == 2)  //WiFi重新初始化
        Wifiinit(0);
      TimeCount++;
    }
  } 
  //如果是正在配置连接
  else if (Mywifi.linkstatus == WIFI_SMART_LINK_ING)
  {
    if (WiFi.smartConfigDone())
    {
      Udp.begin(localPort);
      Mywifi.linkstatus = WIFI_LINKED;
      Serial.println("WIFI_LINKED");
      SuccessedFlag = 1;
    }
    else
    {
       Serial.println("smart linking");
      }
  } 
  else
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      UlinkCount++;
      if (UlinkCount > 2)
      {
        Wifiinit(0);
        UlinkCount = 0;
      }
    } else
      UlinkCount = 0;
  }

  if (SuccessedFlag)
  {
    Printflag = 0;
    //      if(WifiFastInitedFlag==0)
    {
      WifiFastInitedFlag = 1;

      WiFi.printDiag(Serial);
      // Print the IP address
      Serial.println(WiFi.localIP());
      Serial.print("ChipID:");
      Serial.println(ESP.getChipId(), HEX);
    }
  }
}


//IO初始化
void IOinit()
{

  //初始化端口为输出
  pinMode(2, OUTPUT);//板载LED
  pinMode(KEY, INPUT_PULLUP);//key
  pinMode(4, OUTPUT);//继电器1 开机
  pinMode(5, OUTPUT);//继电器2 重启
  pinMode(16,INPUT_PULLUP);//PWR LED
  pinMode(12,INPUT_PULLUP);//HDD LED

  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);

}

//wifi初始化
void Wifiinit(unsigned char InitFlag)
{
 
  if (InitFlag == 0)
  {
    WiFi.mode(WIFI_STA);
    Mywifi.linkstatus = WIFI_LINK_ING;
      Serial.println("WIFI_LINK_ING");
  }
  else if (InitFlag == 1)
  {
    WiFi.beginSmartConfig();  //自动判断协议SC_TYPE_ESPTOUCH   V0.34, SC_TYPE_AIRKISS=1
    Mywifi.linkstatus = WIFI_SMART_LINK_ING;
      Serial.println("WIFI_SMART_LINK_ING");
  }


}

byte ReadKey()    //读按键
{
  byte KeyValue;
  if (digitalRead(KEY) == 0)
    KeyValue = 1;
  else
    KeyValue = 0;
  return KeyValue ;
}

void KeyScan()    //按键处理 WiFi重新配置 20 ms任务
{
  static unsigned int TimeCount = 0;

  if (ReadKey())
  {
    if (TimeCount == 0)
    {
      Wifiinit(1);
      TimeCount = 50;
    }
  }
  else
  {
    if (TimeCount > 0)
      TimeCount--;
  }

}

#define RunLedFREQ    10    //RunLed函数运行频率
void RunLed()   //100ms任务
{
  
  unsigned short Cycle;

    if(Mywifi.linkstatus==LINK_SERVER_FLAG)
    {
      LEDfreq = 20;//0.5;
      LEDratio =6;// 0.25;
    }else if(Mywifi.linkstatus==LINK_OTHER_FLAG)
    {
      LEDfreq = 20;//0.25;
      LEDratio = 10;//0.125;      
    }else if(Mywifi.linkstatus==WIFI_LINKED)
    {
      LEDfreq =20;// 1;
      LEDratio = 15;//1;           
    }
    else if(Mywifi.linkstatus==WIFI_SMART_LINK_ING)
    {
      LEDfreq = 5;//3;
      LEDratio = 3;//0.5;        
    }
    else //if(Mywifi.linkstatus==WIFI_LINKED)
    {
      LEDfreq = 0;
      LEDratio = 0;          
    }
    
  
  
  if(LEDfreq==0)//频率
    digitalWrite(LED, LED_OFF);
   else
   {
    digitalWrite(LED, LED_ON);
       Timecount++;
       
       if(Timecount==1)
       {
         digitalWrite(RUN_LED, LED_ON);
       }
       else if(Timecount==LEDratio)
       {
         digitalWrite(RUN_LED, LED_OFF); 
       }
       else if(Timecount==LEDfreq)
            {
              Timecount = 0;
             }
        
    
      
    }

}


void setup() {

  IOinit();   //GPIO初始化
  ReadFlash();  //读flash数据
  Wifiinit(0);//设置客户端模式


  Serial.begin(115200);


  csh(MyID,MyMM);//请修改设备ID和16位加密密码

  Serial.println("setup end");
}

void loop() {

  
  OS_NowTime = millis();//当前系统时间
 
  //Serial.println(OS_NowTime,DEC);
  //Serial.println(LastTime,DEC);
  if (LastTime != 0)
  {
      if(JsTime!=OS_NowTime)
      {
         JsTime=OS_NowTime;
        
        if ((OS_NowTime - LastTime) % 20==0 && Timeflag_20ms == 0)
          Timeflag_20ms = 1;
        if ((OS_NowTime - LastTime) % 100==0 && Timeflag_100ms == 0)
          Timeflag_100ms = 1;
        if ((OS_NowTime - LastTime) % 1000==0 && Timeflag_1s == 0)
          Timeflag_1s = 1;
         if ((OS_NowTime - LastTime) % 10000==0 && Timeflag_30s == 0)
          Timeflag_30s = 1;
         if ((OS_NowTime - LastTime) % 15000==0 && Timeflag_33s == 0)
          Timeflag_33s = 1;
        if ((OS_NowTime - LastTime) % 20000==0 && Timeflag_35s == 0)
         {
          Timeflag_35s = 1;
          LastTime = OS_NowTime; //上次时间
         }
      }
  }
  else
  {
    LastTime = OS_NowTime; //上次时间
    Timeflag_20ms = 1;
    Timeflag_100ms = 1;
    Timeflag_30s = 1;
    Timeflag_1s = 1;
    Timeflag_33s = 1;
    Timeflag_35s = 1;
  }


  //2S后继电器复位 开机
  if(dotype==1)
  {
    
    if (DoTime1 != 0 && (OS_NowTime - DoTime1) > 2000)
    {
      digitalWrite(4, HIGH); //控制GPIO 4为低电平
      DoTime1=0;
        Serial.println("PWR jdq1 close");
    }
  }
  //强制关机
   else if(dotype==3)
  {
    
     if (DoTime1 != 0 && (OS_NowTime - DoTime1) > 10000)
    {
      digitalWrite(4, HIGH); //控制GPIO 4为低电平
      DoTime1=0;
        Serial.println("QPWR jdq1 close");
    }
    
  }
  //重启
  else if(dotype==2)
  {
   
    if (DoTime2 != 0 && (OS_NowTime - DoTime2) > 2000)
    {
      digitalWrite(5, HIGH); //控制GPIO 4为低电平
      DoTime2=0;
       Serial.println("RST jdq2 close");
    }
  }
  //强制重启
  else if(dotype==4)
  {
   
    if (DoTime2 != 0 && (OS_NowTime - DoTime2) > 10000)
    {
      digitalWrite(5, HIGH); //控制GPIO 4为低电平
      DoTime2=0;
       Serial.println("QRST jdq2 close");
    }
  }

  //UDP数据处理
  cb = Udp.parsePacket();
  if (cb)
  {
    Serial.print("RC:");
    in();//收到数据处理
    
  }
    
    //串口接受处理AT指令
    SysSetMode();    

  
//按键扫描，WiFi重新配置
  if (Timeflag_20ms == 1) //20ms任务
  {
    Timeflag_20ms = 0;
     KeyScan();             

  }
  if(Timeflag_100ms==1)     //100ms任务
   {
    Timeflag_100ms = 0; 
    //Serial.print("100ms:");
    // Serial.println(OS_NowTime,DEC);  
    RunLed();             //LED运行指示灯
   }
  //检测WIFI，进入配置
  if (Timeflag_1s == 1)
  {
    Timeflag_1s = 0;
    GetWIFIStatus();    //wifi连接状态更新
    //Serial.println("wifi");
  }

  if (Timeflag_30s == 1&&WifiFastInitedFlag==1)//连接wifi成功才发
  {
    Timeflag_30s = 0;
    memset(sendsta, 0,5);//清楚缓存

    pwrled=digitalRead(PWR_LED);
    hddled=digitalRead(HDD_LED);
    
    sendsta[0]=pwrled+0x30;
    sendsta[1]=hddled+0x30;
    sendsta[2]=dotype+0x30;
    
    xtb(sendsta);//心跳包
    Serial.println("heart");
  }

  if(Timeflag_33s==1&&WifiFastInitedFlag==1)
  {
    float wd,sd;
    GetTempHumi(&wendu,&shidu); //  读温湿度
    wd=wendu;
    sd=shidu;
    
    //updata函数，用于上传数据 feelID:传感器ID;mun:数据;width:数据整数部分位数;prec:数据小数部分位数
    updata(UserID, "200", wd, 3, 1);
     Timeflag_33s = 0;
   }

  if(Timeflag_35s==1&&WifiFastInitedFlag==1)
  {
    float wd,sd;
    GetTempHumi(&wendu,&shidu); //  读温湿度
    wd=wendu;
    sd=shidu;
    
    //updata函数，用于上传数据 feelID:传感器ID;mun:数据;width:数据整数部分位数;prec:数据小数部分位数
    updata(UserID, "201", sd, 3, 1);
     Timeflag_35s = 0;
   }


  

}
