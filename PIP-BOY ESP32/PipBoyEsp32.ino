#include <AnimatedGIF.h>
#include "SPI.h"
#include "DHT.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <SD.h>
#include <TEA5767.h>
#include "termo.h"
#include "umid.h"
#include "rad.h"
TEA5767 radio = TEA5767();

#define BUFFPIXEL 20 

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

#ifndef BUILTIN_SDCARD
#define BUILTIN_SDCARD 0
#endif
#define TFT_DC 2
#define TFT_CS 12
#define TFT_RST 4
#define TFT_MISO 19         
#define TFT_MOSI 23           
#define TFT_CLK 18 
int but1 = 25;

int but2 = 27;
int led = 17;
int buzzer = 14;
int lerBut1;
#define LOG_PERIOD 15000  //Logging period in milliseconds, recommended value 15000-60000.
#define MAX_PERIOD 60000  //Maximum logging period without modifying this sketch
#define PERIOD 60000.0 // (60 sec) one minute measure period
volatile unsigned long CNT; // variable for counting interrupts from dosimeter
unsigned long counts;     //variable for GM Tube events
unsigned long cpm;        //variable for CPM
unsigned int multiplier;  //variable for calculation CPM in this sketch
unsigned long previousMillis;  //variable for time measurement
unsigned long dispPeriod; // variable for measuring time
unsigned long CPM; // variable for measuring CPM
int greenColor = 255;
int greenColorTemp = 255;
int greenColorHum = 0;
int greenColorRad = 0;
int blackColor = 0;
int tempHumColor = 255;
int menuDataNext = 0;
int radioDataNext = 0;
bool fill = false;
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;
String temp;
String hum;
int t;
int h;
int ligaDesligaLed = 0;
File f;
int nextMenu = 0;
int menuSelecionadoUm = 115;
int menuSelecionadoDois = 255;
int menuSelecionadoTres = 255;
int menuSelecionadoQuatro = 255;
int menuSelecionadoCinco = 255;
DHT dht(16,DHT22);
void * GIFOpenFile(const char *fname, int32_t *pSize)
{
  f = SD.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
}

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
} 

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
  
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} 

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{ 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} 

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth, iHeight;

    iWidth = pDraw->iWidth;
    iHeight = pDraw->iHeight;
    
    
    int initialX = (DISPLAY_WIDTH - iWidth) / 2;
    
    int initialY = (DISPLAY_HEIGHT - iHeight) / 2;

    if (initialX < 0) initialX = 0; // Evita valores negativos
    if (initialY < 0) initialY = 0; // Evita valores negativos

    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1 || iHeight < 1)
       return; 
    s = pDraw->pPixels;
    if (pDraw->ucDisposalMethod == 2) // restore to background color
    {
      for (x=0; x<iWidth; x++)
      {
        if (s[x] == pDraw->ucTransparent)
           s[x] = pDraw->ucBackground;
      }
      pDraw->ucHasTransparency = 0;
    }

    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
      int x, iCount;
      pEnd = s + iWidth;
      x = 0;
      iCount = 0; // count non-transparent pixels
      while(x < iWidth)
      {
        c = ucTransparent-1;
        d = usTemp;
        while (c != ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent) // done, stop
          {
            s--; // back up to treat it like transparent
          }
          else // opaque
          {
             *d++ = usPalette[c];
             iCount++;
          }
        } // while looking for opaque pixels
        if (iCount) // any opaque pixels?
        {
          tft.startWrite();
          tft.setAddrWindow(initialX + x, initialY + y, iCount, 1);
          tft.writePixels(usTemp, iCount, false, false);
          tft.endWrite();
          x += iCount;
          iCount = 0;
        }
        // no, look for a run of transparent pixels
        c = ucTransparent;
        while (c == ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent)
             iCount++;
          else
             s--; 
        }
        if (iCount)
        {
          x += iCount; // skip these
          iCount = 0;
        }
      }
    }
    else
    {
      s = pDraw->pPixels;
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      for (x=0; x<iWidth; x++)
        usTemp[x] = usPalette[*s++];
          
      tft.setAddrWindow(initialX, initialY + y, iWidth, 1);
      tft.writePixels(usTemp, iWidth, false, false);
      tft.endWrite();
    }
}



