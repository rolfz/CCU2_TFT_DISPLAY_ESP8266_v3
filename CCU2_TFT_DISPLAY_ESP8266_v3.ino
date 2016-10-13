/*************************************************************************
WeMos D1 based LCD TFT display for CCU2 Homematic home automation system

Code is based on various information from Homematic FHZ Forum and some testing
using the XML-API that helped me to find out the various adresses to get the 
data from CCU2.

I must say that findint out how to address the devices and variables was mode
difficult than writing the C code for the ESP8266 !!

Autor: ROlf Ziegler September 2016 

Rem. Display is used in french speaking region, so menus are in french but
can easly be modified. 
Some code that been disabled after moving the information the the Alarm case.
It could be reactivated if required.

V1: original code with 5 pages
v2: added alarm overview
v3: added new font
 *************************************************************************/
#define version " 1.1b 23.9.2016"

//#include "SPI.h" // not sure if we need this
//as we need 9bit SPI, we will use Software SPI, no hardware SPI driver required
#include "Adafruit_GFX.h" // ADAFRUIT graphics library
#include "Adafruit_ILI9341.h" // ADAFRUIT TFT driver
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerifBoldItalic12pt7b.h>
#include <Fonts/FreeSerifBoldItalic18pt7b.h>

#define F1 FreeSerif9pt7b
#define F2 FreeSerifBoldItalic12pt7b
#define F3 FreeSerifBoldItalic18pt7b

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
  
const char* ssid     = "SSID";  // Network SSID
const char* password = "PASSWORD";     // Network password
const char* ccuHost = "192.168.1.x"; // CCU IP-Adress
//bool status = false;
int ecolor;
int hcolor;
String ttrue="vraie";
String tfales="faux";
String alarmLevel="0";

int x=0,y=0;

enum ttype {NORM,PCENT,DEG}; // value type from CCU2, used to add prefix and suffix to the display
//test  enum screen {TEMP,MOV,PORTES,PRISES,ALARM,NEXT};
enum screen {ALARM,TEMP,PLUGS,NEXT}; // screen types
bool alarm=false;
long int milicount=0,lastmili;
/*
 * const String titles[4][3]={"P.Entree","P.Hiver","P.Terr.", //portes
                 "F.Bureau","F.Amis","F.Atelier",             //fenetres
                 "M.Jardin","M.Atelier","S.Atelier",          //Movement, Fumée
                 "M.Salon","M.Couche","M.Bain"};
                 */
const String titles[4][3]={"Porte","Porte","Porte", //portes
                 "Fenetre","Fenetre","Fenetre",             //fenetres
                 "Mouvement","Mouvement","Fumee",          //Movement, Fumée
                 "Mouvement","Mouvement","Mouvement"};

                 
const String titles2[4][3]={"Entree","J-Hiver","Terrasse", //portes
                 "Bureau","Amis","Atelier",             //fenetres
                 "Jardin","Atelier","Atelier",          //Movement, Fumée
                 "Salon","J-Hiver","Terrasse"};

// format parameter for alarm table
#define DX 100  // rectangle width
#define DY 48   // rectangle high
#define NX 3    // number of horiz. rectangles
#define NY 4    // number of vertical rectangles
#define OX 2    // x offset for 1st box
#define OY 29   // y offset for 1st box 
#define OFX 3   // x space between boxes
#define OFY 3   // y space between boxes
                 
String Answers[4][3];

// CCU text positions
#define XOFF  20
#define P1 50
#define P2 80
#define P3 110
#define P4 140
#define P5 170
#define P6 200


// WIFI setup
WiFiClient client; // create tcp connection

// For the Adafruit shield, these are the default.
#define TFT_DC 2
#define TFT_CS 5

String displayCCU(String title,String request,String sub, int type,uint8_t xpos, uint8_t ypos);
String readCCU(String request,String type) ;


String request = "", line = "", answer = "";

String timestamp() { // Betriebszeit als Stunde:Minute:Sekunde
  char stamp[10];
  int lfdHours = millis()/3600000;
  int lfdMinuts = millis()/60000-lfdHours*60;
  int lfdSeconds = millis()/1000-lfdHours*3600-lfdMinuts*60;
  sprintf (stamp,"%02d:%02d:%02d", lfdHours, lfdMinuts, lfdSeconds);
  return stamp;
}

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
// If using the breakout, change pins as desired
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

