/***************************************************************

                                    sys.c
                                    DOS-C

                            sys utility for DOS-C

                             Copyright (c) 1991
                             Pasquale J. Villani
                             All Rights Reserved

 This file is part of DOS-C.

 DOS-C is free software; you can redistribute it and/or modify it under the
 terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 DOS-C is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 details.

 You should have received a copy of the GNU General Public License along with
 DOS-C; see the file COPYING.  If not, write to the Free Software Foundation,
 675 Mass Ave, Cambridge, MA 02139, USA.

***************************************************************/
/* $Log$
 * Revision 1.8  2001/09/23 20:39:44  bartoldeman
 *    version 2.1a jeremyd 2001/8/19
 *    modified so takes optional 2nd parameter (similar to PC DOS)
 *    where if only 1 argument is given, assume to be destination drive,
 *    but if two arguments given, 1st is source (drive and/or path)
 *    and second is destination drive
 *
 *    FAT32 support.
 *

/* version 2.1a jeremyd 2001/8/19
   modified so takes optional 2nd parameter (similar to PC DOS)
   where if only 1 argument is given, assume to be destination drive,
   but if two arguments given, 1st is source (drive and/or path)
   and second is destination drive
*/

/* Revision 1.7  2001/07/09 22:19:33  bartoldeman
/* LBA/FCB/FAT/SYS/Ctrl-C/ioctl fixes + memory savings
/*
/* Revision 2.1 tomehlert 2001/4/26

    changed the file system detection code.
    

*/

/* Revision 2.0 tomehlert 2001/4/26
   
   no direct access to the disk any more, this is FORMAT's job
   no floppy.asm anymore, no segmentation problems.
   no access to partition tables
   
   instead copy boot sector using int25/int26 = absdiskread()/write
   
   if xxDOS is able to handle the disk, SYS should work
   
   additionally some space savers:
   
   replaced fopen() by open() 
   
   included (slighly modified) PRF.c from kernel
   
   size is no ~7500 byte vs. ~13690 before

*/
/* Revision 1.6  2001/04/29 17:34:41  bartoldeman
/* A new SYS.COM/config.sys single stepping/console output/misc fixes.
/*
/* Revision 1.5  2001/03/25 17:11:54  bartoldeman
/* Fixed sys.com compilation. Updated to 2023. Also: see history.txt.
/*
/* Revision 1.4  2000/08/06 05:50:17  jimtabor
/* Add new files and update cvs with patches and changes
/*
 * Revision 1.3  2000/05/25 20:56:23  jimtabor
 * Fixed project history
 *
 * Revision 1.2  2000/05/15 05:28:09  jimtabor
 * Cleanup CRs
 *
 * Revision 1.1.1.1  2000/05/06 19:34:53  jhall1
 * The FreeDOS Kernel.  A DOS kernel that aims to be 100% compatible with
 * MS-DOS.  Distributed under the GNU GPL.
 *
 * Revision 1.10  2000/03/31 06:59:10  jprice
 * Added discription of program.
 *
 * Revision 1.9  1999/09/20 18:34:40  jprice
 * *** empty log message ***
 *
 * Revision 1.8  1999/09/20 18:27:19  jprice
 * Changed open/creat to fopen to make TC2 happy.
 *
 * Revision 1.7  1999/09/15 05:39:02  jprice
 * Changed boot sector writing code so easier to read.
 *
 * Revision 1.6  1999/09/14 17:30:44  jprice
 * Added debug log creation to sys.com.
 *
 * Revision 1.5  1999/08/25 03:19:51  jprice
 * ror4 patches to allow TC 2.01 compile.
 *
 * Revision 1.4  1999/04/17 19:14:44  jprice
 * Fixed multi-sector code
 *
 * Revision 1.3  1999/04/01 07:24:05  jprice
 * SYS modified for new boot loader
 *
 * Revision 1.2  1999/03/29 16:24:48  jprice
 * Fixed error message
 *
 * Revision 1.1.1.1  1999/03/29 15:43:15  jprice
 * New version without IPL.SYS
 * Revision 1.3  1999/01/21 04:35:21  jprice Fixed comments.
 *   Added indent program
 *
 * Revision 1.2  1999/01/21 04:13:52  jprice Added messages to sys.  Also made
 *   it create a .COM file.
 *
 */

