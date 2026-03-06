#include "Esp32PicoMini.h" // Esp32PicoMini.h is the hardware abstraction layer (HAL) to connect with ESP32 Pico Mini board.
#include "VectorHaptics.h" // VectorHaptics.h connects sublibraries included in the VH Library.
#include <VHBasePrimitives.h> // VHBasePrimitives.h is fundamental building blocks for creating haptic effects (using primitives like Vibration, Pulse, Pause)
#include <VHChannels.h> // VHChannels.h allows developers to create multiple channels to play haptic effects.

// Defining the VectorHaptics and VHBasePrimitives objects to access the haptic channel and base primitives.
VectorHaptics<Esp32PicoMini> vh;
VHBasePrimitives bp;

// Creating a mono channel with channel number, GPIO pin, and channel tags. Channel tags are String ussed to identify the channel.
VHChannel chnl1(1, 25, {"Left channel", "Channel 1", "Left", "Finger"}, 15);

VHChannels channelList({&chnl1}); // Adding all channels to a channel list

// tof stuff
#include <Wire.h>
#include <VL53L0X.h>

VL53L0X sensor;

#define SDA_PIN 21
#define SCL_PIN 22

void setup() {
  // put your setup code here, to run once:
  vh.Init({&channelList,&bp}); // initializing channel list and core
  xTaskCreate(LoopDriver,"LoopDriver",8000,NULL,configMAX_PRIORITIES,NULL);
  Serial.begin(115200);
  delay(500); // give time for Serial to get ready LOL
  Serial.println("Program Start");
  Wire.begin(SDA_PIN, SCL_PIN);

  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial.println("Failed to detect and initialize sensor!");
    while (1) {}
  }
  // Start continuous back-to-back mode (take readings as
  // fast as possible).  To use continuous timed mode
  // instead, provide a desired inter-measurement period in
  // ms (e.g. sensor.startContinuous(100)).
  sensor.startContinuous();
}

void loop() {
  // put your main code here, to run repeatedly:
  // EffectDriver() function listens to incoming serial commands and executes the commands based on the input. Need to call this function when working with serial communication.
  vh.EffectDriver();

  // Serial.print(sensor.readRangeContinuousMillimeters());
  // if (sensor.timeoutOccurred()) { Serial.print(" TIMEOUT"); }
  // Serial.println();
  // delay(100);
}

void LoopDriver(void *param)
{
    while (true)
    {
      int sum = 0;
      int count = 1;
      for (int i = 0; i < count; i++) {
        int reading = sensor.readRangeContinuousMillimeters();
        sum += reading;
      }
      float dist = sum/float(count);
      dist = constrain(dist, 0, 1000);
      float intensity = 1.0 - (dist / 1000.0);
      intensity = constrain(intensity, 0.0, 1.0);
      // VIBRATE(FREQ, INTENSITY, DURATION, SHARPNESS));
      vh.play(VIBRATE(100, intensity, 30, 1));
      delay(10);
    }
}