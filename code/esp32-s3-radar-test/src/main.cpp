#include <Arduino.h>
 
#include "RD03D.h"

#define RX_PIN 15
#define TX_PIN 16
#define BAUD_RATE 256000
 
RD03D radar(RX_PIN, TX_PIN); // ESP RX/TX pins connected to sensor TX/RX

RD03D::RD03DMode radarMode = RD03D::SINGLE_TARGET;  // Change to RD03D::MULTI_TARGET for multi-target mode

void setup() {
    Serial.begin(115200);                  // Debugging
    Serial.println("RD-03D Radar Module Initialized"); 

    radar.initialize(radarMode);  // Initialize radar in single-target mode
    Serial.println("Radar module initialized successfully.");
    delay(100);
}
 
void loop() {

  static uint32_t lastSend = 0;

  // Plot information, We display data a bit less oftern
  if ( millis() > lastSend){

      // Call the task method frequently to check for new frames.
    radar.tasks();


    if(radarMode == RD03D::SINGLE_TARGET){
      TargetData* tgt = radar.getTarget();
      if(tgt->isValid()){
        Serial.print("Single Target Detected: ");
        tgt->printInfo();

        float distance = tgt->distance;
        float  angle = tgt->angle;
        float  speed = tgt->speed;
      }
      else{
        Serial.println("No target detected.");
      }
    }
    else if(radarMode == RD03D::MULTI_TARGET){
      uint8_t targets = radar.getTargetCount();

      if(targets > 0){
        Serial.printf("Detected %d target(s):\n", targets);

        for(uint8_t i = 0; i < targets; i++){
          TargetData* tgt = radar.getTarget(i);
          if(tgt->isValid()){

            Serial.printf("Target %d: ", i + 1);
            tgt->printInfo();

            float distance = tgt->distance;
            float  angle = tgt->angle;
            float  speed = tgt->speed;

          }
        }
      }
      else{
        Serial.println("No targets detected.");
      }
    }
    
    lastSend = millis() + 100;   // Update next tick every 0.1 second    
  }
}
      

 
 