/* 
    TE thinks, that the boot info storage should be done by FORMAT, noone else

    unfortunately, that doesn't work ???
*/
#define STORE_BOOT_INFO

#define DEBUG
/* #define DDEBUG */

#define SYS_VERSION "v2.1a"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <dos.h>
#include <ctype.h>
#ifdef __TURBOC__
       #include <mem.h>
#else
       #include <memory.h>
#endif
#include <string.h>
#include <dir.h>
#include "portab.h"

#include "b_fat12.h"
#include "b_fat16.h"
#ifdef WITHFAT32
#include "b_fat32.h"
#endif

BYTE pgm[] = "sys";

void put_boot(COUNT);
BOOL check_space(COUNT, BYTE *);
BOOL copy(COUNT, BYTE *);
COUNT DiskRead(WORD, WORD, WORD, WORD, WORD, BYTE FAR *);
COUNT DiskWrite(WORD, WORD, WORD, WORD, WORD, BYTE FAR *);


#define SEC_SIZE        512
#define COPY_SIZE       32768u




struct bootsectortype
{
  UBYTE bsJump[3];
  char OemName[8];
  UWORD bsBytesPerSec;
  UBYTE bsSecPerClust;
  UWORD bsResSectors;
  UBYTE bsFATs;
  UWORD bsRootDirEnts;
  UWORD bsSectors;
  UBYTE bsMedia;
  UWORD bsFATsecs;
  UWORD bsSecPerTrack;
  UWORD bsHeads;
  ULONG bsHiddenSecs;
  ULONG bsHugeSectors;
  UBYTE bsDriveNumber;
  UBYTE bsReserved1;
  UBYTE bsBootSignature;
  ULONG bsVolumeID;
  char bsVolumeLabel[11];
  char bsFileSysType[8];
  char unused[2];
  UWORD sysRootDirSecs;         /* of sectors root dir uses */
  ULONG sysFatStart;            /* first FAT sector */
  ULONG sysRootDirStart;        /* first root directory sector */
  ULONG sysDataStart;           /* first data sector */
};

struct bootsectortype32
{
  UBYTE bsJump[3];
  char OemName[8];
  UWORD bsBytesPerSec;
  UBYTE bsSecPerClust;
  UWORD bsResSectors;
  UBYTE bsFATs;
  UWORD bsRootDirEnts;
  UWORD bsSectors;
  UBYTE bsMedia;
  UWORD bsFATsecs;
  UWORD bsSecPerTrack;
  UWORD bsHeads;
  ULONG bsHiddenSecs;
  ULONG bsHugeSectors;
  ULONG bsBigFatSize;
  UBYTE bsFlags;
  UBYTE bsMajorVersion;
  UWORD bsMinorVersion;
  ULONG bsRootCluster;
  UWORD bsFSInfoSector;
  UWORD bsBackupBoot;
  ULONG bsReserved2[3];
  UBYTE bsDriveNumber;
  UBYTE bsReserved3;
  UBYTE bsExtendedSignature;
  ULONG bsSerialNumber;
  char bsVolumeLabel[11];	
  char bsFileSystemID[8];
  ULONG sysFatStart;
  ULONG sysDataStart;
  UWORD sysFatSecMask;
  UWORD sysFatSecShift;
};

COUNT drive;                    /* destination drive */
BYTE srcPath[MAXPATH];          /* source drive and/or path */
BYTE *endOfSrcPath;             /* marks start of filename */
UBYTE newboot[SEC_SIZE], oldboot[SEC_SIZE];


#define SBOFFSET        11
#define SBSIZE          (sizeof(struct bootsectortype) - SBOFFSET)
#define SBSIZE32        (sizeof(struct bootsectortype32) - SBOFFSET)