void GIFDrawTwo(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth + pDraw->iX > DISPLAY_WIDTH)
       iWidth = DISPLAY_WIDTH - pDraw->iX;
    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
       return; 
    s = pDraw->pPixels;
    if (pDraw->ucDisposalMethod == 2) // restore to background color
    {
      for (x=0; x<iWidth; x++)
      {
        if (s[x] == pDraw->ucTransparent)
           s[x] = pDraw->ucBackground;
      }
      pDraw->ucHasTransparency = 0;
    }

    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
      int x, iCount;
      pEnd = s + iWidth;
      x = 0;
      iCount = 0; // count non-transparent pixels
      while(x < iWidth)
      {
        c = ucTransparent-1;
        d = usTemp;
        while (c != ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent) // done, stop
          {
            s--; // back up to treat it like transparent
          }
          else // opaque
          {
             *d++ = usPalette[c];
             iCount++;
          }
        } // while looking for opaque pixels
        if (iCount) // any opaque pixels?
        {
          tft.setCursor(160,120);
          tft.startWrite();
          tft.setAddrWindow(pDraw->iX+x, y, iCount, 1);
          tft.writePixels(usTemp, iCount, false, false);
          tft.endWrite();
          x += iCount;
          iCount = 0;
        }
        // no, look for a run of transparent pixels
        c = ucTransparent;
        while (c == ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent)
             iCount++;
          else
             s--; 
        }
        if (iCount)
        {
          x += iCount; // skip these
          iCount = 0;
        }
      }
    }
    else
    {
      s = pDraw->pPixels;
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      
      for (x=0; x<iWidth; x++)
        usTemp[x] = usPalette[*s++];
    
      tft.startWrite();
      tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
      tft.writePixels(usTemp, iWidth, false, false);
      tft.endWrite();
    }
} /* GIFDraw() */

