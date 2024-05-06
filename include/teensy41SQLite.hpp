#ifndef TEENSY_41_SQLITE
#define TEENSY_41_SQLITE

#include "sqlite3.h"

#include <SdFat.h>

class T41SQLite
{
  private:
    int m_sectorSize = 0;
    int m_deviceCharacteristics = 0;

  private:
    T41SQLite() = default;
    ~T41SQLite() = default;

  public:
    T41SQLite(const T41SQLite&) = delete;
    T41SQLite& operator=(const T41SQLite&) = delete;

    static T41SQLite& getInstance()
    {
      static T41SQLite instance;

      return instance;
    }

    int begin();
    int end();

    void resetSectorSize();
    void setSectorSize(int in_size);
    bool setSectorSizeAuto(SdFat& in_sdFat);
    int getSectorSize() const;

    void resetDeviceCharacteristics();
    void setDeviceCharacteristics(int in_ioCap);
    int getDeviceCharacteristics() const;
};

#endif // TEENSY_41_SQLITE