VOID main(COUNT argc, char **argv)
{
  WORD slen;

  printf("FreeDOS System Installer " SYS_VERSION "\n\n");

  if (argc == 2)
  {
    drive = toupper(*argv[1]) - 'A';
    srcPath[0] = '\0';
    endOfSrcPath = srcPath;
  }
  else if (argc == 3)
  {
    drive = toupper(*argv[2]) - 'A';
    strncpy(srcPath, argv[1], MAXDIR);
    /* make sure srcPath + "file" is a valid path */
    slen = strlen(srcPath);
    if ( (srcPath[slen-1] != ':') && 
         ((srcPath[slen-1] != '\\') || (srcPath[slen-1] != '/')) )
    {
      srcPath[slen] = '\\';
      slen++;
      srcPath[slen] = '\0';
    }
    endOfSrcPath = srcPath + slen;
  }
  else
  {
    printf("Usage: %s [source] drive\n", pgm);
    printf("  source = A:,B:,C:\\KERNEL\\BIN\\,etc., or current directory if not given\n");
    printf("  drive  = A,B,etc.\n");
    exit(1);
  }

  if (drive < 0 || drive >= 26)
  {
    printf( "%s: drive %c must be A:..Z:\n", pgm,*argv[1]);
    exit(1);
  }


  if (!check_space(drive, oldboot))
  {
    printf("%s: Not enough space to transfer system files\n", pgm);
    exit(1);
  }


  printf("\nCopying KERNEL.SYS...\n");
  if (!copy(drive, "kernel.sys"))
  {
    printf("\n%s: cannot copy \"KERNEL.SYS\"\n", pgm);
    exit(1);
  }

  printf("\nCopying COMMAND.COM...\n");
  if (!copy(drive, "command.com"))
  {
    printf("\n%s: cannot copy \"COMMAND.COM\"\n", pgm);
    exit(1);
  }
  
  printf("\nWriting boot sector...\n");
  put_boot(drive);
  
  printf("\nSystem transferred.\n");
  exit(0);
}

#ifdef DEBUG
VOID dump_sector(unsigned char far * sec)
{
    COUNT x, y;
    char c;

    for (x = 0; x < 32; x++)
    {
      printf("%03X  ", x * 16);
      for (y = 0; y < 16; y++)
      {
        printf("%02X ", sec[x * 16 + y]);
      }
      for (y = 0; y < 16; y++)
      {
        c = oldboot[x * 16 + y];
        if (isprint(c))
          printf( "%c", c);
        else
          printf( ".");
      }
      printf( "\n");
     } 

    printf( "\n");
}

#endif


/*
    TC absRead not functional on MSDOS 6.2, large disks
    MSDOS requires int25, CX=ffff for drives > 32MB
*/

int MyAbsReadWrite(int DosDrive, int count, ULONG sector, void *buffer, unsigned intno)
{
    struct {
        unsigned long  sectorNumber;
        unsigned short count;
        void far *address;
        } diskReadPacket;
    int retval;
    union REGS regs;


    diskReadPacket.sectorNumber = sector;
    diskReadPacket.count        = count;
    diskReadPacket.address      = buffer;

    regs.h.al = (BYTE)DosDrive;
    regs.x.bx = (short)&diskReadPacket;
    regs.x.cx = 0xffff;
    
    if (intno != 0x25 && intno != 0x26) return 0xff;
    
    int86(intno,&regs,&regs);
    
#ifdef WITHFAT32
    if (regs.x.cflag)
      {
        regs.x.ax = 0x7305;
        regs.h.dl = DosDrive + 1;
        regs.x.bx = (short)&diskReadPacket;
        regs.x.cx = 0xffff;
        regs.x.si = intno - 0x25;
        int86(0x21, &regs, &regs);
      }
#endif

    return regs.x.cflag ? 0xff : 0;
}    


