#include <Arduino.h>

#include "teensy41SQLite.hpp"

SdFat sd;

void setupSerial(size_t in_serialBaudrate)
{
  Serial.begin(in_serialBaudrate);

  elapsedMillis initialisationTime;
  while (not Serial && initialisationTime < 15000);

  if (Serial)
  {
    Serial.println();
    Serial.println("Serial logging is ready (initialisationTime: " + String(initialisationTime) + " ms)");
  }
}

void delaySetup(uint8_t in_seconds = 15)
{
  Serial.print("Delay setup by ");
  Serial.print(in_seconds);
  Serial.print(" seconds: ");

  for (uint8_t seconds = 0; seconds < in_seconds; ++seconds)
  {
    if (seconds % 5 == 0)
    {
      Serial.print(" ");
    }

    delay(1000);
    Serial.print(".");
  }

  Serial.println(" Continue setup!");
}

void testSQLite()
{
  int rc = T41SQLite::getInstance().begin();

  if (rc != SQLITE_OK)
  {
    Serial.println("testSQLite - beginSQLite: Failed!");

    return;
  }

  sqlite3* db;
  Serial.println("testSQLite - sqlite3_open - begin");
  rc = sqlite3_open("test.db", &db);
  Serial.println("testSQLite - sqlite3_open - end");
  
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_exec(db, "CREATE TABLE Persons(PersonID INT);", NULL, 0, NULL);

    if (rc != SQLITE_OK)
    {
      int ext_rc = sqlite3_extended_errcode(db);
      Serial.print(ext_rc);
      Serial.print(": ");
      Serial.println(sqlite3_errstr(ext_rc));
    }
  }

  Serial.println("testSQLite - sqlite3_close - begin");
  sqlite3_close(db);
  Serial.println("testSQLite - sqlite3_close - end");
}

void setup()
{
  setupSerial(115200);
  delaySetup(10);

  sd.begin(SdioConfig(FIFO_SDIO));
  if (not sd.remove("test.db")) { Serial.println("remove test.db failed"); }
  if (not sd.remove("test.db-journal")) { Serial.println("remove test.db-journal failed"); }
  testSQLite();
}

void loop()
{

}
