/*
** 2010 April 7
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file implements an example of a simple VFS implementation that 
** omits complex features often not required or not possible on embedded
** platforms.  Code is included to buffer writes to the journal file, 
** which can be a significant performance improvement on some embedded
** platforms.
**
** OVERVIEW
**
**   The code in this file implements a minimal SQLite VFS that can be 
**   used on Teensy 4.x The following system calls are used:
**
**    File-system: open(), remove() <SdFat.h>
**    File IO:     open(), read(), write(), fsync() --> sync(),
**                 close(), fstat() --> size() (only used to get file size) <SdFat.h>
**    Other:       delayMicroseconds(), elapsedMicros <elapsedMillis.h>, now() <TimeLib.h>
**
**   The following VFS features are omitted:
**
**     1. File locking. The user must ensure that there is at most one
**        connection to each database when using this VFS. Multiple
**        connections to a single shared-cache count as a single connection
**        for the purposes of the previous statement.
**
**     2. The loading of dynamic extensions (shared libraries).
**
**     3. Temporary files. The user must configure SQLite to use in-memory
**        temp files when using this VFS. The easiest way to do this is to
**        compile with:
**
**          -DSQLITE_TEMP_STORE=3
**
**   It is assumed that the system uses UNIX-like path-names. Specifically,
**   that '/' characters are used to separate path components and that
**   a path-name is a relative path unless it begins with a '/'. And that
**   no UTF-8 encoded paths are greater than 512 bytes in length.
**
** JOURNAL WRITE-BUFFERING
**
**   To commit a transaction to the database, SQLite first writes rollback
**   information into the journal file. This usually consists of 4 steps:
**
**     1. The rollback information is sequentially written into the journal
**        file, starting at the start of the file.
**     2. The journal file is synced to disk.
**     3. A modification is made to the first few bytes of the journal file.
**     4. The journal file is synced to disk again.
**
**   Most of the data is written in step 1 using a series of calls to the
**   VFS xWrite() method. The buffers passed to the xWrite() calls are of
**   various sizes. For example, as of version 3.6.24, when committing a 
**   transaction that modifies 3 pages of a database file that uses 4096 
**   byte pages residing on a media with 512 byte sectors, SQLite makes 
**   eleven calls to the xWrite() method to create the rollback journal, 
**   as follows:
**
**             Write offset | Bytes written
**             ----------------------------
**                        0            512
**                      512              4
**                      516           4096
**                     4612              4
**                     4616              4
**                     4620           4096
**                     8716              4
**                     8720              4
**                     8724           4096
**                    12820              4
**             ++++++++++++SYNC+++++++++++
**                        0             12
**             ++++++++++++SYNC+++++++++++
**
**   On many operating systems, this is an efficient way to write to a file.
**   However, on some embedded systems that do not cache writes in OS 
**   buffers it is much more efficient to write data in blocks that are
**   an integer multiple of the sector-size in size and aligned at the
**   start of a sector.
**
**   To work around this, the code in this file allocates a fixed size
**   buffer of SQLITE_DEMOVFS_BUFFERSZ using sqlite3_malloc() whenever a 
**   journal file is opened. It uses the buffer to coalesce sequential
**   writes into aligned SQLITE_DEMOVFS_BUFFERSZ blocks. When SQLite
**   invokes the xSync() method to sync the contents of the file to disk,
**   all accumulated data is written out, even if it does not constitute
**   a complete block. This means the actual IO to create the rollback 
**   journal for the example transaction above is this:
**
**             Write offset | Bytes written
**             ----------------------------
**                        0           8192
**                     8192           4632
**             ++++++++++++SYNC+++++++++++
**                        0             12
**             ++++++++++++SYNC+++++++++++
**
**   Much more efficient if the underlying OS is not caching write 
**   operations.
*/

// ---- remaining sqlite demovfs includes ----

#include <assert.h>

