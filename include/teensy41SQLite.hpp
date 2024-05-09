#ifndef TEENSY_41_SQLITE
#define TEENSY_41_SQLITE

#include "sqlite3.h"

#include <FS.h>

class T41SQLite
{
  public:
    static const int ACCESS_FAILED = 0;
    static const int ACCESS_SUCCESFUL = 1;
    
  private:
    int m_sectorSize = 0;
    int m_deviceCharacteristics = 0;
    FS* m_filesystem = nullptr;
    String m_dbDirFullpath = "/";

  private:
    T41SQLite() = default;
    ~T41SQLite() = default;

    int calculateSectorSizeInBytes(unsigned char in_lowBitsSectorSizeAsExponentForPowerOfTwo,
                                   unsigned char in_highBitsSectorSizeAsExponentForPowerOfTwo) const;

  public:
    T41SQLite(const T41SQLite&) = delete;
    T41SQLite& operator=(const T41SQLite&) = delete;

    static T41SQLite& getInstance()
    {
      static T41SQLite instance;

      return instance;
    }

    int begin(FS* io_filesystem);
    int end();
    
    void setFilesystem(FS* io_filesystem);
    FS* getFilesystem();
    
    void setDBDirFullPath(const String& in_dbDirFullpath);
    const String& getDBDirFullPath() const;

    void resetSectorSize();
    void setSectorSize(int in_size);
    int getSectorSize() const;

    bool assumeSingleSectorWriteIsAtomic();
    void resetDeviceCharacteristics();
    void setDeviceCharacteristics(int in_ioCap);
    int getDeviceCharacteristics() const;
};

#endif // TEENSY_41_SQLITE
