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

bool T41SQLite::setSectorSizeAuto(SdFat& in_sdFat)
{
  csd_t csd;

  if (not in_sdFat.card()->readCSD(&csd))
  {
    return false;
  }

  if (csd.v1.csd_ver == 0)
  {
    m_sectorSize = static_cast<int>(pow(2, csd.v1.read_bl_len));

    return true;
  }
  else if (csd.v2.csd_ver == 1)
  {
    m_sectorSize = static_cast<int>(pow(2, csd.v2.read_bl_len));

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