// ---- arduino teensy 4.1 vfs includes ----

#include "teensy41SQLite.hpp"

#include <elapsedMillis.h>
#include <TimeLib.h>

using FileVFS = File; // define FileVFS class, which actually interfaces with the storage hardware (e.g. a sd card)
#define FS_VFS_NAME "t41_vfs" // teensy 4.1 vfs

/*
** Size of the write buffer used by journal files in bytes.
*/
#ifndef SQLITE_DEMOVFS_BUFFERSZ
# define SQLITE_DEMOVFS_BUFFERSZ 8192
#endif

/*
** The maximum pathname length supported by this VFS.
*/
#define MAXPATHNAME 512

/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type DemoFile.
*/
typedef struct DemoFile DemoFile;
struct DemoFile {
  sqlite3_file base;              /* Base class. Must be first. */
  FileVFS* fd;                    /* File descriptor */

  char *aBuffer;                  /* Pointer to malloc'd buffer */
  int nBuffer;                    /* Valid bytes of data in zBuffer */
  sqlite3_int64 iBufferOfst;      /* Offset in file of zBuffer[0] */
};

/*
** Write directly to the file passed as the first argument. Even if the
** file has a write-buffer (DemoFile.aBuffer), ignore it.
*/
static int demoDirectWrite(
  DemoFile *p,                    /* File handle */
  const void *zBuf,               /* Buffer containing data to write */
  int iAmt,                       /* Size of data to write in bytes */
  sqlite_int64 iOfst              /* File offset to write to */
){
  Serial.println("VFS_DEBUG_DIRECT_WRITE");

  if (iAmt < 0) // is a size type, must not be less than zero
  {
    return SQLITE_IOERR_WRITE;
  }

  if (not p->fd->seek(iOfst, SeekSet))
  {
    return SQLITE_IOERR_WRITE;
  }

  size_t toWrite = static_cast<size_t>(iAmt);
  size_t nWrite = p->fd->write(zBuf, toWrite);

  if (nWrite != toWrite)
  {
    return SQLITE_IOERR_WRITE;
  }

  p->fd->flush();

  Serial.print("VFS_DEBUG_DIRECT_WRITE_SIZE: ");
  Serial.println(p->fd->size());

  return SQLITE_OK;
}

/*
** Flush the contents of the DemoFile.aBuffer buffer to disk. This is a
** no-op if this particular file does not have a buffer (i.e. it is not
** a journal file) or if the buffer is currently empty.
*/
static int demoFlushBuffer(DemoFile *p)
{
  Serial.print("VFS_DEBUG_FLUSH_BUFFER ");
  Serial.println(p->nBuffer);

  if (p->nBuffer)
  {
    int rc = demoDirectWrite(p, p->aBuffer, p->nBuffer, p->iBufferOfst);
    p->nBuffer = 0;

    if (rc != SQLITE_OK)
    {
      return rc;
    }
  }

  return SQLITE_OK;
}

/*
** Close a file.
*/
static int demoClose(sqlite3_file *pFile)
{
  DemoFile *p = (DemoFile*)pFile;
  int rc = demoFlushBuffer(p);
  sqlite3_free(p->aBuffer);

  Serial.println("VFS_DEBUG_CLOSE");
  Serial.print("VFS_DEBUG_CLOSE_FILE ");
  Serial.println(p->fd->name());

  p->fd->close();

  return rc;
}

