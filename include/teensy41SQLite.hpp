#ifndef TEENSY_41_SQLITE
#define TEENSY_41_SQLITE

#include "sqlite3.h"

#include <SdFat.h>

extern SdFat* vfsFilesystem;

int beginSQLite(SdFat* io_filesystem);
int endSQLite();

#endif // TEENSY_41_SQLITE