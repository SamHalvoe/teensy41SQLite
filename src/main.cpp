#include <Arduino.h>
#include <SdFat.h>

#include "teensy41SQLite.hpp"

SdFat sd;

void setup()
{
  sd.begin(SdioConfig(FIFO_SDIO));
  beginSQLite();
}

void loop()
{

}