/*
** Read data from a file.
*/
static int demoRead(
  sqlite3_file *pFile, 
  void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
)
{
  Serial.println("VFS_DEBUG_READ - BEGIN");
  Serial.print("VFS_DEBUG_READ_iAMT ");
  Serial.println(iAmt);
  Serial.print("VFS_DEBUG_READ_OFFSET ");
  Serial.println(iOfst);

  DemoFile *p = (DemoFile*)pFile;

  /* Flush any data in the write buffer to disk in case this operation
  ** is trying to read data the file-region currently cached in the buffer.
  ** It would be possible to detect this case and possibly save an 
  ** unnecessary write here, but in practice SQLite will rarely read from
  ** a journal file when there is data cached in the write-buffer.
  */
  int rc = demoFlushBuffer(p);

  if (rc != SQLITE_OK)
  {
    return rc;
  }
  
  Serial.print("VFS_DEBUG_READ_FILE_SIZE ");
  Serial.println(p->fd->size());
  Serial.print("VFS_DEBUG_READ_CUR ");
  Serial.println(p->fd->position());

  uint64_t seekPosition = min(static_cast<uint64_t>(iOfst), p->fd->size());
  if (not p->fd->seek(seekPosition, SeekSet))
  {
    Serial.println("VFS_DEBUG_READ_SEEK_FAIL");
    return SQLITE_IOERR_READ;
  }

  Serial.print("VFS_DEBUG_READ_CUR_AFTER_SEEK ");
  Serial.println(p->fd->position());

  size_t toRead = static_cast<size_t>(iAmt);
  size_t nRead = p->fd->read(zBuf, toRead);
  Serial.print("VFS_DEBUG_READ_FILE_READ_RETURN_VALUE ");
  Serial.println(nRead);

  if (nRead == toRead)
  {
    Serial.println("VFS_DEBUG_READ - END (OK)");

    return SQLITE_OK;
  }
  else if (nRead >= 0)
  {
    if (nRead < toRead)
    {
      memset(&((char*)zBuf)[nRead], 0, toRead - nRead);
    }

    Serial.println("VFS_DEBUG_READ - END (SQLITE_IOERR_SHORT_READ)");

    return SQLITE_IOERR_SHORT_READ; // ok
  }
  
  Serial.println("VFS_DEBUG_READ - END (ERROR)");

  return SQLITE_IOERR_READ; // nRead < 0 --> call to p->fd->read(zBuf, iAmt) failed
}

/*
** Write data to a crash-file.
*/
static int demoWrite(
  sqlite3_file *pFile, 
  const void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
){
  DemoFile *p = (DemoFile*)pFile;

  Serial.println("VFS_DEBUG_WRITE");
  
  if (p->aBuffer)
  {
    char *z = (char *)zBuf;       /* Pointer to remaining data to write */
    int n = iAmt;                 /* Number of bytes at z */
    sqlite3_int64 i = iOfst;      /* File offset to write to */

    while (n > 0)
    {
      int nCopy;                  /* Number of bytes to copy into buffer */

      /* If the buffer is full, or if this data is not being written directly
      ** following the data already buffered, flush the buffer. Flushing
      ** the buffer is a no-op if it is empty.  
      */
      if (p->nBuffer == SQLITE_DEMOVFS_BUFFERSZ ||
          p->iBufferOfst + p->nBuffer != i)
      {
        int rc = demoFlushBuffer(p);
        if (rc != SQLITE_OK)
        {
          return rc;
        }
      }

      assert(p->nBuffer == 0 || p->iBufferOfst + p->nBuffer == i);
      p->iBufferOfst = i - p->nBuffer;

      /* Copy as much data as possible into the buffer. */
      nCopy = SQLITE_DEMOVFS_BUFFERSZ - p->nBuffer;
      if (nCopy > n)
      {
        nCopy = n;
      }
      memcpy(&p->aBuffer[p->nBuffer], z, nCopy);
      p->nBuffer += nCopy;

      n -= nCopy;
      i += nCopy;
      z += nCopy;
    }
  }
  else
  {
    return demoDirectWrite(p, zBuf, iAmt, iOfst);
  }

  return SQLITE_OK;
}

