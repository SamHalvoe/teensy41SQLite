#include <Arduino.h>

#include "teensy41SQLite.hpp"

SdFat sd;

void setup()
{
  sd.begin(SdioConfig(FIFO_SDIO));
  T41SQLite::getInstance().setSectorSizeAuto(sd);
  T41SQLite::getInstance().begin();
}

void loop()
{

}
