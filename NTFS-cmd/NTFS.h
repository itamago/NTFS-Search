// NTFS Structures

#include "windows.h"
#include <string>
#include <vector>

using std::wstring;
using std::vector;


//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
struct BOOT_BLOCK
{
    UCHAR       Jump[3];
    UCHAR       Format[8];
    USHORT      BytesPerSector;
    UCHAR       SectorsPerCluster;
    USHORT      BootSectors;
    UCHAR       Mbz1;
    USHORT      Mbz2;
    USHORT      Reserved1;
    UCHAR       MediaType;
    USHORT      Mbz3;
    USHORT      SectorsPerTrack;
    USHORT      NumberOfHeads;
    ULONG       PartitionOffset;
    ULONG       Rserved2[2];
    ULONGLONG   TotalSectors;
    ULONGLONG   MftStartLcn;
    ULONGLONG   Mft2StartLcn;
    ULONG       ClustersPerFileRecord;
    ULONG       ClustersPerIndexBlock;
    ULONGLONG   VolumeSerialNumber;
    UCHAR       Code[0x1AE];
    USHORT      BootSignature;
};
#pragma pack(pop)

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// NTFS_RECORD_HEADER
//    type - 'FILE' 'INDX' 'BAAD' 'HOLE' *CHKD'
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct NTFS_RECORD_HEADER
{
    ULONG       Type;
    USHORT      UsaOffset;
    USHORT      UsaCount;
    USN         Usn;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// FILE_RECORD_HEADER
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct FILE_RECORD_HEADER
{
    NTFS_RECORD_HEADER  Ntfs;
    USHORT              SequenceNumber;
    USHORT              LinkCount;
    USHORT              AttributesOffset;
    USHORT              Flags; // 0x0001 InUse; 0x0002 Directory
    ULONG               BytesInUse;
    ULONG               BytesAllocated;
    ULARGE_INTEGER      BaseFileRecord;
    USHORT              NextAttributeNumber;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ATTRIBUTE_TYPE enumeration
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
enum class AttributeType : int
{
    ZeroValue           = 0,
    StandardInformation = 0x10,
    AttributeList       = 0x20,
    FileName            = 0x30,
    ObjectId            = 0x40,
    SecurityDescripter  = 0x50,
    VolumeName          = 0x60,
    VolumeInformation   = 0x70,
    Data                = 0x80,
    IndexRoot           = 0x90,
    IndexAllocation     = 0xA0,
    Bitmap              = 0xB0,
    ReparsePoint        = 0xC0,
    EAInformation       = 0xD0,
    EA                  = 0xE0,
    PropertySet         = 0xF0,
    LoggedUtilityStream = 0x100
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ATTRIBUTE Structure
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct ATTRIBUTE
{
    AttributeType   AttribType;
    ULONG           Length;
    BOOLEAN         Nonresident;
    UCHAR           NameLength; 
    USHORT          NameOffset;     // Starts form the Attribute Offset
    USHORT          Flags;          // 0x001 = Compressed
    USHORT          AttributeNumber;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ATTRIBUTE resident
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct RESIDENT_ATTRIBUTE
{
    ATTRIBUTE       Attribute;
    ULONG           ValueLength;
    USHORT          ValueOffset;    // Starts from the Attribute
    USHORT          Flags;          // 0x0001 Indexed
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ATTRIBUTE nonresident
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct NONRESIDENT_ATTRIBUTE
{
    ATTRIBUTE       Attribute;
    ULONGLONG       LowVcn;
    ULONGLONG       HighVcn;
    USHORT          RunArrayOffset;
    UCHAR           CompressionUnit;
    UCHAR           AligmentOrReserved[5];
    ULONGLONG       AllocatedSize;
    ULONGLONG       DataSize;
    ULONGLONG       InitializedSize;
    ULONGLONG       CompressedSize;     //Only when compressed
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//    VolumeName - just a Unicode String
//    Data = just data
//    SecurityDescriptor - rarely found
//    Bitmap - array of bits, which indicate the use of entries
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// STANDARD_INFORMATION
//    FILE_ATTRIBUTES_* like in windows.h and is always resident
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct STANDARD_INFORMATION
{
    FILETIME        CreationTime;
    FILETIME        ChangeTime;
    FILETIME        LastWriteTime;
    FILETIME        LastAccessTime;
    ULONG           FileAttributes;
    ULONG           AligmentOrReservedOrUnknown[3];
    ULONG           QuotaId;        //NTFS 3.0 or higher
    ULONG           SecurityID;     //NTFS 3.0 or higher
    ULONGLONG       QuotaCharge;    //NTFS 3.0 or higher
    USN             Usn;            //NTFS 3.0 or higher
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ATTRIBUTE_LIST is always nonresident and consists of an array of ATTRIBUTE_LIST
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct ATTRIBUTE_LIST
{
    AttributeType  Attribute;
    USHORT          Length;
    UCHAR           NameLength;
    USHORT          NameOffset;     // starts at structure begin
    ULONGLONG       LowVcn;
    ULONGLONG       FileReferenceNumber;
    USHORT          AttributeNumber;
    USHORT          AligmentOrReserved[3];
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// FILENAME_ATTRIBUTE is always resident
//    ULONGLONG informations only updated, if name changes
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct FILENAME_ATTRIBUTE
{
    ULONGLONG       DirectoryFileReferenceNumber;   //points to a MFT Index of a directory
    FILETIME        CreationTime;                   //saved on creation, changed when filename changes
    FILETIME        ChangeTime;
    FILETIME        LastWriteTime;
    FILETIME        LastAccessTime;
    ULONGLONG       AllocatedSize;
    ULONGLONG       DataSize; 
    ULONG           FileAttributes;
    ULONG           AligmentOrReserved;
    UCHAR           NameLength;
    UCHAR           NameType;       // 0x01 Long, 0x02 Short, 0x00 Posix?
    WCHAR           Name[1];
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Custom structures

#define NTFSDISK  1

// not supported
#define FAT32DISK 2
#define FATDISK   4
#define EXT2      8

#define UNKNOWN 0xff99ff99

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct SEARCHFILEINFO
{
    USHORT          Flags           = 0;  // 0x0001 InUse; 0x0002 Directory
    ULARGE_INTEGER  ParentId        = {};
    ULONGLONG       DataSize        = 0;
    ULONGLONG       AllocatedSize   = 0;
    char            data[64]        = {};
    wstring         FileName;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct DISKHANDLE
{
    HANDLE  fileHandle  = NULL;
    bool    isNTFS      = false;
    DWORD   IsLong      = 0;
    DWORD   filesSize   = 0;
    DWORD   realFiles   = 0;
    WCHAR   DosDevice   = 0;

    struct
    {
        BOOT_BLOCK      bootSector         = {};
        DWORD           BytesPerFileRecord = 0;
        DWORD           BytesPerCluster    = 0;
        bool            complete           = false;
        DWORD           sizeMFT            = 0;
        DWORD           entryCount         = 0;
        ULARGE_INTEGER  MFTLocation        = {};
        UCHAR*          MFT                = NULL;
        UCHAR*          Bitmap             = NULL;
    } NTFS;

    vector<SEARCHFILEINFO> fFiles;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define LONGINFO        1
#define SHORTINFO       2
#define SEARCHINFO      3
#define EXTRALONGINFO   4

// Functions
DISKHANDLE* OpenDisk(LPCTSTR disk);
DISKHANDLE* OpenDisk(WCHAR DosDevice);

bool        CloseDisk(DISKHANDLE* disk);
ULONGLONG   LoadMFT(DISKHANDLE* disk, bool complete);

DWORD       ParseMFT(DISKHANDLE* disk, UINT option, DWORD* progressValue);
bool        ReparseDisk(DISKHANDLE* disk, UINT option, DWORD* progressValue);

LPWSTR      GetPath(DISKHANDLE* disk, int id);

// Internal
bool        FixFileRecord(FILE_RECORD_HEADER* file);

ATTRIBUTE*  FindAttribute(FILE_RECORD_HEADER* file, AttributeType type);

bool        FetchSearchInfo(DISKHANDLE* disk, FILE_RECORD_HEADER* file, SEARCHFILEINFO* data);

ULONG       RunLength(PUCHAR run);
LONGLONG    RunLCN(PUCHAR run);
ULONGLONG   RunCount(PUCHAR run);
bool        FindRun(NONRESIDENT_ATTRIBUTE* attr, ULONGLONG vcn, PULONGLONG lcn, PULONGLONG count);
DWORD       ReadMFTParse(DISKHANDLE* disk, NONRESIDENT_ATTRIBUTE* attr, ULONGLONG vcn, ULONG count, PVOID buffer, DWORD* progressValue);
DWORD       ReadMFTLCN(DISKHANDLE* disk, ULONGLONG lcn, ULONG count, PVOID buffer, DWORD* progressValue);
DWORD       ProcessBuffer(DISKHANDLE* disk, PUCHAR buffer, DWORD size);

#if 0
LPWSTR GetCompletePath(DISKHANDLE* disk, int id);
#endif