/* (From SQLite documentation:)
** The xTruncate method truncates a file to be nByte bytes in length. If the file is already nByte bytes or less in length then this method is a no-op.
** The xTruncate method returns SQLITE_OK on success and SQLITE_IOERR_TRUNCATE if anything goes wrong. 
*/
static int demoTruncate(sqlite3_file *pFile, sqlite_int64 size)
{
  DemoFile* p = (DemoFile*)pFile;
  size_t reducedSize = static_cast<size_t>(size);

  if (p->fd->size() > reducedSize)
  {
    return p->fd->truncate(reducedSize) ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
  }

  return SQLITE_OK;
}

/*
** Sync the contents of the file to the persistent media.
*/
static int demoSync(sqlite3_file *pFile, int flags)
{
  DemoFile* p = (DemoFile*)pFile;
  int rc = demoFlushBuffer(p);
  
  if (rc != SQLITE_OK)
  {
    return rc;
  }
  
  p->fd->flush();

  return SQLITE_OK;
}

/*
** Write the size of the file in bytes to *pSize.
*/
static int demoFileSize(sqlite3_file *pFile, sqlite_int64 *pSize)
{
  DemoFile *p = (DemoFile*)pFile;
  /* Flush the contents of the buffer to disk. As with the flush in the
  ** demoRead() method, it would be possible to avoid this and save a write
  ** here and there. But in practice this comes up so infrequently it is
  ** not worth the trouble.
  */
  int rc = demoFlushBuffer(p);

  if (rc != SQLITE_OK)
  {
    return rc;
  }
  
  *pSize = p->fd->size();

  return SQLITE_OK;
}

/*
** Locking functions. The xLock() and xUnlock() methods are both no-ops.
** The xCheckReservedLock() always indicates that no other process holds
** a reserved lock on the database file. This ensures that if a hot-journal
** file is found in the file-system it is rolled back.
*/
static int demoLock(sqlite3_file *pFile, int eLock)
{
  return SQLITE_OK;
}
static int demoUnlock(sqlite3_file *pFile, int eLock)
{
  return SQLITE_OK;
}
static int demoCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
  *pResOut = 0;
  return SQLITE_OK;
}

/*
** No xFileControl() verbs are implemented by this VFS.
*/
static int demoFileControl(sqlite3_file *pFile, int op, void *pArg)
{
  return SQLITE_NOTFOUND;
}

/*
** The xSectorSize() and xDeviceCharacteristics() methods. These two
** may return special values allowing SQLite to optimize file-system 
** access to some extent. But it is also safe to simply return 0.
*/
static int demoSectorSize(sqlite3_file *pFile)
{
  return T41SQLite::getInstance().getSectorSize();
}

static int demoDeviceCharacteristics(sqlite3_file *pFile)
{
  return T41SQLite::getInstance().getDeviceCharacteristics();
}

/*
** Open a file handle.
*/
static int demoOpen(
  sqlite3_vfs *pVfs,              /* VFS */
  const char *zName,              /* File to open, or 0 for a temp file */
  sqlite3_file *pFile,            /* Pointer to DemoFile struct to populate */
  int flags,                      /* Input SQLITE_OPEN_XXX flags */
  int *pOutFlags                  /* Output SQLITE_OPEN_XXX flags (or NULL) */
){
  static const sqlite3_io_methods demoio = {
    1,                            /* iVersion */
    demoClose,                    /* xClose */
    demoRead,                     /* xRead */
    demoWrite,                    /* xWrite */
    demoTruncate,                 /* xTruncate */
    demoSync,                     /* xSync */
    demoFileSize,                 /* xFileSize */
    demoLock,                     /* xLock */
    demoUnlock,                   /* xUnlock */
    demoCheckReservedLock,        /* xCheckReservedLock */
    demoFileControl,              /* xFileControl */
    demoSectorSize,               /* xSectorSize */
    demoDeviceCharacteristics     /* xDeviceCharacteristics */
  };

  Serial.println("VFS_DEBUG_OPEN");

  DemoFile* p = (DemoFile*)pFile; /* Populate this structure */
  char* aBuf = 0;

  if (zName == 0)
  {
    return SQLITE_IOERR;
  }

  Serial.print("VFS_DEBUG_OPEN_FILE ");
  Serial.println(zName);

  if (flags & SQLITE_OPEN_MAIN_JOURNAL)
  {
    aBuf = (char*)sqlite3_malloc(SQLITE_DEMOVFS_BUFFERSZ);
    
    if (not aBuf)
    {
      return SQLITE_NOMEM;
    }
  }
  
  uint8_t openMode = (flags & SQLITE_OPEN_READONLY) ? FILE_READ : FILE_WRITE;
  memset(p, 0, sizeof(DemoFile));
  p->fd = new FileVFS(T41SQLite::getInstance().getFilesystem()->open(zName, openMode));
  
  if (not p->fd) // check if file is open
  {
    sqlite3_free(aBuf);
    return SQLITE_CANTOPEN;
  }

  p->aBuffer = aBuf;

  if (pOutFlags)
  {
    *pOutFlags = flags;
  }

  p->base.pMethods = &demoio;

  return SQLITE_OK;
}