void setup() {
  Serial.begin(250000);
  
  while (!Serial);
  Wire.begin();
  dht.begin();
  
  pinMode(but1,INPUT_PULLUP);

  pinMode(but2,INPUT_PULLUP);
  pinMode(led,OUTPUT);
  pinMode(buzzer,OUTPUT);
  if (!SD.begin()) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("1. is a card inserted?");
    Serial.println("2. is your wiring correct?");
    Serial.println("3. did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    while (true);
  }

  // put your setup code here, to run once:
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  gif.begin(LITTLE_ENDIAN_PIXELS);
  if(gif.open("/gif/infive.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDrawTwo)){
    //digitalWrite(buzzer,HIGH);
    
    //tone(buzzer,  100,10000);
    
    GIFINFO gi;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    if (gif.getInfo(&gi)) {
      Serial.printf("frame count: %d\n", gi.iFrameCount);
      Serial.printf("duration: %d ms\n", gi.iDuration);
      Serial.printf("max delay: %d ms\n", gi.iMaxDelay);
      Serial.printf("min delay: %d ms\n", gi.iMinDelay);
      
      

    }
    tone(buzzer,100,11000);
      
    while (gif.playFrame(true, NULL)){
      
 
    }
    gif.close();
  }else{
    Serial.printf("Error opening file = %d\n", gif.getLastError());
    while (1){

    };
  }
  tft.fillScreen(ILI9341_BLACK);
  CNT = 0;
  CPM = 0;
  dispPeriod = 0;

  delay(1000);
  pinMode(39,INPUT);
  attachInterrupt(39,GetEvent,FALLING); // Event on pin 2
}

void loop() {
    //noTone(buzzer);
    nextMenu = map(analogRead(34),0,4095,0,4);

    if(nextMenu == 0){
      menuSelecionadoUm = 115;
      menuSelecionadoDois = 255;
      menuSelecionadoTres = 255;
      menuSelecionadoQuatro = 255;
      menuSelecionadoCinco = 255;
      tft.fillScreen(ILI9341_BLACK);
      fill = true;
      digitalWrite(buzzer,HIGH);
      delay(150);
      digitalWrite(buzzer,LOW);
      while(fill){
        nextMenu = map(analogRead(34),0,4095,0,4);
        if(!digitalRead(but2)){
          ligaDesligaLed++;
        }
        if(ligaDesligaLed%2 != 0){
          digitalWrite(led,HIGH);
        }else{
          digitalWrite(led,LOW);
        }
        statMenu();
        
        if(nextMenu != 0){
          break;
        }
      }
      
     
    }

    if(nextMenu == 1){
       menuSelecionadoUm = 255;
       menuSelecionadoDois = 115;
       menuSelecionadoTres = 255;
       menuSelecionadoQuatro = 255;
       menuSelecionadoCinco = 255;
      tft.fillScreen(ILI9341_BLACK);
      
      fill = true;
      digitalWrite(buzzer,HIGH);
      delay(150);
      digitalWrite(buzzer,LOW);
      while(fill){
        nextMenu = map(analogRead(34),0,4095,0,4);
        if(!digitalRead(but2)){
          ligaDesligaLed++;
        }
        if(ligaDesligaLed%2 != 0){
          digitalWrite(led,HIGH);
        }else{
          digitalWrite(led,LOW);
        }
        invMenu();
        if(nextMenu != 1){
          break;
        }
      }
    }

    if(nextMenu == 2){
       menuSelecionadoUm = 255;
      menuSelecionadoDois = 255;
      menuSelecionadoTres = 115;
      menuSelecionadoQuatro = 255;
      menuSelecionadoCinco = 255;
      tft.fillScreen(ILI9341_BLACK);
      digitalWrite(buzzer,HIGH);
      delay(150);
      digitalWrite(buzzer,LOW);
      fill = true;
      while(fill){
        nextMenu = map(analogRead(34),0,4095,0,4);
        lerBut1 = digitalRead(but1);

        if(!digitalRead(but2)){
          ligaDesligaLed++;
        }
        if(ligaDesligaLed%2 != 0){
          digitalWrite(led,HIGH);
        }else{
          digitalWrite(led,LOW);
        }
        t = dht.readTemperature();
        h = dht.readHumidity();
        temp = String(t)+"C";
        hum = String(h)+"%";
        dataMenu();
        
        if(nextMenu != 2){
          break;
        }
      }
      
    }

    if(nextMenu == 3){
       menuSelecionadoUm = 255;
      menuSelecionadoDois = 255;
      menuSelecionadoTres = 255;
      menuSelecionadoQuatro = 115;
      menuSelecionadoCinco = 255;
      tft.fillScreen(ILI9341_BLACK);
      digitalWrite(buzzer,HIGH);
      delay(150);
      digitalWrite(buzzer,LOW);
      fill = true;
      while(fill){
        nextMenu = map(analogRead(34),0,4095,0,4);
        
        if(!digitalRead(but2)){
          ligaDesligaLed++;
        }
        if(ligaDesligaLed%2 != 0){
          digitalWrite(led,HIGH);
        }else{
          digitalWrite(led,LOW);
        }
        mapMenu();
        if(nextMenu != 3){
          break;
        }
      }
    }   

    if(nextMenu == 4){
       menuSelecionadoUm = 255;
      menuSelecionadoDois = 255;
      menuSelecionadoTres = 255;
      menuSelecionadoQuatro = 255;
      menuSelecionadoCinco = 115;
      tft.fillScreen(ILI9341_BLACK);
      digitalWrite(buzzer,HIGH);
      delay(150);
      digitalWrite(buzzer,LOW);
      fill = true;
      while(fill){
        lerBut1 = digitalRead(but1);
      
        nextMenu = map(analogRead(34),0,4095,0,4);
        if(!digitalRead(but2)){
      ligaDesligaLed++;
    }
    if(ligaDesligaLed%2 != 0){
      digitalWrite(led,HIGH);
    }else{
      digitalWrite(led,LOW);
    }
        radioMenu();
        if(nextMenu != 4){
          break;
        }
      }
    }

}

void statMenu(){
radio.setMuted(true);
if(gif.open("/gif/bw.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)){
    
    Serial.println(digitalRead(but2));
    tft.setCursor(15, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoUm,0));
    tft.setTextSize (2);
   
    tft.println("STAT");
    tft.setCursor(75, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoDois,0));
    tft.setTextSize (2);
    
    tft.println("INV");
    tft.setCursor(135, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoTres,0));
    tft.setTextSize (2);
    tft.println("DATA");
    tft.setCursor(195, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoQuatro,0));
    tft.setTextSize (2);
    tft.println("MAP");
    tft.setCursor(255, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoCinco,0));
    tft.setTextSize (2);
    tft.println("RADIO");
    tft.fillRect(142, 55, 25, 5, tft.color565(0, 255,0));
    tft.fillRect(95, 105, 25, 5, tft.color565(0, 255,0));tft.fillRect(195, 105, 25, 5, tft.color565(0, 255,0));
    tft.fillRect(95, 155, 25, 5, tft.color565(0, 255,0));tft.fillRect(195, 155, 25, 5, tft.color565(0, 255,0));
    tft.fillRect(142, 175, 25, 5, tft.color565(0, 255,0));
 
    tft.setTextSize (1);
    tft.fillRect(6, 185, 65, 20, tft.color565(0, 255,0));
    tft.setCursor(8, 192);
    tft.setTextColor(tft.color565(0, 115,0));
    
    tft.println("STIMPAK(0)");  
    tft.fillRect(90, 185, 65, 20, tft.color565(0, 255,0));
    tft.setCursor(92, 192);
    tft.setTextColor(tft.color565(0, 115,0));
    
    tft.println("RADAWAY(0)");


    tft.fillRect(6, 210, 65, 20, tft.color565(0, 115,0));
    tft.setCursor(8, 217);
    tft.setTextColor(tft.color565(0, 255,0));
    
    tft.println("HP 115/115");  
    
    tft.fillRect(76, 210, 175, 20, tft.color565(0, 115,0));
    tft.setCursor(78, 217);
    tft.setTextColor(tft.color565(0, 255,0));
    
    tft.println("LEVEL 6");
    tft.fillRect(135, 215, 100, 10, tft.color565(0, 255,0));

    tft.fillRect(256, 210, 60, 20, tft.color565(0, 115,0));
    tft.setCursor(258, 217);
    tft.setTextColor(tft.color565(0, 255,0));
    
    tft.println("AP 90/90");
    GIFINFO gi;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    if (gif.getInfo(&gi)) {
      Serial.printf("frame count: %d\n", gi.iFrameCount);
      Serial.printf("duration: %d ms\n", gi.iDuration);
      Serial.printf("max delay: %d ms\n", gi.iMaxDelay);
      Serial.printf("min delay: %d ms\n", gi.iMinDelay);
    }
    while (gif.playFrame(true, NULL)){
      
    }
    gif.close();
  }else{
    Serial.printf("Error opening file = %d\n", gif.getLastError());
    while (1){
      
    };
  }
}