VOID put_boot(COUNT drive)
{
  COUNT i, z;
  WORD head, track, sector, ret;
  WORD count;
  ULONG temp;
  struct bootsectortype *bs;
#ifdef WITHFAT32
  struct bootsectortype32 *bs32;
#endif
  int fs;
  union REGS regs;
  struct SREGS sregs;
  char drivename[] = "A:\\";
  unsigned char x[0x40];
  

#ifdef DEBUG
    printf("Reading old bootsector from drive %c:\n",drive+'A');
#endif            

  if (MyAbsReadWrite(drive, 1, 0, oldboot,0x25) != 0)
    {
    printf("can't read old boot sector for drive %c:\n", drive +'A');
    exit(1);
    }
  

#ifdef DDEBUG
  printf("Old Boot Sector:\n");
  dump_sector(oldboot);
#endif

  bs = (struct bootsectortype *) & oldboot;
  if ((bs->bsFileSysType[4] == '6') && (bs->bsBootSignature == 0x29))
  {
    fs = 16;
  }
  else
  {
    fs = 12;
  }

/*
    the above code is not save enough for me (TE), so we change the
    FS detection method to GetFreeDiskSpace().
    this should work, as the disk was writeable, so GetFreeDiskSpace should work.
*/
    
    regs.h.ah = 0x36;    /* get drive free space */
    regs.h.dl = drive+1; /* 1 = 'A',... */
    int86(0x21,&regs,&regs);
    
    if (regs.x.ax == 0xffff) 
        {
        printf("can't get free disk space for %c:\n", drive+'A');
        exit(1);
        }

    if (regs.x.dx <= 0xff6)
        {
        if (fs != 12) printf("warning : new detection overrides old detection\a\n");
        fs = 12;
        }
    else {
        
        if (fs != 16) printf("warning : new detection overrides old detection\a\n");
        fs = 16;

                                /* fs = 16/32.
                                   we don't want to crash a FAT32 drive
                                */

        segread(&sregs);
        sregs.es = sregs.ds;
        
        regs.x.ax = 0x7303;    /* get extended drive free space */
        
        drivename[0] = 'A' + drive;
        regs.x.dx = (unsigned)&drivename;
        regs.x.di = (unsigned)&x;
        regs.x.cx = sizeof(x);
        
        int86x(0x21,&regs,&regs,&sregs);
        
        if (regs.x.cflag)       /* error --> no Win98 --> no FAT32 */
            {
            printf("get extended drive space not supported --> no FAT32\n");    
            }
        else {
            if (*(unsigned long *)(x+0x2c) /* total number of clusters */
                    > (unsigned)65526l)
                {
                fs = 32;        
                }
            }
        }


    if (fs == 16)
        {
        memcpy(newboot, b_fat16, SEC_SIZE); /* copy FAT16 boot sector */
        printf("FAT type: FAT16\n");
        }
    else if (fs == 12) 
        {
        memcpy(newboot, b_fat12, SEC_SIZE); /* copy FAT12 boot sector */
        printf("FAT type: FAT12\n");
        }
    else
        {
          printf("FAT type: FAT32\n");
#ifdef WITHFAT32
          memcpy(newboot, b_fat32, SEC_SIZE); /* copy FAT32 boot sector */
#else
          printf("SYS hasn't been compiled with FAT32 support.");
          printf("Consider using -DWITHFAT32 option.\n");
          exit(1);
#endif
        }

  /* Copy disk parameter from old sector to new sector */
#ifdef WITHFAT32
  if (fs == 32)
    memcpy(&newboot[SBOFFSET], &oldboot[SBOFFSET], SBSIZE32);
  else
#endif
    memcpy(&newboot[SBOFFSET], &oldboot[SBOFFSET], SBSIZE);

  bs = (struct bootsectortype *) & newboot;
  
  memcpy(bs->OemName, "FreeDOS ",8);

#ifdef WITHFAT32
  if (fs == 32)
    {
      bs32 = (struct bootsectortype32 *) & newboot;
  
      temp = bs32->bsHiddenSecs + bs32->bsResSectors;
      bs32->sysFatStart = temp;
  
      bs32->sysDataStart = temp + bs32->bsBigFatSize * bs32->bsFATs;
      bs32->sysFatSecMask = bs32->bsBytesPerSec / 4 - 1;
  
      temp = bs32->sysFatSecMask + 1;
      for (bs32->sysFatSecShift = 0; temp != 1; bs32->sysFatSecShift++, temp >>= 1);
    }
#ifdef DEBUG
  if (fs == 32)
    {
      printf( "FAT starts at sector %lx = (%lx + %x)\n", bs32->sysFatStart,
              bs32->bsHiddenSecs, bs32->bsResSectors);
      printf("DATA starts at sector %lx\n", bs32->sysDataStart);
    }
#endif
#else
#ifdef STORE_BOOT_INFO
    /* TE thinks : never, see above */
  /* temporary HACK for the load segment (0x0060): it is in unused */
  /* only needed for older kernels */
  *((UWORD *)(bs->unused)) = *((UWORD *)(((struct bootsectortype *)&b_fat16)->unused));
  /* end of HACK */
                                  /* root directory sectors */

  bs->sysRootDirSecs = bs->bsRootDirEnts / 16;

                                  /* sector FAT starts on */
  temp = bs->bsHiddenSecs + bs->bsResSectors;
  bs->sysFatStart = temp;
  
                                  /* sector root directory starts on */
  temp = temp + bs->bsFATsecs * bs->bsFATs;
  bs->sysRootDirStart = temp;
  
                                  /* sector data starts on */
  temp = temp + bs->sysRootDirSecs;
  bs->sysDataStart = temp;
  
#ifdef DEBUG
  printf("Root dir entries = %u\n", bs->bsRootDirEnts);
  printf("Root dir sectors = %u\n", bs->sysRootDirSecs);

  printf( "FAT starts at sector %lu = (%lu + %u)\n", bs->sysFatStart,
          bs->bsHiddenSecs, bs->bsResSectors);
  printf("Root directory starts at sector %lu = (PREVIOUS + %u * %u)\n",
          bs->sysRootDirStart, bs->bsFATsecs, bs->bsFATs);
  printf("DATA starts at sector %lu = (PREVIOUS + %u)\n", bs->sysDataStart,
          bs->sysRootDirSecs);
#endif
#endif
#endif


#ifdef DDEBUG
  printf("\nNew Boot Sector:\n");
  dump_sector(newboot);
#endif

#ifdef DEBUG
    printf("writing new bootsector to drive %c:\n",drive+'A');
#endif            

  if (MyAbsReadWrite(drive, 1, 0, newboot,0x26) != 0)
    {
    printf("Can't write new boot sector to drive %c:\n", drive +'A');
    exit(1);
    }

}


