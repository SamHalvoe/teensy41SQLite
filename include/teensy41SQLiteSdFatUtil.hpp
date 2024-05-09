#ifndef TEENSY_41_SQLITE_SDFAT_UTIL
#define TEENSY_41_SQLITE_SDFAT_UTIL

#ifdef USE_TEENSY_41_SQLITE_SDFAT_UTIL

#include <SdFat.h>

namespace T41SQLiteUtil
{
  int calculateSectorSizeInBytes(unsigned char in_lowBitsSectorSizeAsExponentForPowerOfTwo,
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
  int getSectorSizeFromSdCard(SdCard* in_sdCard)
  {
    csd_t csd;

    if (not in_sdCard->readCSD(&csd))
    {
      return -1;
    }

    if (csd.v1.csd_ver == 0)
    {
      return calculateSectorSizeInBytes(csd.v1.write_bl_len_low, csd.v1.write_bl_len_high);
    }
    else if (csd.v2.csd_ver == 1)
    {
      return calculateSectorSizeInBytes(csd.v2.write_bl_len_low, csd.v2.write_bl_len_high);
    }
    
    return -1;
  }
}

#endif // USE_TEENSY_41_SQLITE_SDFAT_UTIL

#endif // TEENSY_41_SQLITE_SDFAT_UTIL
