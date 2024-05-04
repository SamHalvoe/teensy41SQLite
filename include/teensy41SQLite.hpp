#pragma once

#include "sqlite3.h"

#include <SdFat.h>

sqlite3_vfs *sqlite3_demovfs();
SdFat* vfsFilesystem;

int setupSQLite(SdFat* io_filesystem)
{
  if (not io_filesystem)
  {
    return SQLITE_ERROR;
  }

  vfsFilesystem = io_filesystem;

  return sqlite3_vfs_register(sqlite3_demovfs(), 1);
}
