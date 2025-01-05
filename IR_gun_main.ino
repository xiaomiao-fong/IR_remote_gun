#include "FS.h"

#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library
#include <string.h>
//#define DECODE_DENON
#include <IRremote.hpp>

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

#define DECODE_NEC
#define IR_RECEIVE_PIN 4

#define ENCODER_PIN_A 27
#define ENCODER_PIN_B 26
#define IR_SEND_PIN 32
#define LOAD_PIN 34
#define SHOOT_PIN 35

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL 0

#define LABEL1_FONT &FreeSansOblique12pt7b // Key label font 1
#define LABEL2_FONT &FreeSansBold12pt7b    // Key label font 2

#define KEY_TEXTSIZE 1   // Font size multiplier

enum DISPLAY_STATE {
  MENU,
  LOAD,
  WRITE,
  WRITE_DONE
};

DISPLAY_STATE current_display;
DISPLAY_STATE prev_display;

bool sw = 0, prev_sw = 0;
bool shoot = 0, prev_shoot = 0;


TFT_eSPI_Button load, collect, menu;
TFT_eSPI_Button bullets[4];
char bulletvalue[6][64];
char bullethexvalue[6][64];

volatile long encoderValue = 0; 
volatile unsigned long lastInterruptTime = 0; 
int datacount = 0;
int current_bullet = 0;
char datam[100][64];

void setup() {

  // Use serial port
  Serial.begin(115200);
  
  if (!SPIFFS.begin()) {
    Serial.println("formatting file system");
    SPIFFS.format();
    SPIFFS.begin();
  }
  
  add_test_file();

  // Initialise the TFT screen
  tft.init();

  // Set the rotation before we calibrate
  tft.setRotation(3);

  // Calibrate the touch screen and retrieve the scaling factors
  touch_calibrate();

  // Clear the screen
  tft.fillScreen(TFT_BLACK);

  prev_display = LOAD;
  current_display = MENU;
  handle_menu(0,0);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver
  
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(IR_SEND_PIN, OUTPUT);
  pinMode(LOAD_PIN, INPUT); 
  pinMode(SHOOT_PIN, INPUT); 
  IrSender.begin(IR_SEND_PIN);
  
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderISR, CHANGE);

}

void loop() {
  // put your main code here, to run repeatedly:

  sw = digitalRead(LOAD_PIN);
  shoot = digitalRead(SHOOT_PIN);
  uint16_t x = 0, y = 0; // To store the touch coordinates
  bool pressed = tft.getTouch(&x, &y);

  current_bullet = calculateBlock(encoderValue);

  if(shoot ^ prev_shoot){

    long rawData = 0;
    for(byte i = 2; bullethexvalue[current_bullet][i] != 0; ++i){

      long temp = bullethexvalue[current_bullet][i] - ((bullethexvalue[current_bullet][i] > '9') ? '7' : '0');
      rawData = rawData*16 + temp;
      
    }

    IrSender.sendNECRaw(rawData, 1);
    Serial.println(rawData, HEX);
    
  }

  prev_sw = sw;
  prev_shoot = shoot;
  
  if(pressed){
    
    tft.fillScreen(TFT_BLACK);
    
    switch (current_display){
  
      case MENU:
        handle_menu(x,y);
        break;

      case LOAD:
        handle_load(x,y);
        break;
      
      case WRITE:
        handle_write(x,y);
        break;

      default:
        handle_menu(x,y);
        break;
      
    }
  }

  prev_display = current_display;

  delay(10);
}

void handle_menu(uint16_t x, uint16_t y) {

  tft.setFreeFont(LABEL1_FONT);
  
  load.initButton(&tft, 80,
                          120, // x, y, w, h, outline, fill, text
                          120, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                          "LOAD", KEY_TEXTSIZE);
    
  collect.initButton(&tft, 240,
                          120, // x, y, w, h, outline, fill, text
                          120, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                          "WRITE", KEY_TEXTSIZE);
                          
  load.drawButton();
  collect.drawButton();

  if(load.contains(x,y)){

    current_display = LOAD;
    return;
    
  }

  if(collect.contains(x,y)){

    current_display = WRITE;
    return;
    
  }
  
}

