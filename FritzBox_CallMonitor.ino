/*
 * FritzBox CallMonitor
 * --------------------
 * Copyright (C) 2014 by Tobias Link - ranger81@dsuclan.de - www.ranger81.de
 *
 * This sketch can be used on a Arduino hooked up to an ENC28J60 ethernet module
 * or shield. There should be also a LiquidCrystal display (16x2 characters)
 * attached to the Arduino. It provides a simple call monitoring functionality to
 * see who is currently calling and who you are going to call. Useful for phones
 * without own display.
 *
 * Requirements:
 * - Arduino (ATMega w/ 32kByte flash storage)
 * - ENC28J60 ethernet module or shield
 * - LC Display HD44780 (16x2 characters)
 * - AVM FritzBox with enabled CallMonitor support (dial #96*5* to enable)
 * - UIPEthernet Library (https://github.com/ntruchsess/arduino_uip)
 *
 * Last changes:
 * - 2014-04-06: Added missed call counter and display of last missed call number on LCD
 * - 2014-04-05: Added missed call LED feature
 * - 2014-04-04: Added dim timeout for display backlight and proper time display after DISCONNECT
 * - 2014-04-03: Added timer for display how long the phone call was
 * - 2014-04-03: Initial internal release
 *
 * Open Topics / ToDo:
 */

/**** INCLUDES *********************************/
#include <UIPEthernet.h>
#include <LiquidCrystal.h>
/***********************************************/

/**** CONFIGURATION ****************************/
#define    LCD_BACKLIGHT_PIN                    9             // LCD backlight positive PIN
#define    MISSED_CALL_LED_PIN                  A0            // LED notification if call was missed/not picked up
#define    CLEAR_MISSED_CALL_BUTTON_PIN         8             // Attached button to clear missed calls
#define    CLEAR_MISESED_CALL_ON_NEW_ACTION     0             // Clear missed call state if new action was done
#define    FRITZBOX_HOSTNAME                    "fritz.box"   // Host name of FritzBox
#define    FRITZBOX_PORT                        1012          // Port with running FritzBox call monitor
#define    RETRY_TIMEOUT                        5000          // Retry connection to FB every x seconds
#define    CALL_DURATION_UPDATE_INTERVAL        1000          // Update ongoing call duration on LCD
#define    LCD_PIN_RS                           7             // Pin for initialization of LiquidCrystal library (RS)
#define    LCD_PIN_ENABLE                       6             // Pin for initialization of LiquidCrystal library (ENABLE)
#define    LCD_PIN_D4                           5             // Pin for initialization of LiquidCrystal library (D4)
#define    LCD_PIN_D5                           4             // Pin for initialization of LiquidCrystal library (D5)
#define    LCD_PIN_D6                           3             // Pin for initialization of LiquidCrystal library (D6)
#define    LCD_PIN_D7                           2             // Pin for initialization of LiquidCrystal library (D7)
#define    LCD_MAX_CHARS                        16            // LCD max chars in one line
#define    LCD_ENABLE_DIM                       1             // If enabled, LCD backlight will only dim if unused. If disabled LCD backlight will turn off
#define    LCD_DIM_TIMEOUT                      10000         // Timeout for display dimmer after DISCONNECT
#define    LCD_DIM_PWM_VALUE                    10            // analogWrite PWM value for dimmed LCD backlight
#define    DISPLAY_CALL_DURATION                1             // Enable or Disable display of call duration
#define    DEBUG                                1             // Enable serial debugging
#define    SERIAL_BAUD_RATE                     9600          // Baud rate for serial communication
#define    PRICEPERMINUTE                       0.025         // Price per minute in Euro
#define    CHECKCONNECTION                      10000         // Milliseconds
/***********************************************/

/**** GLOBAL VARIABLES *************************/
EthernetClient client;
LiquidCrystal lcd(LCD_PIN_RS, LCD_PIN_ENABLE, LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7);

