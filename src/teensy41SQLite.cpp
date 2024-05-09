#include "teensy41SQLite.hpp"

T41SQLite::T41SQLite() : m_dbDirFullpath("/")
{}

int T41SQLite::begin()
{
  return sqlite3_initialize();
}

int T41SQLite::end()
{
  return sqlite3_shutdown();
}

void T41SQLite::setDBDirFullPath(const String& in_dbDirFullpath)
{
  m_dbDirFullpath = in_dbDirFullpath;
}

const String &T41SQLite::getDBDirFullPath() const
{
  return m_dbDirFullpath;
}

void T41SQLite::resetSectorSize()
{
  m_sectorSize = 0;
}

void T41SQLite::setSectorSize(int in_size)
{
  m_sectorSize = in_size;
}

int T41SQLite::calculateSectorSizeInBytes(unsigned char in_lowBitsSectorSizeAsExponentForPowerOfTwo,
                                          unsigned char in_highBitsSectorSizeAsExponentForPowerOfTwo) const
{
  int sectorSizeAsExponentForPowerOfTwo =
    (in_highBitsSectorSizeAsExponentForPowerOfTwo & 0b11110000) |
    (in_lowBitsSectorSizeAsExponentForPowerOfTwo  & 0b00001111);

  return static_cast<int>(pow(2, sectorSizeAsExponentForPowerOfTwo));
}

/*
SQLite is only concerned with the minimum write amount and so for the purposes of this article,
when we say "sector" we mean the minimum amount of data that can be written to mass storage in a single go.
(https://www.sqlite.org/atomiccommit.html)

Therefore we use csd.vX.write_bl_len_low and csd.vX.write_bl_len_high to calculate the sector size in bytes.
*/
bool T41SQLite::setSectorSizeAuto(SdFat& in_sdFat)
{
  csd_t csd;

  if (not in_sdFat.card()->readCSD(&csd))
  {
    return false;
  }

  if (csd.v1.csd_ver == 0)
  {
    m_sectorSize = calculateSectorSizeInBytes(csd.v1.write_bl_len_low, csd.v1.write_bl_len_high);

    return true;
  }
  else if (csd.v2.csd_ver == 1)
  {
    m_sectorSize = calculateSectorSizeInBytes(csd.v2.write_bl_len_low, csd.v2.write_bl_len_high);

    return true;
  }
  
  return false;
}

int T41SQLite::getSectorSize() const
{
  return m_sectorSize;
}

/*
This methods overwrites the values set with setDeviceCharacteristics!
It returns true if a valid sector is given. Otherwise it retruns false.
*/
bool T41SQLite::assumeSingleSectorWriteIsAtomic()
{
  switch (m_sectorSize)
  {
    case 512 * 1:
      m_deviceCharacteristics = SQLITE_IOCAP_ATOMIC512;
    break;

    case 512 * 2:
      m_deviceCharacteristics = SQLITE_IOCAP_ATOMIC1K;
    break;

    case 512 * 4:
      m_deviceCharacteristics = SQLITE_IOCAP_ATOMIC2K;
    break;

    case 512 * 8:
      m_deviceCharacteristics = SQLITE_IOCAP_ATOMIC4K;
    break;

    case 512 * 16:
      m_deviceCharacteristics = SQLITE_IOCAP_ATOMIC8K;
    break;

    case 512 * 32:
      m_deviceCharacteristics = SQLITE_IOCAP_ATOMIC16K;
    break;

    case 512 * 64:
      m_deviceCharacteristics = SQLITE_IOCAP_ATOMIC32K;
    break;

    case 512 * 128:
      m_deviceCharacteristics = SQLITE_IOCAP_ATOMIC64K;
    break;

    default:
      return false;
  }

  return true;
}

void T41SQLite::resetDeviceCharacteristics()
{
  m_deviceCharacteristics = 0;
}

void T41SQLite::setDeviceCharacteristics(int in_ioCap)
{
  m_deviceCharacteristics = in_ioCap;
}

int T41SQLite::getDeviceCharacteristics() const
{
  return m_deviceCharacteristics;
}