void invMenu(){
  radio.setMuted(true);
   Serial.println(digitalRead(but2));
  tft.setCursor(15, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoUm,0));
    tft.setTextSize (2);
   
    tft.println("STAT");
    tft.setCursor(75, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoDois,0));
    tft.setTextSize (2);
    
    tft.println("INV");
    tft.setCursor(135, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoTres,0));
    tft.setTextSize (2);
    tft.println("DATA");
    tft.setCursor(195, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoQuatro,0));
    tft.setTextSize (2);
    tft.println("MAP");
    tft.setCursor(255, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoCinco,0));
    tft.setTextSize (2);
    tft.println("RADIO");
    tft.fillRect(20, 55, 155, 30, tft.color565(0, 255,0));
    tft.setTextSize (1);
    tft.setCursor(25, 65);
    tft.setTextColor(tft.color565(0, 115,0));
    tft.println("10mm Pistol");

    tft.setCursor(25, 95);
    tft.setTextColor(tft.color565(0, 115,0));
    tft.println("Baseball Bat");

     tft.setCursor(25, 125);
    tft.setTextColor(tft.color565(0, 115,0));
    tft.println("Minigun");
    tft.setTextSize(1);
    tft.fillRect(6, 210, 65, 20, tft.color565(0, 115,0));
    tft.setCursor(8, 217);
    tft.setTextColor(tft.color565(0, 255,0));
    
    


    tft.println("* 133/240");  
    tft.fillRect(76, 210, 175, 20, tft.color565(0, 115,0));
    tft.setCursor(78, 217);
    tft.setTextColor(tft.color565(0, 255,0));

    tft.println("C 576");
    

    tft.fillRect(256, 210, 60, 20, tft.color565(0, 115,0));
    tft.setCursor(258, 217);
    tft.setTextColor(tft.color565(0, 255,0));
    
    tft.println("+ 18");
    delay(1000);
}