BOOL check_space(COUNT drive, BYTE * BlkBuffer)
{
    /* this should check, if on destination is enough space
       to hold command.com+ kernel.sys */
       
    UNREFERENCED_PARAMETER(drive);
    UNREFERENCED_PARAMETER(BlkBuffer);
       

  return 1;
}

BYTE copybuffer[COPY_SIZE];

BOOL copy(COUNT drive, BYTE * file)
{
  BYTE dest[64];
  COUNT ifd, ofd;
  unsigned ret;
  int fdin, fdout;
  ULONG copied = 0;
  struct stat fstatbuf;
 
  if (stat(file,&fstatbuf))
    {
    printf( "%s: \"%s\" not found\n", pgm, file);
    return FALSE;
    }


  sprintf(dest, "%c:\\%s", 'A' + drive, file);
  strcpy(endOfSrcPath, file);
  if ((fdin = open(srcPath, O_RDONLY|O_BINARY)) < 0)
  {
    printf( "%s: \"%s\" not found\n", pgm, srcPath);
    return FALSE;
  }


  if ((fdout = open(dest, O_RDWR | O_TRUNC | O_CREAT | O_BINARY,S_IREAD|S_IWRITE)) < 0)
  {
    printf( " %s: can't create\"%s\"\nDOS errnum %d", pgm, dest, errno);
    close(fdin);
    return FALSE;
  }

  while ((ret = read(fdin, copybuffer,COPY_SIZE)) > 0)
    {
        if (write(fdout, copybuffer, ret) != ret)
        {
            printf("Can't write %u bytes to %s\n",dest);
            close(fdout);
            unlink(dest);
            break;
        }
    copied += ret;        
    }        

#ifdef __TURBO__
  {
    struct ftime ftime;
    getftime(fdin, &ftime);
    setftime(fdout, &ftime);
  }
#endif  

  close(fdin);
  close(fdout);
  
#ifdef _MSV_VER
  {
  #include <utime.h>
  struct utimbuf utimb;

  utimb.actime =                                               /* access time */
  utimb.modtime = fstatbuf.st_mtime;    /* modification time */
  utime(dest,&utimb);
  };

#endif


  printf("%lu Bytes transferred", copied);

  return TRUE;
}