unsigned long next;
unsigned long callstart;
unsigned long calllaststatus;
unsigned long lcdtimeoutstart;
unsigned long connectioncheck;

byte missedcallcount;
char * lastnumber;
char * lastmissednumber;

boolean call_connected;
boolean lcd_dimmer;
boolean lastcallwasmissedcall;
boolean showprice;

uint8_t mac[6] = {
  0x74,0x69,0x69,0x2D,0x30,0x02                                                                        };
/***********************************************/

/**** METHOD: SETUP ****************************/
void setup() {


  next = 0;
  call_connected = false;
  lcd_dimmer = false;
  showprice = false;

  missedcallcount = 0;
  lastcallwasmissedcall = 0;

  pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
  pinMode(CLEAR_MISSED_CALL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MISSED_CALL_LED_PIN, OUTPUT);

  lcd.begin(LCD_MAX_CHARS, 2);

  lcdon();
  lcdsplash();
  //lcdstartdim();

#ifdef DEBUG
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println(F("[FritzBox CallMonitor - www.ranger81.de]\n"));
  Serial.println(F("Starting Network..."));
#endif
  Ethernet.begin(mac);
#ifdef DEBUG 
  Serial.print(F("IP Address: "));
  Serial.println(Ethernet.localIP());
  Serial.print(F("Subnet Mask: "));
  Serial.println(Ethernet.subnetMask());
  Serial.print(F("Gateway IP: "));
  Serial.println(Ethernet.gatewayIP());
  Serial.print(F("DNS Server IP: "));
  Serial.println(Ethernet.dnsServerIP());
#endif

  lastnumber = (char*) malloc (LCD_MAX_CHARS+1);
  lastmissednumber = (char*) malloc (LCD_MAX_CHARS+1);

}
/***********************************************/

/**** METHOD: LCDSPLASH ************************/
void lcdsplash() {
  lcd.clear();
  lcd.print(F(" FB CallMonitor"));
  lcd.setCursor(0,1);
  lcd.print(F("www.ranger81.de")); 
}
/***********************************************/
/**** METHOD: RESETETHERNET ********************/
void resetEthernet() {
  client.stop();
  delay(1000);
  Ethernet.begin(mac);
  delay(1000);
}
/***********************************************/

/**** METHOD: LCDON ****************************/
void lcdon() {
  digitalWrite(LCD_BACKLIGHT_PIN, HIGH);
}
/***********************************************/

/**** METHOD: LCDDIM ***************************/
void lcddim() {
  if(LCD_ENABLE_DIM)
  {
    if(missedcallcount > 0)
    {
      lcdmissedcall();
    }
    else
    {
      lcdsplash();
    }
    analogWrite(LCD_BACKLIGHT_PIN, LCD_DIM_PWM_VALUE); 
  }
  else
  {
    lcdoff();
  }
}
/***********************************************/

/**** METHOD: LCDOFF ***************************/
void lcdoff() {
  if(missedcallcount > 0)
  {
    lcdmissedcall();
  }
  else
  {
    lcdsplash();
  }
  digitalWrite(LCD_BACKLIGHT_PIN, LOW); 
}
/***********************************************/

/**** METHOD: LCDSTARTDIM **********************/
void lcdstartdim() {
  lcd_dimmer = true;
  lcdtimeoutstart = millis(); 
}
/***********************************************/

/**** METHOD: LCDCONNECTING ********************/
void lcdconnecting() {
  lcdon();
  lcd.clear();
  lcd.print("Connecting to");
  lcd.setCursor(0,1);
  lcd.print("FritzBox ...");
}
/***********************************************/

/**** METHOD: MISSEDCALLLEDON ******************/
void missedcallledon(char* lastnr) {
  missedcallcount++;
  digitalWrite(MISSED_CALL_LED_PIN, HIGH);
  lcd.clear();
  lcd.print(missedcallcount);
  lcd.print(" missed calls");
  lcd.setCursor(0,1);
  // strcpy(lastmissednumber, lastnr);
  memcpy(lastmissednumber, lastnr,  LCD_MAX_CHARS+1);
  lcd.print(lastmissednumber);
  lcdstartdim();
}
/***********************************************/