/*
** Delete the file identified by argument zPath. If the dirSync parameter
** is non-zero, then WE SHOULD ensure the file-system modification to delete the
** file has been synced to disk before returning.
** BUT CANNOT ensure the sync, therefore we ignore the dirSync parameter.
*/
static int demoDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
  Serial.print("VFS_DEBUG_DELETE_PATH ");
  Serial.println(zPath);

  if (not T41SQLite::getInstance().getFilesystem()->remove(zPath))
  {
    return SQLITE_IOERR_DELETE;
  }
  
  return SQLITE_OK;
}

/*
** Query the file-system to see if the named file exists, is readable or
** is both readable and writable.
*/
static int demoAccess(
  sqlite3_vfs *pVfs, 
  const char *zPath, 
  int flags, 
  int *pResOut
){
  assert(flags==SQLITE_ACCESS_EXISTS ||
         flags==SQLITE_ACCESS_READ ||
         flags==SQLITE_ACCESS_READWRITE);

  // Because we cannot/don't need to check access permissions,
  // we will set *pResOut to T41SQLite::ACCESS_SUCCESFUL,
  // if a file with the given name exists.
  if (T41SQLite::getInstance().getFilesystem()->exists(zPath))
  {
    *pResOut = T41SQLite::ACCESS_SUCCESFUL;
  }
  else
  {
    *pResOut = T41SQLite::ACCESS_FAILED;
  }
  
  return SQLITE_OK;
}

/*
** Argument zPath points to a nul-terminated string containing a file path.
** If zPath is an absolute path, then it is copied as is into the output 
** buffer. Otherwise, if it is a relative path, then the equivalent full
** path is written to the output buffer.
**
** This function assumes that paths are UNIX style. Specifically, that:
**
**   1. Path components are separated by a '/'. and 
**   2. Full paths begin with a '/' character.
*/
// !!!! It impossible to get the full pathname with exFAT. !!!!
// !!!! Therefore we copy zPath with (set by user) getDBDirFullPath() into zPathOut. !!!!
static int demoFullPathname(
  sqlite3_vfs *pVfs,              /* VFS */
  const char *zPath,              /* Input path (possibly a relative path) */
  int nPathOut,                   /* Size of output buffer in bytes */
  char *zPathOut                  /* Pointer to output buffer */
){
  String fullPath = T41SQLite::getInstance().getDBDirFullPath();
  fullPath.append(zPath);
  sqlite3_snprintf(nPathOut, zPathOut, "%s", fullPath.c_str());
  zPathOut[nPathOut - 1] = '\0';

  Serial.print("VFS_DEBUG_FULL_PATH ");
  Serial.println(zPathOut);

  return SQLITE_OK;
}