// missing macro
#define min(X, Y) (((X)<(Y))?(X):(Y))

//====================== code starts here================================
void setup() {
  WiFi.mode(WIFI_OFF);
  Serial.begin(9600);
  Serial.println("ILI9341 Test!"); 

  pinMode(TFT_DC,OUTPUT);
  pinMode(TFT_CS,OUTPUT);
  
// init TFT to print wifi data on it
// send start screen
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(40, 40);
  tft.setFont(&F3);
  tft.setTextColor(ILI9341_BLUE); //   tft.setTextSize(3);
  tft.print("Z-Control");
  tft.setCursor(60, 80);
  tft.setFont(&F2);
  tft.setTextColor(ILI9341_YELLOW); //tft.setTextSize(2);
  tft.print("Domotique");
  tft.setCursor(70, 120);
  tft.print("Display");
                 
  tft.setFont(&F1);
  tft.setTextColor(ILI9341_YELLOW); //tft.setTextSize(1);
  tft.setCursor(0, 180);
  tft.print(" Version:");
  tft.print(version);

  delay(5000);

  Serial.println("\n\r \n\rWorking to connect\r\nOn Wifi Network");
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 40);
  tft.setFont(&F2);
  tft.setTextColor(ILI9341_RED);  //  tft.setTextSize(2);
  tft.print("Connecting to: ");
  tft.setTextColor(ILI9341_YELLOW);
  tft.println(ssid);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    tft.print(".");
    delay(500);
    }
  ecolor=ILI9341_GREEN;
  tft.fillScreen(ILI9341_BLACK);    
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(0,5);
  tft.setFont(&F2);
//  tft.setTextSize(3);
  tft.println("NET: "+String(ssid));
