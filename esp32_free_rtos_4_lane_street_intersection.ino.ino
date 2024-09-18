/**********************************************************************
  Filename    : Free RTOS 4 Lane Street Intersection
  Description : Use 2 74HC595 to drive the LED Matrix display, 
  1 FreenoveRGB LED Module, 1 74HC595 to drive 4 digit 7-segment display
  Author      : Arya Tama
  Modification: 2024/09/18
**********************************************************************/

#include "Freenove_WS2812_Lib_for_ESP32.h"

// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

#define LEDS_COUNT  8  // The number of led
#define LEDS_PIN    0  // define the pin connected to the Freenove 8 led strip
#define CHANNEL     0  // RMT module channel

int latchPin = 2;          // Pin connected to ST_CP of 74HC595（Pin12）
int clockPin = 4;          // Pin connected to SH_CP of 74HC595（Pin11）
int dataPin = 15;          // Pin connected to DS of 74HC595（Pin14）

// Some constants and structs
const unsigned long redTimeout = 5000;
const unsigned long yellowTimeout = 2000;
const unsigned long greenTimeout = 5000;
const unsigned long updateTrafficFlowTimeout = 400;
const unsigned long displayTrafficLightsTimeout = 1;
const unsigned long displayTrafficFlowTimeout = 1;
const int ledColor[4][3] = {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 0, 0}};

enum lightState {
  Red,
  Yellow,
  Green
};

struct trafficLight{
  int posRed;
  int posYellow;
  int posGreen;
  lightState curState;
  lightState prevState;
};

struct carGenerator{
  int pos;
  int timeout;
};

// Shared resources
trafficLight upDownTL;
trafficLight leftRightTL;
int carPositions[] = {                     
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Globals
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);
carGenerator randomCarGenerator;
static SemaphoreHandle_t mutex;

//*****************************************************************************
// Tasks

void updateUpDownTrafficLightState(void *parameters) {

  // Loop forever
  while (1) {
    updateTrafficLightState(upDownTL);
    unsigned long delayValue = getDelayUpdateTrafficLightState(upDownTL);
    vTaskDelay(delayValue / portTICK_PERIOD_MS);
  }
}

void updateLeftRightTrafficLightState(void *parameters) {

  // Loop forever
  while (1) {
    updateTrafficLightState(leftRightTL);
    unsigned long delayValue = getDelayUpdateTrafficLightState(leftRightTL);
    vTaskDelay(delayValue / portTICK_PERIOD_MS);
  }
}

void displayTrafficLights(void *parameters) {

  // Loop forever
  while (1) {
    displayTrafficLight(upDownTL);
    displayTrafficLight(leftRightTL);
    vTaskDelay(displayTrafficLightsTimeout / portTICK_PERIOD_MS);
  }
}

void updateTrafficFlow(void *parameters) {

  // Loop forever
  while (1) {

    // Take mutex prior to critical section
    if (xSemaphoreTake(mutex, 0) == pdTRUE) {

      // Critical section
      int updatedUpDown = ((carPositions[0] & 0x08) << 4) | ((carPositions[1] & 0x08) << 3) | ((carPositions[2] & 0x08) << 2) | ((carPositions[3] & 0x08) << 1) | 
      (carPositions[4] & 0x08) | ((carPositions[5] & 0x08) >> 1) | ((carPositions[6] & 0x08) >> 2) | ((carPositions[7] & 0x08) >> 3);
      int updatedDownUp = ((carPositions[0] & 0x10) << 3) | ((carPositions[1] & 0x10) << 2) | ((carPositions[2] & 0x10) << 1) | (carPositions[3] & 0x10) | 
      ((carPositions[4] & 0x10) >> 1) | ((carPositions[5] & 0x10) >> 2) | ((carPositions[6] & 0x10) >> 3) | ((carPositions[7] & 0x10) >> 4);
      int updatedLeftRight = carPositions[3];
      int updatedRightLeft = carPositions[4];
      
      updateCarOnRoad(updatedUpDown, updatedDownUp, upDownTL);
      updateCarOnRoad(updatedLeftRight, updatedRightLeft, leftRightTL);
    
      for (int i = 0; i < 8; i++) {   // update 8 column
        int movingVertically = (((updatedUpDown >> (7-i)) & 1) << 3) | (((updatedDownUp >> (7-i)) & 1) << 4);
        if (i == 3) 
        {
          carPositions[i] = updatedLeftRight | movingVertically;
        }
        else if (i == 4) 
        {
          carPositions[i] = updatedRightLeft | movingVertically;
        }
        else
        {
          carPositions[i] = movingVertically;
        }
      }
  
      // Give mutex after critical section
      xSemaphoreGive(mutex);

      // Go to sleep
      vTaskDelay(updateTrafficFlowTimeout / portTICK_PERIOD_MS);

    } else {
      // Do something else
      Serial.println("Update Car Positions: Mutex Taken");
    }
  }
}

