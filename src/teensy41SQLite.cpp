#include "teensy41SQLite.hpp"

SdFat* vfsFilesystem = nullptr;

int beginSQLite(SdFat* io_filesystem)
{
  if (not io_filesystem)
  {
    return SQLITE_ERROR;
  }

  vfsFilesystem = io_filesystem;

  return sqlite3_initialize();
}

int endSQLite()
{
  vfsFilesystem = nullptr;

  return sqlite3_shutdown();
}