void handle_load(uint16_t x, uint16_t y) {

  tft.setFreeFont(LABEL1_FONT);

  if (SPIFFS.exists("/testIRdata.txt")){

    datacount = 0;

    File IRdata = SPIFFS.open("/testIRdata.txt", FILE_READ);

    while (IRdata.available()){

      long long l = IRdata.readBytesUntil('\n', datam[datacount], sizeof(datam[datacount]));
      datam[datacount][l] = 0;
      ++datacount;
      
    }
    
  }
  
  menu.initButton(&tft, 50,
                          30, // x, y, w, h, outline, fill, text
                          80, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                          "MENU", KEY_TEXTSIZE);
                          
  menu.drawButton();

  if(menu.contains(x,y)){

    current_display = MENU;
    return;
    
  }

  Serial.println(datacount);

  tft.drawLine(100,0,100,240,TFT_BLUE);
  tft.drawString(String("No.") + String(current_bullet+1), 2, 140);
  tft.drawString(String(bulletvalue[current_bullet]), 2, 180);

  if(!datacount) return;


  for(byte i = 0; i < min(datacount,4); ++i){

    bullets[i].initButton(&tft, 125 + 80,
                          30 + 55*i, // x, y, w, h, outline, fill, text
                          120, 50, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                          datam[i], KEY_TEXTSIZE);
    
    bullets[i].drawButton();
    
    if(bullets[i].contains(x,y)){

      bool first = 1;
      char* token;
      token = strtok(datam[i], ",");
      strcpy(bulletvalue[current_bullet], token);
  
      while( token != NULL ){

        //tft.drawString(token, 125, 10 + i*(55) + ((first) ? 0 : 25));
        if(!first) strcpy(bullethexvalue[current_bullet], token);
        token = strtok(NULL, ",");
        first = 0;
        
      }
    }
    /*
    bool first = 1;
    char* token;
    token = strtok(datam[i], ",");

    while( token != NULL ){

      tft.drawString(token, 125, 10 + i*(55) + ((first) ? 0 : 25));
      token = strtok(NULL, ",");
      first = 0;
      
    }*/
    
  }

  return;
  
}

void handle_write(uint16_t x, uint16_t y){
   
   tft.setFreeFont(LABEL1_FONT);
   /*if(IrReceiver.decode()){
     Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
     IrReceiver.resume();
   }*/

  if (SPIFFS.exists("/testIRdata.txt")){

    datacount = 0;

    File IRdata = SPIFFS.open("/testIRdata.txt", FILE_APPEND);
    
    long long Indata; 
    while(!IrReceiver.decode());
    Indata = IrReceiver.decodedIRData.decodedRawData;
    Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
    IrReceiver.resume();
//    Indata = Serial.read();
    ;
    char name_[] = "name";
    
    if(IRdata.print(name_) && IRdata.print(",0x") && IRdata.println(Indata,HEX)){
      Serial.print(name_);
      Serial.print(",0x");
      Serial.println(Indata, HEX);
    }else{
      Serial.println("Nooo");
    }

    
    IRdata.close();
  }
  
  menu.initButton(&tft, 50,
                          30, // x, y, w, h, outline, fill, text
                          80, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                          "MENU", KEY_TEXTSIZE);
                          
  menu.drawButton();

  if(menu.contains(x,y)){

    current_display = MENU;
    return;
    
  }

  return;
}

void touch_calibrate() {
  
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    }
    else
    {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}

void add_test_file() {

  //if (!SPIFFS.exists("/testIRdata.txt")) return;
  
  File fileToAppend = SPIFFS.open("/testIRdata.txt", FILE_WRITE);
  
  if(!fileToAppend){
      Serial.println("There was an error opening the file for appending");
      return;
  }
  
  if(fileToAppend.println("test,0x472983")){
      Serial.println("File content was appended");
  } else {
      Serial.println("File append failed");
  }
  
  fileToAppend.close();

}
void encoderISR() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 5) { // 防反彈時間 5ms
    bool A = digitalRead(ENCODER_PIN_A);
    bool B = digitalRead(ENCODER_PIN_B);

    // 判斷方向
    if (A != B) {
      encoderValue++; // 順時針
    } else {
      encoderValue--; // 逆時針
    }

    if (encoderValue > 17) {
      encoderValue -= 17;
    } else if (encoderValue < 0) {
      encoderValue += 17;
    }
  }

  lastInterruptTime = interruptTime;
}

int calculateBlock(long value) {
  if (value >= 0 && value <= 2) {
    return 0; // 區塊 1
  } else if (value >= 3 && value <= 5) {
    return 1; // 區塊 2
  } else if (value >= 6 && value <= 8) {
    return 2; // 區塊 3
  } else if (value >= 9 && value <= 11) {
    return 3; // 區塊 4
  } else if (value >= 12 && value <= 14) {
    return 4; // 區塊 5
  } else if (value >= 15 && value <= 17){
    return 5; // 區塊 6
  }
}
