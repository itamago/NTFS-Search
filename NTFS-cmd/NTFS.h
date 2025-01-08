// NTFS Structures

#include "windows.h"
#include <string>
#include <vector>

using std::wstring;
using std::vector;

/* BOOT_BLOCK

*/
#pragma pack(push,1)

typedef struct
{
	UCHAR Jump[3];
	UCHAR Format[8];
	USHORT BytesPerSector;
	UCHAR SectorsPerCluster;
	USHORT BootSectors;
	UCHAR Mbz1;
	USHORT Mbz2;
	USHORT Reserved1;
	UCHAR MediaType;
	USHORT Mbz3;
	USHORT SectorsPerTrack;
	USHORT NumberOfHeads;
	ULONG PartitionOffset;
	ULONG Rserved2[2];
	ULONGLONG TotalSectors;
	ULONGLONG MftStartLcn;
	ULONGLONG Mft2StartLcn;
	ULONG ClustersPerFileRecord;
	ULONG ClustersPerIndexBlock;
	ULONGLONG VolumeSerialNumber;
	UCHAR Code[0x1AE];
	USHORT BootSignature;
}BOOT_BLOCK, *PBOOT_BLOCK;

#pragma pack(pop)


/* NTFS_RECORD_HEADER
	type - 'FILE' 'INDX' 'BAAD' 'HOLE' *CHKD'

*/
typedef struct
{
	ULONG Type;
	USHORT UsaOffset;
	USHORT UsaCount;
	USN Usn;
}NTFS_RECORD_HEADER, *PNTFS_RECORD_HEADER;

/* FILE_RECORD_HEADER

*/
typedef struct
{
	NTFS_RECORD_HEADER Ntfs;
	USHORT SequenceNumber;
	USHORT LinkCount;
	USHORT AttributesOffset;
	USHORT Flags; // 0x0001 InUse; 0x0002 Directory
	ULONG BytesInUse;
	ULONG BytesAllocated;
	ULARGE_INTEGER BaseFileRecord;
	USHORT NextAttributeNumber;
} FILE_RECORD_HEADER, *PFILE_RECORD_HEADER;

/* ATTRIBUTE_TYPE enumeration

*/

typedef enum 
{
	ZeroValue = 0,
	StandardInformation = 0x10,
	AttributeList = 0x20,
	FileName = 0x30,
	ObjectId = 0x40,
	SecurityDescripter = 0x50,
	VolumeName = 0x60,
	VolumeInformation = 0x70,
	Data = 0x80,
	IndexRoot = 0x90,
	IndexAllocation = 0xA0,
	Bitmap = 0xB0,
	ReparsePoint = 0xC0,
	EAInformation = 0xD0,
	EA = 0xE0,
	PropertySet = 0xF0,
	LoggedUtilityStream = 0x100
} ATTRIBUTE_TYPE, *PATTRIBUTE_TYPE;

/* ATTRIBUTE Structure

*/
typedef struct
{
	ATTRIBUTE_TYPE AttributeType;
	ULONG Length;
	BOOLEAN Nonresident;
	UCHAR NameLength; 
	USHORT NameOffset; // Starts form the Attribute Offset
	USHORT Flags; // 0x001 = Compressed
	USHORT AttributeNumber;
} ATTRIBUTE, *PATTRIBUTE;

/* ATTRIBUTE resident

*/
typedef struct 
{
	ATTRIBUTE Attribute;
	ULONG ValueLength;
	USHORT ValueOffset; //Starts from the Attribute
	USHORT Flags; //0x0001 Indexed
} RESIDENT_ATTRIBUTE, *PRESIDENT_ATTRIBUTE;

/* ATTRIBUTE nonresident

*/
typedef struct {
	ATTRIBUTE Attribute;
	ULONGLONG LowVcn;
	ULONGLONG HighVcn;
	USHORT RunArrayOffset;
	UCHAR CompressionUnit;
	UCHAR AligmentOrReserved[5];
	ULONGLONG AllocatedSize;
	ULONGLONG DataSize;
	ULONGLONG InitializedSize;
	ULONGLONG CompressedSize; //Only when compressed
} NONRESIDENT_ATTRIBUTE, *PNONRESIDENT_ATTRIBUTE;

/* 
	VolumeName - just a Unicode String
	Data = just data
	SecurityDescriptor - rarely found
	Bitmap - array of bits, which indicate the use of entries
*/

/* STANDARD_INFORMATION
	FILE_ATTRIBUTES_* like in windows.h
	and is always resident
*/
typedef struct {
	FILETIME CreationTime;
	FILETIME ChangeTime;
	FILETIME LastWriteTime;
	FILETIME LastAccessTime;
	ULONG FileAttributes;
	ULONG AligmentOrReservedOrUnknown[3];
	ULONG QuotaId; //NTFS 3.0 or higher
	ULONG SecurityID; //NTFS 3.0 or higher
	ULONGLONG QuotaCharge; //NTFS 3.0 or higher
	USN Usn; //NTFS 3.0 or higher
} STANDARD_INFORMATION, *PSTANDARD_INFORMATION;

