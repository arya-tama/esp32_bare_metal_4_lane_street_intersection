/**********************************************************************
  Filename    : 4 Lane Street Intersection
  Description : Use 2 74HC595 to drive the LED Matrix display, 
  1 FreenoveRGB LED Module, 1 74HC595 to drive 4 digit 7-segment display
  Author      : Arya Tama
  Modification: 2024/09/18
**********************************************************************/
#include "Freenove_WS2812_Lib_for_ESP32.h"

#define LEDS_COUNT  8  // The number of led
#define LEDS_PIN    0  // define the pin connected to the Freenove 8 led strip
#define CHANNEL     0  // RMT module channel

int latchPin = 2;          // Pin connected to ST_CP of 74HC595（Pin12）
int clockPin = 4;          // Pin connected to SH_CP of 74HC595（Pin11）
int dataPin = 15;          // Pin connected to DS of 74HC595（Pin14）

// Traffic light
const unsigned long redTimeout = 5000;
const unsigned long yellowTimeout = 2000;
const unsigned long greenTimeout = 5000;
const int ledColor[4][3] = {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 0, 0}};

enum lightState {
  Red,
  Yellow,
  Green
};

struct trafficLight{
  lightState curState;
  lightState prevState;
  unsigned long startTime;
};

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);
trafficLight leftRightTL;
trafficLight upDownTL;

