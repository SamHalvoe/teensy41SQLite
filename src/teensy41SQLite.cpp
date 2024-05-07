#include "teensy41SQLite.hpp"

int T41SQLite::begin()
{
  return sqlite3_initialize();
}

int T41SQLite::end()
{
  return sqlite3_shutdown();
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