void dataMenu(){
    
radio.setMuted(true);
    
    
    tft.setCursor(15, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoUm,0));
    tft.setTextSize (2);
   
    tft.println("STAT");
    tft.setCursor(75, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoDois,0));
    tft.setTextSize (2);
    
    tft.println("INV");
    tft.setCursor(135, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoTres,0));
    tft.setTextSize (2);
    tft.println("DATA");
    tft.setCursor(195, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoQuatro,0));
    tft.setTextSize (2);
    tft.println("MAP");
    tft.setCursor(255, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoCinco,0));
    tft.setTextSize (2);
    tft.println("RADIO");
    tft.fillRect(20, 55, 135, 30, tft.color565(0, greenColorTemp,0));
    tft.setTextSize (1);
    tft.setCursor(25, 65);
    tft.setTextColor(tft.color565(0, 115,0));
    tft.println("Temperature");
    tft.fillRect(20, 85, 135, 30, tft.color565(0, greenColorHum,0));
    tft.setCursor(25, 95);
    tft.setTextColor(tft.color565(0, 115,0));
    tft.println("Humidity");
    tft.fillRect(20, 115, 135, 30, tft.color565(0, greenColorRad,0));
    tft.setCursor(25, 125);
    tft.setTextColor(tft.color565(0, 115,0));
    tft.println("Radiation");
    
    

    tft.setTextSize(1);
    tft.setCursor(8, 217);

    
    tft.setTextColor(tft.color565(0, 255,0));
    tft.fillRect(6, 210, 65, 20, tft.color565(0, 115,0));
    tft.println("10.23.2287");  
    
    tft.fillRect(76, 210, 225, 20, tft.color565(0, 115,0));
    tft.setCursor(78, 217);
    tft.setTextColor(tft.color565(0, 255,0));
    
    tft.println("6:02 PM");
    delay(1000);
    if(!lerBut1){
      menuDataNext++;
    }
    /*
    if(!lerBut1){
      menuDataNext--;
    }
    */
    if(menuDataNext == 0){
      greenColorTemp = 255;
      greenColorHum = 0;
      greenColorRad = 0;
      tft.drawRGBBitmap(210,60,termoBitmap,TERMO_WIDTH, TERMO_HEIGHT);
      tft.setTextSize (2);
      tft.setTextColor(tft.color565(0, 255,0)); 
      tft.setCursor(180, 65);
      tft.println(temp);
      delay(1000);
      tft.setTextColor(tft.color565(0, 0,0)); 
      tft.setCursor(180, 65);
      tft.println(temp);
    }
    if(menuDataNext == 1){
      greenColorTemp = 0;
      greenColorHum = 255;
      greenColorRad = 0;
      tft.drawRGBBitmap(210,60,umidBitmap,UMID_WIDTH, UMID_HEIGHT);
      tft.setTextSize (2);
      tft.setTextColor(tft.color565(0, 255,0)); 
      tft.setCursor(180, 65);
      tft.println(hum);
      delay(1000);
      tft.setTextColor(tft.color565(0, 0,0)); 
      tft.setCursor(180, 65);
      tft.println(hum);
      
    }
    if(menuDataNext == 2 ){
      greenColorTemp = 0;
      greenColorHum = 0;
      greenColorRad = 255;
      tft.drawRGBBitmap(210,60,radBitmap,RAD_WIDTH, RAD_HEIGHT);
      
      if (millis() >=dispPeriod + PERIOD) { // If one minute is over

        // Do something about accumulated CNT events....

        CPM = CNT;

        CNT = 0;
        dispPeriod = millis();
        tft.setTextSize (2);
      tft.setTextColor(tft.color565(0, 255,0)); 
      tft.setCursor(180, 65);
      tft.println(CPM);
      delay(4000);
      tft.setTextColor(tft.color565(0, 0,0)); 
      tft.setCursor(180, 65);
      tft.println(CPM);
      } 
        Serial.println(CNT);
    }
    if(menuDataNext == 3 || menuDataNext == 4){
      menuDataNext = -1;
    }
}