/**** METHOD: LCDMISSEDCALL ********************/
void lcdmissedcall() {
  lcd.clear();
  lcd.print(missedcallcount);
  lcd.print(" missed calls");
  lcd.setCursor(0,1);
  lcd.print(lastmissednumber);
}
/***********************************************/

/**** METHOD: MISSEDCALLLEDOFF *****************/
void missedcallledoff() {
  if(missedcallcount > 0)
  {
    digitalWrite(MISSED_CALL_LED_PIN, LOW);
    missedcallcount = 0;
    lcddim();
  }
}
/***********************************************/

/**** METHOD: LCDDISPLAYTIME *******************/
void lcddisplaytime(unsigned long t) {
  word h = (t / 3600) % 24;
  byte m = (t / 60) % 60;
  byte s = t % 60;
  lcd.setCursor(0,1);

  if(!showprice)
  {
    lcd.print("    ");
  }

  if(h < 10)
  {
    lcd.print("0");
  }
  lcd.print(h);
  lcd.print(":");
  if(m < 10)
  {
    lcd.print("0");
  }
  lcd.print(m);
  lcd.print(":");
  if(s < 10)
  {
    lcd.print("0");
  }
  lcd.print(s);

  // Price calculation
  if(showprice && t > 0)
  {

    float currentprice = (1 + m + (h * 60)) * PRICEPERMINUTE;

    lcd.print(" ");
    lcd.print(currentprice);

    /*
  float battvcc = (float) readVcc() / 1000.0;
     char battbuffer[5] = "\0";
     dtostrf(battvcc, 4,2, battbuffer);
     */
  }
  else
  {
    lcd.print("    ");
  }
}
/***********************************************/

