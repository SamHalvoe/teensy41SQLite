#include "teensy41SQLite.hpp"

int beginSQLite()
{
  return sqlite3_initialize();
}

int endSQLite()
{
  return sqlite3_shutdown();
}