void mapMenu(){
radio.setMuted(true);


if(gif.open("/gif/map.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)){
       
    tft.setCursor(15, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoUm,0));
    tft.setTextSize (2);
   
    tft.println("STAT");
    tft.setCursor(75, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoDois,0));
    tft.setTextSize (2);
    
    tft.println("INV");
    tft.setCursor(135, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoTres,0));
    tft.setTextSize (2);
    tft.println("DATA");
    tft.setCursor(195, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoQuatro,0));
    tft.setTextSize (2);
    tft.println("MAP");
    tft.setCursor(255, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoCinco,0));
    tft.setTextSize (2);
    tft.println("RADIO");
    
    tft.setTextSize(1);
    tft.setCursor(8, 217);
    tft.setTextColor(tft.color565(0, 255,0));
    tft.fillRect(6, 210, 65, 20, tft.color565(0, 115,0));
    tft.println("10.23.2287");  
    
    tft.fillRect(76, 210, 180, 20, tft.color565(0, 115,0));
    tft.setCursor(78, 217);
    tft.setTextColor(tft.color565(0, 255,0));
    
    tft.println("6:02 PM");
    

    tft.fillRect(256, 210, 60, 20, tft.color565(0, 255,0));
    tft.setCursor(258, 217);
    tft.setTextColor(tft.color565(0, 115,0));
    
    tft.println("LOCAL MAP");
    GIFINFO gi;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    if (gif.getInfo(&gi)) {
      Serial.printf("frame count: %d\n", gi.iFrameCount);
      Serial.printf("duration: %d ms\n", gi.iDuration);
      Serial.printf("max delay: %d ms\n", gi.iMaxDelay);
      Serial.printf("min delay: %d ms\n", gi.iMinDelay);
    }
    while (gif.playFrame(true, NULL)){
      
    }
    gif.close();
    
  }else{
    Serial.printf("Error opening file = %d\n", gif.getLastError());
    while (1){

    };
  }
}

void radioMenu(){

    
   radio.setMuted(false);
    
    tft.setCursor(15, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoUm,0));
    tft.setTextSize (2);
    
    tft.println("STAT");
    tft.setCursor(75, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoDois,0));
    tft.setTextSize (2);
    
    tft.println("INV");
    tft.setCursor(135, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoTres,0));
    tft.setTextSize (2);
    tft.println("DATA");
    tft.setCursor(195, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoQuatro,0));
    tft.setTextSize (2);
    tft.println("MAP");
    tft.setCursor(255, 15);
    tft.setTextColor(tft.color565(0, menuSelecionadoCinco,0));
    tft.setTextSize (2);
    tft.println("RADIO");
    tft.fillRect(20, 55, 155, 30, tft.color565(0, greenColor,0));
    tft.setTextSize (1);
    tft.setCursor(25, 65);
    tft.setTextColor(tft.color565(0, 115,0));
    tft.println("Classic Radio 99.10");
     tft.fillRect(20, 85, 155, 30, tft.color565(0, blackColor,0));
    tft.setCursor(25, 95);
    tft.println("Diamond City Radio 108");
    tft.setTextColor(tft.color565(0, 255,0));
    tft.fillRect(0, 220, 320, 20, tft.color565(0, 115,0));
    Serial.println(lerBut1);
  
    delay(2000);

    if(!lerBut1){
      radioDataNext++;
    }
    if(radioDataNext == 0 || radioDataNext%2 == 0){
      greenColor = 255;
      blackColor = 0;
      radio.setFrequency(99.10);
    }
    if(radioDataNext%2 != 0){
      greenColor = 0;
      blackColor = 255;
      radio.setFrequency(108);
    }
}
void GetEvent(){ // Get Event from Device
CNT++;
}