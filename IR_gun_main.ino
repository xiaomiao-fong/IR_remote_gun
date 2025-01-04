#include "FS.h"

#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL 1

#define LABEL1_FONT &FreeSansOblique12pt7b // Key label font 1
#define LABEL2_FONT &FreeSansBold12pt7b    // Key label font 2

#define KEY_TEXTSIZE 1   // Font size multiplier

#define DISP_X 1
#define DISP_Y 10
#define DISP_W 238
#define DISP_H 50
#define DISP_TSIZE 3
#define DISP_TCOLOR TFT_CYAN

enum DISPLAY_STATE {
  MENU,
  LOAD,
  WRITE,
  WRITE_DONE
};

DISPLAY_STATE current_display;
DISPLAY_STATE prev_display;

TFT_eSPI_Button load, collect;

void setup() {
  
  // Use serial port
  Serial.begin(115200);
  
  if (!SPIFFS.begin()) {
    Serial.println("formatting file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

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

}

void loop() {
  // put your main code here, to run repeatedly:
  
  uint16_t x = 0, y = 0; // To store the touch coordinates
  bool pressed = tft.getTouch(&x, &y);
  
  if(pressed){
    switch (current_display){
  
      case MENU:
        handle_menu(x,y);
        break;

      default:
        handle_menu(x,y);
        break;
      
    }
  }

  if (prev_display != current_display){
    tft.fillScreen(TFT_BLACK);
  }

  prev_display = current_display;

  delay(10);
}

void handle_menu(uint16_t x, uint16_t y) {

  tft.setFreeFont(LABEL1_FONT);
  
  load.initButton(&tft, 50,
                          30, // x, y, w, h, outline, fill, text
                          100, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                          "LOAD", KEY_TEXTSIZE);
    
  collect.initButton(&tft, 170,
                          30, // x, y, w, h, outline, fill, text
                          100, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                          "WRITE", KEY_TEXTSIZE);
                          
  load.drawButton();
  collect.drawButton();
  
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