tft.setFont(&F1);
//  tft.setTextSize(2);
    
  tft.setCursor(0, 80);
  tft.setTextColor(ILI9341_GREEN);
  tft.print("Connected to ");
  tft.setTextColor(ILI9341_YELLOW);
  tft.println(ssid);
  tft.setCursor(0, 120);
  tft.setTextColor(ILI9341_GREEN);
  tft.print("IP Address: ");
  tft.setTextColor(ILI9341_YELLOW);
  String ipAddr =WiFi.localIP().toString();
  tft.println(ipAddr);

  tft.setCursor(0, 160);
  tft.setTextColor(ILI9341_GREEN);
  tft.print("Signal level: ");
  int rssi = WiFi.RSSI();
  tft.setTextColor(ILI9341_YELLOW);
  tft.print(rssi);
  tft.setTextColor(ILI9341_GREEN);
  tft.println(" dBm");
  delay(3000);

  lastmili=millis();
}
//  test sensor request with following line
// http://192.168.1.137:8181/x.exe?answer=dom.GetObject("BidCos-RF.LEQ0657834:1.BRIGHTNESS").State()
//
void loop(void) {

      #define P_DEL 5 // page delay
      long int mil=millis();
      
      // increase the task counter every 1000*P_DEL ms
      if((mil-lastmili)>=(1000*P_DEL)) // currently every 5 seconds
      {lastmili=mil;
       
//test       if(milicount==NEXT)milicount=ALARM;
       // we also check if there was an alarm every 5 seconds
       if(readCCU("Alarm.Source","")!="null")
       // display alarm if alarm variable is not == "Null"
       {
        // show alarm source as long as Alarm is not cleared

        return;
       }
       else {
        // show next screen
       milicount++;
       if(milicount==NEXT)milicount=0;
       }
//      Serial.println(milicount);

      tft.fillScreen(ILI9341_BLACK);    
      tft.setCursor(0,7);
      tft.setFont(&F3);
//      tft.setTextSize(3);

// set alarm status line on top of the screen 
      alarmLevel =readCCU("Alarm",".Level"); 
      
      if(alarmLevel=="0"){
        tft.setTextColor(ILI9341_GREEN);
        tft.println("   ALARM OFF    ");
      } else
      if(alarmLevel== "1"){
        tft.setTextColor(ILI9341_YELLOW);
        tft.println(" ALARM INTERNE  ");
      }else
      if(alarmLevel=="2"){
        tft.setTextColor(ILI9341_RED);
        tft.println(" ALARM EXTERNE  ");
      }else{
        tft.setTextColor(ILI9341_YELLOW);
        tft.println("ALARM Bellavista");
      }
      
      switch(milicount){
// temperature display
      case TEMP:
      ecolor=ILI9341_CYAN;
      hcolor=ILI9341_RED;

      tft.drawRect(1,2,317,234,ILI9341_CYAN);
      tft.drawLine(0, 35, 317,35, ILI9341_CYAN);
      tft.setFont(&F2);
//      tft.setTextSize(2);
      tft.setTextColor(ecolor);
      displayCCU("Exterrieure: ","CUxD.CUX1200001:1",".TEMPERATURE",DEG,XOFF,P1);
      displayCCU("Interrieure: ","CUxD.CUX1200003:1",".TEMPERATURE",DEG,XOFF,P2);
      displayCCU("J-Hiver:     ","CUxD.CUX1200002:1",".TEMPERATURE",DEG,XOFF,P3); 
      displayCCU("Hum. inter.: ","CUxD.CUX1200003:1",".HUMIDITY",PCENT,XOFF,P4); 
      displayCCU("Hum. J-Hiver:","CUxD.CUX1200002:1",".HUMIDITY",PCENT,XOFF,P5); 
      break;
   
      case PLUGS:
// switch display    

      tfales="HORS";
      ttrue="EN";
      ecolor=ILI9341_GREEN;
      hcolor=ILI9341_RED;
      tft.drawRect(1,2,317,234,ILI9341_GREEN);
      tft.drawLine(0, 35, 317,35, ILI9341_GREEN);
      tft.setFont(&F2);
//      tft.setTextSize(2);
      tft.setTextColor(ecolor);
      displayCCU("Entree:        ","CUxD.CUX0200005:1",".STATE",NORM,XOFF,P1); 
      displayCCU("Bureau:        ","CUxD.CUX0200003:1",".STATE",NORM,XOFF,P2);
      displayCCU("Chambre amis:  ","CUxD.CUX0200001:1",".STATE",NORM,XOFF,P3);
      displayCCU("TV:            ","CUxD.CUX0200006:1",".STATE",NORM,XOFF,P4); 
      displayCCU("Salon:         ","CUxD.CUX0200004:1",".STATE",NORM,XOFF,P5); 
      displayCCU("Sortie Balcon: ","CUxD.CUX0200002:1",".STATE",NORM,XOFF,P6);       
    break;

// ALARM DISPLAY    
      case ALARM: // case to display an error
      // get data from non scanned sensors
      // as motion and doors are disables, we need to get this info
      Answers[0][0]=readCCU("BidCos-RF.LEQ0501270:1",".STATE");
      Answers[0][1]=readCCU("BidCos-RF.LEQ0501025:1",".STATE");
      Answers[0][2]=readCCU("BidCos-RF.LEQ0502786:1",".STATE"); 
      Answers[1][0]=readCCU("BidCos-RF.LEQ0502403:1",".STATE"); 
      Answers[1][1]=readCCU("BidCos-RF.LEQ0500953:1",".STATE"); 
      Answers[1][2]=readCCU("BidCos-RF.LEQ0501273:1",".STATE");       
      
      
      Answers[2][0]=readCCU("BidCos-RF.LEQ0657749:1",".MOTION"); // Jardin
      Serial.println(Answers[2][0]);
      Answers[2][1]=readCCU("BidCos-RF.LEQ0657834:1",".MOTION"); // Atelier
      Serial.println(Answers[2][1]);
      Answers[2][2]=readCCU("BidCos-RF.*LEQ1092567:1",".STATE"); // get smoke 
      Serial.println(Answers[2][2]);
      
// not yet finished on CCU side      
      Answers[3][0]=readCCU("Salon",".Motion"); //
      Serial.println(Answers[0][3]);
      Answers[3][1]=readCCU("Couche",".Motion"); //
      Serial.println(Answers[1][3]);
      Answers[3][2]=readCCU("Bain",".Motion"); //
      Serial.println(Answers[2][3]);

//      tft.setTextSize(1);
      for(y=0;y<NY;y++)
        {
         for(x=0;x<NX;x++)
            {
              tft.drawRect(1,2,316,234,ILI9341_YELLOW);
              int posx=OX+(3+x*DX+OFX*x);
              int posy=OY+(3+y*DY+OFY*y);
              
              // if we have an alarm, we change the color from green to red
              Serial.println("Answ "+Answers[x][y]);
              if(Answers[y][x]=="true"){
                      tft.fillRect(posx,posy,DX,DY,ILI9341_RED);
                      tft.setTextColor(ILI9341_WHITE);
                      }else 
                      if(Answers[y][x]=="false"){
                      tft.fillRect(posx,posy,DX,DY,ILI9341_GREEN);
                      tft.setTextColor(ILI9341_BLACK);
                      }else{
                      tft.fillRect(posx,posy,DX,DY,ILI9341_YELLOW);
                      tft.setTextColor(ILI9341_BLACK);
                      }
              // print logo into box        
              tft.setFont(&F1);
//              tft.setTextSize(1);
              tft.setCursor(posx+4,posy+5); 
              tft.println(titles[y][x]);
              
              tft.setTextSize(2);
              tft.setCursor(posx+4,posy+25); 
              tft.println(titles2[y][x]);
            }  
      }
     
    break;
      case NEXT:
      break;
        
      } // end switch
    } // end delay
}