/*
** The following four VFS methods:
**
**   xDlOpen
**   xDlError
**   xDlSym
**   xDlClose
**
** are supposed to implement the functionality needed by SQLite to load
** extensions compiled as shared objects. This simple VFS does not support
** this functionality, so the following functions are no-ops.
*/
static void* demoDlOpen(sqlite3_vfs* pVfs, const char* zPath)
{
  return nullptr;
}

static void demoDlError(sqlite3_vfs* pVfs, int nByte, char* zErrMsg)
{
  sqlite3_snprintf(nByte, zErrMsg, "Loadable extensions are not supported");
  zErrMsg[nByte-1] = '\0';
}

static void (*demoDlSym(sqlite3_vfs* pVfs, void* pH, const char* z))(void)
{
  return 0;
}

static void demoDlClose(sqlite3_vfs *pVfs, void *pHandle)
{
  return;
}

/*
** Parameter zByte points to a buffer nByte bytes in size. Populate this
** buffer with pseudo-random data.
*/
static int demoRandomness(sqlite3_vfs *pVfs, int nByte, char *zByte)
{
  return SQLITE_OK;
}

/*
** Sleep for at least nMicro microseconds. Return the (approximate) number 
** of microseconds slept for.
*/
static int demoSleep(sqlite3_vfs *pVfs, int nMicro)
{
  elapsedMicros elapsedMicroseconds;
  delayMicroseconds(nMicro);
  return elapsedMicroseconds;
}

/*
** Set *pTime to the current UTC time expressed as a Julian day. Return
** SQLITE_OK if successful, or an error code otherwise.
**
**   http://en.wikipedia.org/wiki/Julian_day
**
** This implementation is not very good. The current time is rounded to
** an integer number of seconds. Also, assuming time_t is a signed 32-bit 
** value, it will stop working some time in the year 2038 AD (the so-called
** "year 2038" problem that afflicts systems that store time this way). 
*/
static int demoCurrentTime(sqlite3_vfs *pVfs, double *pTime)
{
  time_t t = now();
  *pTime = t/86400.0 + 2440587.5; 
  return SQLITE_OK;
}

/*
** This function returns a pointer to the VFS implemented in this file.
** To make the VFS available to SQLite:
**
**   sqlite3_vfs_register(sqlite3_demovfs(), 0);
*/
sqlite3_vfs *sqlite3_demovfs(void)
{
  static sqlite3_vfs demovfs = {
    1,                            /* iVersion */
    sizeof(DemoFile),             /* szOsFile */
    MAXPATHNAME,                  /* mxPathname */
    0,                            /* pNext */
    FS_VFS_NAME,                  /* zName */
    0,                            /* pAppData */
    demoOpen,                     /* xOpen */
    demoDelete,                   /* xDelete */
    demoAccess,                   /* xAccess */
    demoFullPathname,             /* xFullPathname */
    demoDlOpen,                   /* xDlOpen */
    demoDlError,                  /* xDlError */
    demoDlSym,                    /* xDlSym */
    demoDlClose,                  /* xDlClose */
    demoRandomness,               /* xRandomness */
    demoSleep,                    /* xSleep */
    demoCurrentTime,              /* xCurrentTime */
  };
  
  return &demovfs;
}

void errorLogCallback(void *pArg, int iErrCode, const char *zMsg)
{
  Serial.printf("(%d) %s\n", iErrCode, zMsg);
}

int sqlite3_os_init(void)
{
  int returnCode = SQLITE_OK;
  
  returnCode = sqlite3_config(SQLITE_CONFIG_LOG, errorLogCallback, NULL);
  if (returnCode != SQLITE_OK) { return returnCode; }
  returnCode = sqlite3_vfs_register(sqlite3_demovfs(), 1);
  if (returnCode != SQLITE_OK) { return returnCode; }

  return SQLITE_OK;
}

int sqlite3_os_end(void)
{
  // undo what sqlite3_os_init did (e.g. free resources)

  return SQLITE_OK;
}