/* ATTRIBUTE_LIST 
	is always nonresident and consists of an array of ATTRIBUTE_LIST
*/
typedef struct {
	ATTRIBUTE_TYPE Attribute;
	USHORT Length;
	UCHAR NameLength;
	USHORT NameOffset; // starts at structure begin
	ULONGLONG LowVcn;
	ULONGLONG FileReferenceNumber;
	USHORT AttributeNumber;
	USHORT AligmentOrReserved[3];
}ATTRIBUTE_LIST, *PATTRIBUTE_LIST;

/* FILENAME_ATTRIBUTE
	is always resident
	ULONGLONG informations only updated, if name changes
*/
typedef struct {
	ULONGLONG DirectoryFileReferenceNumber; //points to a MFT Index of a directory
	FILETIME CreationTime; //saved on creation, changed when filename changes
	FILETIME ChangeTime;
	FILETIME LastWriteTime;
	FILETIME LastAccessTime;
	ULONGLONG AllocatedSize;
	ULONGLONG DataSize; 
	ULONG FileAttributes; // ditto
	ULONG AligmentOrReserved;
	UCHAR NameLength;
	UCHAR NameType; // 0x01 Long 0x02 Short 0x00 Posix?
	WCHAR Name[1];
}FILENAME_ATTRIBUTE, *PFILENAME_ATTRIBUTE;

#define POSIX_NAME 0
#define WIN32_NAME 1
#define DOS_NAME 2
#define WIN32DOS_NAME 3


/* MYSTRUCTS


*/
#define NTFSDISK  1

// not supported
#define FAT32DISK 2
#define FATDISK   4
#define EXT2	  8

#define UNKNOWN 0xff99ff99


struct SEARCHFILEINFO
{
    USHORT Flags			= 0;  // 0x0001 InUse; 0x0002 Directory
	ULARGE_INTEGER ParentId	= {};

    ULONGLONG DataSize		= 0;
    ULONGLONG AllocatedSize	= 0;

	char data[64]			= {};
    wstring FileName;
};


struct DISKHANDLE
{
	HANDLE fileHandle;
	DWORD type;
	DWORD IsLong;
	DWORD filesSize;
	DWORD realFiles;
	WCHAR DosDevice;
	union
	{
		struct
		{
			BOOT_BLOCK bootSector;
			DWORD BytesPerFileRecord;
			DWORD BytesPerCluster;
			BOOL complete;
			DWORD sizeMFT;
			DWORD entryCount;
			ULARGE_INTEGER MFTLocation;
			UCHAR *MFT;
			UCHAR *Bitmap;
		} NTFS;
		struct
		{
			DWORD FAT;
		} FAT;
	};
    vector<SEARCHFILEINFO> fFiles;
};

/* MY FUNCTIONS

*/

#define LONGINFO		1
#define SHORTINFO		2
#define SEARCHINFO		3
#define EXTRALONGINFO	4

typedef DWORD(__cdecl *FETCHPROC)(DISKHANDLE*, PFILE_RECORD_HEADER, PUCHAR);

// Functions
DISKHANDLE* OpenDisk(LPCTSTR disk);
DISKHANDLE* OpenDisk(WCHAR DosDevice);

BOOL		CloseDisk(DISKHANDLE* disk);
ULONGLONG	LoadMFT(DISKHANDLE* disk, BOOL complete);

DWORD		ParseMFT(DISKHANDLE* disk, UINT option, DWORD* progressValue);
BOOL		ReparseDisk(DISKHANDLE* disk, UINT option, DWORD* progressValue);

LPWSTR		GetPath(DISKHANDLE* disk, int id);

// Internal
BOOL		FixFileRecord(PFILE_RECORD_HEADER file);

PATTRIBUTE	FindAttribute(PFILE_RECORD_HEADER file, ATTRIBUTE_TYPE type);

BOOL		FetchSearchInfo(DISKHANDLE* disk, PFILE_RECORD_HEADER file, SEARCHFILEINFO* data);

ULONG		RunLength(PUCHAR run);
LONGLONG	RunLCN(PUCHAR run);
ULONGLONG	RunCount(PUCHAR run);
BOOL		FindRun(PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, PULONGLONG lcn, PULONGLONG count);
DWORD		ReadMFTParse(DISKHANDLE* disk, PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, ULONG count, PVOID buffer, FETCHPROC fetch, DWORD* progressValue);
DWORD		ReadMFTLCN(DISKHANDLE* disk, ULONGLONG lcn, ULONG count, PVOID buffer, FETCHPROC fetch, DWORD* progressValue);
DWORD		ProcessBuffer(DISKHANDLE* disk, PUCHAR buffer, DWORD size, FETCHPROC fetch);

#if 0
LPWSTR GetCompletePath(DISKHANDLE* disk, int id);
#endif