int cnt=0;

// get data from CCU2 without graphics display
String readCCU(String request,String type) { // LCD steuern
      HTTPClient http;
      http.begin(ccuHost, 8181, "/eriwan.exe?answer=dom.GetObject(\"" + String(request) +String(type) +"\").State()"); // original
      int httpStatuscode = http.GET();
      if(httpStatuscode) {
        if(httpStatuscode == 200) { // Statuscode ok?
          char hbuffer[200] = { 0 };  // Buffer for 200 characters
          WiFiClient * stream = http.getStreamPtr(); // Read stream 
          while(http.connected()) {
            int streamLen = stream->available();
            if(streamLen) {
              int c = stream->readBytes(hbuffer, streamLen);
              String xml = String(hbuffer);
              String answer = xml.substring(xml.indexOf("<answer>") + 8, xml.indexOf("</answer>")); 
              return answer;
            }
            else {
                  //Serial.println(request+" ! No data");
                  delay(1);
            }
            }
        }
      }
      else {
        Serial.print(timestamp());
        Serial.printf("  Error: No CCU connection, HTTP Status code %d\n", httpStatuscode);
      }
    }

// get graphics and display it on TFT      
String displayCCU(String title,String request,String sub, int type,uint8_t xpos, uint8_t ypos) { // LCD steuern

      String post="";
      String nanswer="";
      HTTPClient http;
      http.begin(ccuHost, 8181, "/eriwan.exe?answer=dom.GetObject(\"" + String(request) +String(sub) +"\").State()"); // original
      int httpStatuscode = http.GET();
      if(httpStatuscode) {
        if(httpStatuscode == 200) { // Statuscode ok?
          char hbuffer[200] = { 0 };  // Buffer for 200 characters
          WiFiClient * stream = http.getStreamPtr(); // Read stream 
          while(http.connected()) {
            int streamLen = stream->available();
            if(streamLen) {
              int c = stream->readBytes(hbuffer, streamLen);
              String xml = String(hbuffer);
              String answer = xml.substring(xml.indexOf("<answer>") + 8, xml.indexOf("</answer>")); 
              if(answer != "null"){     
                // set color if value it true/fales 
                if(answer=="true"){nanswer=ttrue;
                      tft.setTextColor(ecolor);
                                   }
                else if(answer=="false"){ nanswer=tfales; 
                                          tft.setTextColor(hcolor);
                                        }
                
                 String temp;
                 switch(type){
                  case NORM:
                          post="";
                  break;
                  case DEG : // nous avons une température
                           temp = answer.substring(0, answer.indexOf(".")+2); 
                           nanswer=temp;
                           post=" C"; // to print deg.C
                  break;
                  
                  case PCENT :// nous avons de l'humidité en %
                           post="%";
                           nanswer=answer; // no change in the value
                  break;
                  default:
                  break;
                 }   
                        
                 tft.setCursor(xpos,ypos);
                 tft.println(String(title) + " " + nanswer+post);
                 Serial.println(String(timestamp())+" "+title+request + " > " + nanswer+post);
                 return answer; // last change added this line
                }
            }
            else {
                  delay(1);
            }
            }
        }
      }
      else {
        Serial.print(timestamp());
        Serial.printf("  Error: No CCU connection, HTTP Status code %d\n", httpStatuscode);
      }
}
  
// end