void generateCarsRandomly(void *parameters) {

  // Loop forever
  while (1) {

    // Take mutex prior to critical section
    if (xSemaphoreTake(mutex, 0) == pdTRUE) {

      // Critical section
      // Position: 16 possibilities splitted to four directions
      randomCarGenerator.pos = random(16);
      for (int i = 0; i < 4; i++) {
        bool isCarAdded = ((randomCarGenerator.pos >> (3-i)) & 1);
        if (i == 0) // Add car from left
        {
          bool isRearEmpty = !((carPositions[0] & 0x08) >> 3);
          if (isCarAdded && isRearEmpty)
          {
            carPositions[0] |= 0x08;
          }
        }
        else if (i == 1) // Add car from right
        {
          bool isRearEmpty = !((carPositions[7] & 0x10) >> 4);
          if (isCarAdded && isRearEmpty)
          {
            carPositions[7] |= 0x10;
          }
        }
        else if (i == 2) // Add car from up
        {
          bool isRearEmpty = !((carPositions[3] & 0x80) >> 7);
          if (isCarAdded && isRearEmpty)
          {
            carPositions[3] |= 0x80;
          }
        }
        else // Add car from down
        {
          bool isRearEmpty = !(carPositions[4] & 0x01);
          if (isCarAdded && isRearEmpty)
          {
            carPositions[4] |= 0x01;
          }
        }
      }
      randomCarGenerator.timeout = generateRandomTimeout();
  
      // Give mutex after critical section
      xSemaphoreGive(mutex);

      // Go to sleep
      vTaskDelay(randomCarGenerator.timeout*1000 / portTICK_PERIOD_MS);

    } else {
      // Do something else
      Serial.println("Generate Cars Randomly: Mutex Taken");
    }
  }
}

void displayTrafficFlow(void *parameters) {

  // Loop forever
  while (1) {
    int cols;
    cols = 0x01;
    for (int i = 0; i < 8; i++) {   // display 8 column data by scaning
      matrixRowsVal(carPositions[i]);// display the data in this column
      matrixColsVal(~cols);          // select this column
      delay(1);                     // display them for a period of time
      matrixRowsVal(0x00);          // clear the data of this column
      cols <<= 1;                   // shift"cols" 1 bit left to select the next column
    }
    vTaskDelay(displayTrafficFlowTimeout / portTICK_PERIOD_MS);
  }
}

//*****************************************************************************
// Helper functions

void updateTrafficLightState(trafficLight &inputTrafficLight){
  
  if (inputTrafficLight.curState == lightState::Red)
  {
    inputTrafficLight.prevState = inputTrafficLight.curState;
    inputTrafficLight.curState = lightState::Yellow;
  }
  else if (inputTrafficLight.curState == lightState::Yellow)
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
  }
  else // Green
  {
    inputTrafficLight.prevState = inputTrafficLight.curState;
    inputTrafficLight.curState = lightState::Yellow;
  }
}

unsigned long getDelayUpdateTrafficLightState(trafficLight &inputTrafficLight) {
  
  unsigned long delayUpdate;
  if (inputTrafficLight.curState == lightState::Red) {
    delayUpdate = redTimeout;
  }
  else if (inputTrafficLight.curState == lightState::Yellow) {
    delayUpdate = yellowTimeout;
  }
  else { // Green
    delayUpdate = greenTimeout;
  }

  return delayUpdate;
}