/**** METHOD: LOOP *****************************/
void loop() {

  if ((millis() - next) > RETRY_TIMEOUT)
  {
    next = millis();
#ifdef DEBUG
    Serial.println(F("Trying to connect..."));
#endif

    lcdconnecting();

    // replace hostname with name of machine running tcpserver.pl
    if (client.connect(FRITZBOX_HOSTNAME,FRITZBOX_PORT))
      //if (client.connect(IPAddress(192,168,171,1),1012))
    {

      if(client.connected())
      {
#ifdef DEBUG
        Serial.println(F("Connected successfully"));
#endif
        connectioncheck = millis();
        lcddim();
      }

      /*client.println("DATA from Client");
       while(client.available()==0)  
       {
       if (next - millis() < 0)
       goto close;
       }*/
      while(client.connected())
      {
        // if in a call
        if(call_connected && ((millis() - calllaststatus) > CALL_DURATION_UPDATE_INTERVAL))
        {
          unsigned long seconds = (millis() - callstart) / 1000;
          lcddisplaytime(seconds);
        }   

        // Dim LCD screen
        if(lcd_dimmer && ((millis() - lcdtimeoutstart) > LCD_DIM_TIMEOUT))
        {
          lcddim();
          lcd_dimmer = false;
        }

        if(!digitalRead(CLEAR_MISSED_CALL_BUTTON_PIN))
        {
          missedcallledoff(); 
        }

        int size;

        while((size = client.available()) > 0)
        { 
          uint8_t* msg = (uint8_t*)malloc(size);
          size = client.read(msg,size);
          msg[size-1] = '\0';

#ifdef DEBUG
          Serial.print(F("->Msg: "));
          Serial.println((char*)msg);
#endif

          // Copy of msg needed for strtok/strtok_r because of modification of original value (see https://www.securecoding.cert.org/confluence/display/seccode/STR06-C.+Do+not+assume+that+strtok%28%29+leaves+the+parse+string+unchanged)
          uint8_t* copymsgforsplit = (uint8_t*)malloc(size);
          memcpy(copymsgforsplit, msg, size);

          // Analyze incoming msg
          int i = 0;
          char *pch, *ptr;
          char type[11];
          pch = strtok_r ((char*)copymsgforsplit,";", &ptr);

          while (pch != NULL)
          {

#ifdef DEBUG
            Serial.print(F("    ->Splitted part "));
            Serial.print(i);
            Serial.print(F(": "));
            Serial.println(pch);
#endif

            switch(i)
            {
            case 0:    // Date and Time
              if(CLEAR_MISESED_CALL_ON_NEW_ACTION)
              {
                missedcallledoff();
              }
              lastcallwasmissedcall = false;
              lcd.clear();
              lcdon();
              lcd.print(pch[9]);
              lcd.print(pch[10]);
              lcd.print(pch[11]);
              lcd.print(pch[12]);
              lcd.print(pch[13]);
              lcd.print(" ");
              break;
            case 1:    // TYPE
              if (((strcmp(type, "RING")  == 0) && (strcmp(pch, "DISCONNECT")  == 0)) || (strstr((char*)msg, ";40;") && strcmp(type, "RING")  == 0 && strcmp(pch, "CONNECT") == 0))
              {
                missedcallledon(lastnumber);
                lastcallwasmissedcall = true;
              }         

              strcpy(type, pch);

              if(strcmp(type, "CALL")  == 0)
              {
                showprice = true;
              }

              if (!lastcallwasmissedcall)
              {
                lcd.print(type);
                lcd.setCursor(0,1);
              }                  
              break;
            case 2:    // ConnectionID
              // Currently not needed...
              break;
            case 3:
              if (strcmp(type, "RING")  == 0) // Who is calling
              {
                if(strstr((char*)msg, ";;")) // Unknown caller?
                {
                  lcd.print("Unknown caller");
                  strcpy(lastnumber, "Unknown caller\0");
                }
                else
                {
                  lcd.print(pch);
                  memcpy(lastnumber, pch, LCD_MAX_CHARS+1); // Geht das auch? Etwas sicherer, falls pch länger ist als lastnumber // ja, geht
                  lastnumber[LCD_MAX_CHARS+1] = '\0';
                  // strcpy(lastnumber, pch);
                  //  strcat(lastnumber, "\0");
                }
              }
              else if (strcmp(type, "DISCONNECT")  == 0) // How long was the call
              {
                //lcd.print(pch);
                //lcd.print(" seconds");
                call_connected = false;
                if(!lastcallwasmissedcall)
                {
                  lcddisplaytime(strtoul(pch, NULL, 0));
                  showprice = false;
                  lcdstartdim();
                }
              }

              break;
            case 4:
              if (strcmp(type, "CONNECT")  == 0) // Connected with number
              {
                if(!lastcallwasmissedcall)
                {
                  lcd.print(pch);
                }
                if(DISPLAY_CALL_DURATION)
                {
                  call_connected = true;
                  callstart = calllaststatus = millis();
                }
              }
              break;
            case 5:
              if (strcmp(type, "CALL")  == 0) // Calling number
              {
                lcd.print(pch);
              }
              break;
            default: 
              break;
            }
            i++;
            pch = strtok_r (NULL, ";", &ptr); // Split next part
          }

          free(msg);
          free(copymsgforsplit);
        }


        // Check connection
        if((millis() - connectioncheck) > CHECKCONNECTION)
        {
#ifdef DEBUG
          Serial.println(F("Checking connection..."));
#endif
          connectioncheck = millis();

          // Send dummy data to "refresh" connection state
          client.write("x");
        }
      }
      //close:
      //disconnect client
#ifdef DEBUG
      Serial.println(F("Disconnected"));
#endif
      //client.stop();
      lcdconnecting();
      resetEthernet();
    }
    else
    {
#ifdef DEBUG
      Serial.println(F("Connection failed, retrying..."));
#endif

      // Hier auch ein resetEthernet() nötig???

    }
  }
}










