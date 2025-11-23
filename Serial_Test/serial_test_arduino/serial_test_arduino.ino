#include <hd44780.h>
#include <hd44780ioClass/hd44780_pinIO.h>
#include <Arduino.h>


const int rs = 8, en = 9, d4 = 4, d5 = 5, d6 = 6, d7 = 7;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
int cc = 21;

// BAUDRATE = 115200

hd44780_pinIO lcd(rs, en, d4, d5, d6, d7);

void setup() {
  // put your setup code here, to run once: 
  lcd.begin(LCD_COLS, LCD_ROWS);
  Serial.begin(115200);

  lcd.lineWrap();
	lcd.print("Baud:115200");
  lcd.setCursor(0,1);
  lcd.write("RDY> ");

  //lcd.autoscroll();
  lcd.blink();

  
}

void loop() {
  
  if(Serial.available()){
    delay(90);
    
    char carattere = Serial.read();

    if(carattere != '\n' && carattere != '\r'){
      
      if(cc < (LCD_COLS*2)-1){
        cc++;
      } else {
        cc = 0;
        lcd.clear();
      }
      lcd.write(carattere);
    }
    else if (carattere == '\r'){
      if (cc < LCD_COLS){
        lcd.setCursor(0, 0); cc = 0;
      } else {
        lcd.setCursor(0, 1); cc = 17;
      }
    } else if (carattere == '\n') {
      if (cc < LCD_COLS){
        lcd.setCursor(0, 1); cc = 17;
      } else {
        lcd.setCursor(0, 0); cc = 0;
        lcd.clear();
      }
    }
  }
}