void displayTrafficLight(trafficLight inputTrafficLight) {
  
  if (inputTrafficLight.curState == lightState::Red)
  {
    strip.setLedColorData(inputTrafficLight.posRed, ledColor[0][0], ledColor[0][1], ledColor[0][2]);
    strip.setLedColorData(inputTrafficLight.posYellow, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
    strip.setLedColorData(inputTrafficLight.posGreen, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
  }
  else if (inputTrafficLight.curState == lightState::Yellow)
  {
    if (inputTrafficLight.prevState == lightState::Red)
    {
      strip.setLedColorData(inputTrafficLight.posRed, ledColor[0][0], ledColor[0][1], ledColor[0][2]);
    }
    else
    {
      strip.setLedColorData(inputTrafficLight.posRed, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
    }
    strip.setLedColorData(inputTrafficLight.posYellow, ledColor[1][0], ledColor[1][1], ledColor[1][2]);
    strip.setLedColorData(inputTrafficLight.posGreen, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
  }
  else // Green
  {
    strip.setLedColorData(inputTrafficLight.posRed, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
    strip.setLedColorData(inputTrafficLight.posYellow, ledColor[3][0], ledColor[3][1], ledColor[3][2]);
    strip.setLedColorData(inputTrafficLight.posGreen, ledColor[2][0], ledColor[2][1], ledColor[2][2]);
  }
  strip.show();   // Send color data to LED, and display.
  
}

void updateCarOnRoad(int &initialFirstTraffic, int &initialSecondTraffic, trafficLight inputTrafficLight) {
  
  if (inputTrafficLight.curState == lightState::Green) 
  {
    initialFirstTraffic >>= 1; 
    initialSecondTraffic <<= 1; 
  }
  else if (inputTrafficLight.curState == lightState::Yellow && inputTrafficLight.prevState == lightState::Green) 
  {
    updateCarOnRoadWhileNotGreen(initialFirstTraffic, initialSecondTraffic);
  }
  else
  {
    initialFirstTraffic &= 0xE7; 
    initialSecondTraffic &= 0xE7; 
    updateCarOnRoadWhileNotGreen(initialFirstTraffic, initialSecondTraffic);
  }
}

void updateCarOnRoadWhileNotGreen(int &initialFirstTraffic, int &initialSecondTraffic) {
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

int generateRandomTimeout() {
  return random(1, 7);
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

//*****************************************************************************

void setup() {
  // Serial line
  Serial.begin(115200);
  
  // Set pins to output
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
  // Init random car generator
  randomSeed(analogRead(0));
  randomCarGenerator.timeout = generateRandomTimeout();
  
  // Init LED for displaying traffic light
  strip.begin();
  strip.setBrightness(10);  
  
  // Init traffic light
  upDownTL.posRed = 5;
  upDownTL.posYellow = 4;
  upDownTL.posGreen = 3;
  upDownTL.prevState = lightState::Yellow;
  upDownTL.curState = lightState::Green;
  leftRightTL.posRed = 7;
  leftRightTL.posYellow = 0;
  leftRightTL.posGreen = 1;
  leftRightTL.prevState = lightState::Yellow;
  leftRightTL.curState = lightState::Red;

  // Create mutex before starting tasks
  mutex = xSemaphoreCreateMutex();

  // Start task 1
  xTaskCreatePinnedToCore(updateUpDownTrafficLightState,
                          "Update Up Down",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu); 

  // Start task 2
  xTaskCreatePinnedToCore(updateLeftRightTrafficLightState,
                          "Update Left Right",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu);

  // Start task 3
  xTaskCreatePinnedToCore(displayTrafficLights,
                          "Display Traffic Lights",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu); 

  // Start task 4
  xTaskCreatePinnedToCore(updateTrafficFlow,
                          "Update Traffic Flow",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu); 

  // Start task 5
  xTaskCreatePinnedToCore(generateCarsRandomly,
                          "Generate Cars Randomly",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu); 

  // Start task 6
  xTaskCreatePinnedToCore(displayTrafficFlow,
                          "Display Traffic Flow",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu); 
                          
  // Delete "setup and loop" task
  vTaskDelete(NULL);
}

void loop() {
  
}