// Car traffic
const unsigned long updateTrafficTimeout = 400;
unsigned long updateTrafficStartTime;
int carPositions[] = {                     
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Car generator
struct carGenerator{
  int pos;
  int timeout;
  unsigned long startTime;
};
carGenerator randomCarGenerator;

void setup() {
  // serial line
  Serial.begin(115200);
  // set pins to output
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  // init random car generator
  randomSeed(analogRead(0));
  generateRandomTimeout();
  // init led for showing traffic light
  strip.begin();
  strip.setBrightness(10);  
  // init traffic light state
  leftRightTL.prevState = lightState::Yellow;
  leftRightTL.curState = lightState::Green;
  upDownTL.prevState = lightState::Yellow;
  upDownTL.curState = lightState::Red;
  // init all timers
  unsigned long initTime = millis();
  leftRightTL.startTime = initTime;
  upDownTL.startTime = initTime;
  updateTrafficStartTime = initTime;
  randomCarGenerator.startTime = initTime;
}

void loop() {

  // update trafic light state red -> yellow -> green -> ...
  updateTrafficLightState(leftRightTL);  
  updateTrafficLightState(upDownTL);

  // update car positions every 400 ms
  if (millis() - updateTrafficStartTime >= updateTrafficTimeout)
  {
    updateTraffic(leftRightTL, upDownTL);
    updateTrafficStartTime = millis();
  }

  // generate new cars randomly
  if (millis() - randomCarGenerator.startTime >= randomCarGenerator.timeout*1000)
  {
    addNewCars();
    generateRandomTimeout();
    randomCarGenerator.startTime = millis();
  }

  // display car traffic on led matrix
  displayTraffic();

  // show traffic light on LED
  showTrafficLight(leftRightTL, 5, 4, 3);
  showTrafficLight(upDownTL, 7, 0, 1);
}

void updateTrafficLightState(trafficLight &inputTrafficLight){
  if (inputTrafficLight.curState == lightState::Red && millis() - inputTrafficLight.startTime >= redTimeout)
  {
    inputTrafficLight.prevState = inputTrafficLight.curState;
    inputTrafficLight.curState = lightState::Yellow;
    inputTrafficLight.startTime = millis();
  }
  else if (inputTrafficLight.curState == lightState::Yellow && millis() - inputTrafficLight.startTime >= yellowTimeout)
  {
    if (inputTrafficLight.prevState == lightState::Red)
    {
      inputTrafficLight.prevState = inputTrafficLight.curState;
      inputTrafficLight.curState = lightState::Green;
    }
    else
    {
      inputTrafficLight.prevState = inputTrafficLight.curState;
      inputTrafficLight.curState = lightState::Red;
    }
    inputTrafficLight.startTime = millis();
  }
  else if (inputTrafficLight.curState == lightState::Green && millis() - inputTrafficLight.startTime >= greenTimeout)
  {
    inputTrafficLight.prevState = inputTrafficLight.curState;
    inputTrafficLight.curState = lightState::Yellow;
    inputTrafficLight.startTime = millis();
  }
}

void showTrafficLight(trafficLight inputTrafficLight, int posRed, int posYellow, int posGreen) {
  if (inputTrafficLight.curState == lightState::Red)
  {
    strip.setLedColorData(posRed, ledColor[0][0], ledColor[0][1], ledColor[0][2]);
    strip.setLedColorData(posYellow, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
    strip.setLedColorData(posGreen, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
  }
  else if (inputTrafficLight.curState == lightState::Yellow)
  {
    if (inputTrafficLight.prevState == lightState::Red)
    {
      strip.setLedColorData(posRed, ledColor[0][0], ledColor[0][1], ledColor[0][2]);
    }
    else
    {
      strip.setLedColorData(posRed, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
    }
    strip.setLedColorData(posYellow, ledColor[1][0], ledColor[1][1], ledColor[1][2]);
    strip.setLedColorData(posGreen, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
  }
  else
  {
    strip.setLedColorData(posRed, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
    strip.setLedColorData(posYellow, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
    strip.setLedColorData(posGreen, ledColor[2][0], ledColor[2][1], ledColor[2][2]);
  }

  strip.show();   // Send color data to LED, and display.
}

void generateRandomTimeout() {
  randomCarGenerator.timeout = random(1, 7);
}

void addNewCars() {
  // Position: 16 possibilities splitted to four directions
  randomCarGenerator.pos = random(16);
  for (int i = 0; i < 4; i++) {
    bool isCarAdded = ((randomCarGenerator.pos >> (3-i)) & 1);
    if (i == 0) // Add car from left
    {
      bool isCarLineEmpty = !((carPositions[0] & 0x08) >> 3);
      if (isCarAdded && isCarLineEmpty)
      {
        carPositions[0] |= 0x08;
      }
    }
    else if (i == 1) // Add car from right
    {
      bool isCarLineEmpty = !((carPositions[7] & 0x10) >> 4);
      if (isCarAdded && isCarLineEmpty)
      {
        carPositions[7] |= 0x10;
      }
    }
    else if (i == 2) // Add car from up
    {
      bool isCarLineEmpty = !((carPositions[3] & 0x80) >> 7);
      if (isCarAdded && isCarLineEmpty)
      {
        carPositions[3] |= 0x80;
      }
    }
    else // Add car from down
    {
      bool isCarLineEmpty = !(carPositions[4] & 0x01);
      if (isCarAdded && isCarLineEmpty)
      {
        carPositions[4] |= 0x01;
      }
    }
  }
}

void updateTraffic(trafficLight inputFirstTrafficLight, trafficLight inputSecondTrafficLight) {
  
  int updatedLeftRight = ((carPositions[0] & 0x08) << 4) | ((carPositions[1] & 0x08) << 3) | ((carPositions[2] & 0x08) << 2) | ((carPositions[3] & 0x08) << 1) | 
  (carPositions[4] & 0x08) | ((carPositions[5] & 0x08) >> 1) | ((carPositions[6] & 0x08) >> 2) | ((carPositions[7] & 0x08) >> 3);
  
  int updatedRightLeft = ((carPositions[0] & 0x10) << 3) | ((carPositions[1] & 0x10) << 2) | ((carPositions[2] & 0x10) << 1) | (carPositions[3] & 0x10) | 
  ((carPositions[4] & 0x10) >> 1) | ((carPositions[5] & 0x10) >> 2) | ((carPositions[6] & 0x10) >> 3) | ((carPositions[7] & 0x10) >> 4);

  int updatedUpDown = carPositions[3];
  
  int updatedDownUp = carPositions[4];
  
  updateCarOnRoad(updatedLeftRight, updatedRightLeft, inputFirstTrafficLight);
  updateCarOnRoad(updatedUpDown, updatedDownUp, inputSecondTrafficLight);

  for (int i = 0; i < 8; i++) {   // update 8 column
    int movingHorizontal = (((updatedLeftRight >> (7-i)) & 1) << 3) | (((updatedRightLeft >> (7-i)) & 1) << 4);
    if (i == 3) 
    {
      carPositions[i] = updatedUpDown | movingHorizontal;
    }
    else if (i == 4) 
    {
      carPositions[i] = updatedDownUp | movingHorizontal;
    }
    else
    {
      carPositions[i] = movingHorizontal;
    }
  }
}

void updateCarOnRoad(int &initialFirstTraffic, int &initialSecondTraffic, trafficLight inputTrafficLight) {
  
  if (inputTrafficLight.curState == lightState::Green) 
  {
    initialFirstTraffic >>= 1; 
    initialSecondTraffic <<= 1; 
  }
  else if (inputTrafficLight.curState == lightState::Yellow && inputTrafficLight.prevState == lightState::Green) 
  {
    updateCarWhileNotGreen(initialFirstTraffic, initialSecondTraffic);
  }
  else
  {
    initialFirstTraffic &= 0xE7; 
    initialSecondTraffic &= 0xE7; 
    updateCarWhileNotGreen(initialFirstTraffic, initialSecondTraffic);
  }
}

void updateCarWhileNotGreen(int &initialFirstTraffic, int &initialSecondTraffic) {
  // First direction
  initialFirstTraffic >>= 1; 
  int halfRear = (initialFirstTraffic & 0xF0) >> 4;
  int halfFront = initialFirstTraffic & 0x0F;
  if ((halfRear & 1) != 0)
  {
      if ((halfRear & 0x0E) == 0x04)
      {
        halfRear = 0x04 | 0x02;
      }
      else
      {
        halfRear = ((halfRear & 0x0E) << 1) | 0x02;
      }
  }
  initialFirstTraffic = (halfRear << 4) | halfFront;
  
  // Second direction
  initialSecondTraffic <<= 1; 
  halfRear = (initialSecondTraffic & 0xF0) >> 4;
  halfFront = initialSecondTraffic & 0x0F;
  if ((halfFront & 8) != 0)
  {
      if ((halfFront & 0x07) == 0x02)
      {
        halfFront = 0x04 | 0x02;
      }
      else
      {
        halfFront = 0x04 | ((halfFront & 0x07) >> 1);
      }
  }
  initialSecondTraffic = (halfRear << 4) | halfFront;
}

void displayTraffic() {
  int cols;
  cols = 0x01;
  for (int i = 0; i < 8; i++) {   // display 8 column data by scaning
    matrixRowsVal(carPositions[i]);// display the data in this column
    matrixColsVal(~cols);          // select this column
    delay(1);                     // display them for a period of time
    matrixRowsVal(0x00);          // clear the data of this column
    cols <<= 1;                   // shift"cols" 1 bit left to select the next column
  }
}

void matrixRowsVal(int value) {
  // make latchPin output low level
  digitalWrite(latchPin, LOW);
  // Send serial data to 74HC595
  shiftOut(dataPin, clockPin, LSBFIRST, value);
  // make latchPin output high level, then 74HC595 will update the data to parallel output
  digitalWrite(latchPin, HIGH);
}

void matrixColsVal(int value) {
  // make latchPin output low level
  digitalWrite(latchPin, LOW);
  // Send serial data to 74HC595
  shiftOut(dataPin, clockPin, MSBFIRST, value);
  // make latchPin output high level, then 74HC595 will update the data to parallel output
  digitalWrite(latchPin, HIGH);
}
