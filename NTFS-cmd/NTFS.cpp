
#include "windows.h"
#include "commctrl.h"
#include "FixList.h"

enum { CLUSTERSPERREAD = 1024 };

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ULONGLONG AttributeLength(ATTRIBUTE* attr) 
{ 
    return attr->Nonresident
        ? ((NONRESIDENT_ATTRIBUTE*)attr)->DataSize
        : ((RESIDENT_ATTRIBUTE*)attr)->ValueLength;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ULONGLONG AttributeLengthAllocated(ATTRIBUTE* attr)
{
    return attr->Nonresident ? ((NONRESIDENT_ATTRIBUTE*)attr)->AllocatedSize : ((RESIDENT_ATTRIBUTE*)attr)->ValueLength;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DISKHANDLE* OpenDisk(WCHAR DosDevice)
{
    WCHAR path[8];
    path[0] = L'\\';
    path[1] = L'\\';
    path[2] = L'.';
    path[3] = L'\\';
    path[4] = DosDevice;
    path[5] = L':';
    path[6] = L'\0';
    
    DISKHANDLE* disk = OpenDisk(path);
    if (disk != nullptr)
    {
        disk->DosDevice = DosDevice;
        return disk;
    }
    
    return nullptr;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DISKHANDLE* OpenDisk(LPCTSTR disk)
{
    DISKHANDLE* tmpDisk = new DISKHANDLE;

    DWORD read = 0;
    tmpDisk->fileHandle = CreateFile(disk, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (tmpDisk->fileHandle != INVALID_HANDLE_VALUE)
    {
        ReadFile(tmpDisk->fileHandle, &tmpDisk->NTFS.bootSector, sizeof(BOOT_BLOCK), &read, nullptr);
        if (read == sizeof(BOOT_BLOCK))
        {
            if (strncmp("NTFS", reinterpret_cast<const char*>(&tmpDisk->NTFS.bootSector.Format), 4) == 0)
            {
                tmpDisk->isNTFS = true;
                tmpDisk->NTFS.BytesPerCluster = tmpDisk->NTFS.bootSector.BytesPerSector * tmpDisk->NTFS.bootSector.SectorsPerCluster;
                tmpDisk->NTFS.BytesPerFileRecord = tmpDisk->NTFS.bootSector.ClustersPerFileRecord < 0x80 ? tmpDisk->NTFS.bootSector.ClustersPerFileRecord * tmpDisk->NTFS.BytesPerCluster : 1 << (0x100 - tmpDisk->NTFS.bootSector.ClustersPerFileRecord);

                tmpDisk->NTFS.complete = false;
                tmpDisk->NTFS.MFTLocation.QuadPart = tmpDisk->NTFS.bootSector.MftStartLcn * tmpDisk->NTFS.BytesPerCluster;
                tmpDisk->NTFS.MFT = nullptr;
                tmpDisk->IsLong = 0;
                tmpDisk->NTFS.sizeMFT = 0;
            }
            else
            {
                tmpDisk->isNTFS = false;
                tmpDisk->fFiles.clear();
            }
        }
        return tmpDisk;
    }

    delete tmpDisk;
    return nullptr;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool CloseDisk(DISKHANDLE* disk)
{
    if (disk != nullptr)
    {
        if (disk->fileHandle > INVALID_HANDLE_VALUE) 
        {
            CloseHandle(disk->fileHandle);
        }

        if (disk->isNTFS)
        {
            {
                delete disk->NTFS.MFT;
            }
            disk->NTFS.MFT = nullptr;
            {
                delete disk->NTFS.Bitmap;
            }
            disk->NTFS.Bitmap = nullptr;
        }

        delete disk;
        return true;
    }
    return false;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ULONGLONG LoadMFT(DISKHANDLE* disk, bool complete)
{
    DWORD read;
    ULARGE_INTEGER offset;
    UCHAR *buf;

    if (disk == nullptr) {
        return 0;
    }

    if (disk->isNTFS)
    {
        offset = disk->NTFS.MFTLocation;

        SetFilePointer(disk->fileHandle, offset.LowPart, reinterpret_cast<PLONG>(&offset.HighPart), FILE_BEGIN);
        buf = new UCHAR[disk->NTFS.BytesPerCluster];
        ReadFile(disk->fileHandle, buf, disk->NTFS.BytesPerCluster, &read, nullptr);

        FILE_RECORD_HEADER* file = (FILE_RECORD_HEADER*)buf;

        FixFileRecord(file);

        NONRESIDENT_ATTRIBUTE* nattr = nullptr;
        if (file->Ntfs.Type == 'ELIF')
        {
            NONRESIDENT_ATTRIBUTE* nattr2 = nullptr;
            ATTRIBUTE* attr = (ATTRIBUTE*)(reinterpret_cast<PUCHAR>(file) + file->AttributesOffset);
            int stop = min(8, file->NextAttributeNumber);

            for (int i = 0; i < stop; i++)
            {
                if (int(attr->AttribType) < 0 || int(attr->AttribType) > 0x100) 
                    break;

                switch (attr->AttribType)
                {
                    case AttributeType::AttributeList:
                        // now it gets tricky
                        // we have to rebuild the data attribute

                        // wake down the list to find all runarrays
                        // use ReadAttribute to get the list
                        // I think, the right order is important

                        // find out how to walk down the list !!!!

                        // the only solution for now
                        return 3;
                        break;
                    case AttributeType::Data:
                        nattr = ((NONRESIDENT_ATTRIBUTE*)attr);
                        break;
                    case AttributeType::Bitmap:
                        nattr2 = ((NONRESIDENT_ATTRIBUTE*)attr);
                    default:
                        break;
                };


                if (attr->Length > 0 && attr->Length < file->BytesInUse) 
                {
                    attr = (ATTRIBUTE*)(PUCHAR(attr) + attr->Length);
                }
                else if (attr->Nonresident == TRUE) 
                {
                    attr = (ATTRIBUTE*)(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));
                }
            }
            if (nattr == nullptr) {
                return 0;
            }
            if (nattr2 == nullptr) {
                return 0;
            }
        }
        disk->NTFS.sizeMFT = static_cast<DWORD>(nattr->DataSize);
        disk->NTFS.MFT = buf;

        disk->NTFS.entryCount = disk->NTFS.sizeMFT / disk->NTFS.BytesPerFileRecord;
        return nattr->DataSize;
    }
    return 0;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ATTRIBUTE* FindAttribute(FILE_RECORD_HEADER* file, AttributeType type)
{
    ATTRIBUTE* attr = (ATTRIBUTE*)(reinterpret_cast<PUCHAR>(file) + file->AttributesOffset);

    for (int i = 1; i < file->NextAttributeNumber; i++)
    {
        if (attr->AttribType == type)
            return attr;

        if (int(attr->AttribType) < 1 || int(attr->AttribType) > 0x100)
            break;

        if (attr->Length > 0 && attr->Length < file->BytesInUse)
        {
            attr = (ATTRIBUTE*)(PUCHAR(attr) + attr->Length);
        }
        else if (attr->Nonresident == TRUE) 
        {
            attr = (ATTRIBUTE*)(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DWORD ParseMFT(DISKHANDLE* disk, UINT option, DWORD* progressValue)
{
    if (disk == nullptr)
        return 0;

    if (disk->isNTFS)
    {
        CreateFixList();

        FILE_RECORD_HEADER* fh = (FILE_RECORD_HEADER*)disk->NTFS.MFT;
        FixFileRecord(fh);

        disk->IsLong = 1;//sizeof(SEARCHFILEINFO);

        NONRESIDENT_ATTRIBUTE* nattr = ((NONRESIDENT_ATTRIBUTE*)FindAttribute(fh, AttributeType::Data));
        if (nattr != nullptr)
        {
            auto buffer = new UCHAR[CLUSTERSPERREAD*disk->NTFS.BytesPerCluster];
            ReadMFTParse(disk, nattr, 0, ULONG(nattr->HighVcn) + 1, buffer, progressValue);
            delete[] buffer;
        }

        ProcessFixList(disk);
    }

    return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DWORD ReadMFTParse(DISKHANDLE* disk, NONRESIDENT_ATTRIBUTE* attr, ULONGLONG vcn, ULONG count, PVOID buffer, DWORD* progressValue)
{
    ULONGLONG lcn, runcount;
    ULONG readcount, left;
    DWORD ret = 0;
    auto bytes = PUCHAR(buffer);

    int x = (disk->NTFS.entryCount + 16);
    disk->fFiles.resize(x);

    for (left = count; left > 0; left -= readcount)
    {
        FindRun(attr, vcn, &lcn, &runcount);
        readcount = ULONG(min(runcount, left));
        ULONG n = readcount * disk->NTFS.BytesPerCluster;
        if (lcn == 0)
        {
            // spares file?
            memset(bytes, 0, n);
        }
        else
        {
            ret += ReadMFTLCN(disk, lcn, readcount, buffer, progressValue);
        }
        vcn += readcount;
        bytes += n;
    }
    return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ULONG RunLength(const PUCHAR run)
{
    // i guess it must be this way
    return (*run & 0xf) + ((*run >> 4) & 0xf) + 1;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LONGLONG RunLCN(const PUCHAR run)
{
    UCHAR n1 = *run & 0xf;
    UCHAR n2 = (*run >> 4) & 0xf;
    LONGLONG lcn = n2 == 0 ? 0 : CHAR(run[n1 + n2]);

    for (LONG i = n1 + n2 - 1; i > n1; i--) {
        lcn = (lcn << 8) + run[i];
    }
    return lcn;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ULONGLONG RunCount(const PUCHAR run)
{
    // count the runs we have to process
    UCHAR k = *run & 0xf;
    ULONGLONG count = 0;

    for (ULONG i = k; i > 0; i--) {
        count = (count << 8) + run[i];
    }

    return count;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool FindRun(NONRESIDENT_ATTRIBUTE* attr, ULONGLONG vcn, PULONGLONG lcn, PULONGLONG count)
{
    if (vcn < attr->LowVcn || vcn > attr->HighVcn)
        return false;
    *lcn = 0;

    ULONGLONG base = attr->LowVcn;

    for (auto run = PUCHAR(PUCHAR(attr) + attr->RunArrayOffset); *run != 0; run += RunLength(run))
    {
        *lcn += RunLCN(run);
        *count = RunCount(run);
        if (base <= vcn && vcn < base + *count)
        {
            *lcn = RunLCN(run) == 0 ? 0 : *lcn + vcn - base;
            *count -= ULONG(vcn - base);
            return true;
        }

        base += *count;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DWORD ReadMFTLCN(DISKHANDLE* disk, ULONGLONG lcn, ULONG count, PVOID buffer, DWORD* progressValue)
{
    LARGE_INTEGER offset;
    DWORD read = 0;
    //DWORD ret=0;
    DWORD cnt = 0, c = 0, pos = 0;

    offset.QuadPart = lcn*disk->NTFS.BytesPerCluster;
    SetFilePointer(disk->fileHandle, offset.LowPart, &offset.HighPart, FILE_BEGIN);

    cnt = count / CLUSTERSPERREAD;

    for (int i = 1; i <= cnt; i++)
    {
        ReadFile(disk->fileHandle, buffer, CLUSTERSPERREAD*disk->NTFS.BytesPerCluster, &read, nullptr);
        c += CLUSTERSPERREAD;
        pos += read;

        ProcessBuffer(disk, static_cast<PUCHAR>(buffer), read);
        *progressValue = disk->filesSize;
    }

    ReadFile(disk->fileHandle, buffer, (count - c)*disk->NTFS.BytesPerCluster, &read, nullptr);
    ProcessBuffer(disk, static_cast<PUCHAR>(buffer), read);
    *progressValue = disk->filesSize;

    pos += read;
    return pos;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DWORD ProcessBuffer(DISKHANDLE* disk, PUCHAR buffer, DWORD size)
{
    auto end = PUCHAR(buffer) + size;
    SEARCHFILEINFO* fileInfo = &disk->fFiles[disk->filesSize];

    while (buffer < end)
    {
        FILE_RECORD_HEADER* fh = (FILE_RECORD_HEADER*)buffer;
        FixFileRecord(fh);
        if (FetchSearchInfo(disk, fh, fileInfo)) 
        {
            disk->realFiles++;
        }
        buffer += disk->NTFS.BytesPerFileRecord;
        ++fileInfo;
        disk->filesSize++;
    }
    return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define POSIX_NAME      0
#define WIN32_NAME      1
#define DOS_NAME        2
#define WIN32DOS_NAME   3

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool FetchSearchInfo(DISKHANDLE* disk, FILE_RECORD_HEADER* file, SEARCHFILEINFO* fileInfo)
{
    FILENAME_ATTRIBUTE* fn = nullptr;
    ATTRIBUTE* attr = (ATTRIBUTE*)(reinterpret_cast<PUCHAR>(file) + file->AttributesOffset);
    int stop = min(8, file->NextAttributeNumber);

    bool fileNameFound = false;
    bool fileSizeFound = false;
    bool dataFound = false;

    if (file->Ntfs.Type == 'ELIF')
    {
        fileInfo->Flags = file->Flags;

        for (int i = 0; i < stop; i++)
        {
            if (int(attr->AttribType) < 0 || int(attr->AttribType) >0x100)
                break;

            switch (attr->AttribType)
            {
                case AttributeType::FileName:
                    fn = (FILENAME_ATTRIBUTE*)(PUCHAR(attr) + ((RESIDENT_ATTRIBUTE*)attr)->ValueOffset);
                    if (((fn->NameType & WIN32_NAME) != 0) || fn->NameType == 0)
                    {
                        fn->Name[fn->NameLength] = L'\0';
                        fileInfo->FileName.clear();
                        fileInfo->FileName.append(fn->Name, fn->NameLength);
                        fileInfo->ParentId.QuadPart = fn->DirectoryFileReferenceNumber;
                        fileInfo->ParentId.HighPart &= 0x0000ffff;

                        if (fn->DataSize || fn->AllocatedSize)
                        {
                            fileInfo->DataSize = fn->DataSize;
                            fileInfo->AllocatedSize = fn->AllocatedSize;
                        }

                        if (file->BaseFileRecord.LowPart != 0)// && file->BaseFileRecord.HighPart !=0x10000)
                        {
                            AddToFixList(file->BaseFileRecord.LowPart, disk->filesSize);
                        }

                        if (dataFound && fileSizeFound)
                            return true;
                        
                        fileNameFound = true;
                    }
                    break;

                case AttributeType::Data:
                    if (!attr->Nonresident && ((RESIDENT_ATTRIBUTE*)attr)->ValueLength > 0)
                    {
                        memcpy(fileInfo->data,
                            PUCHAR(attr) + ((RESIDENT_ATTRIBUTE*)attr)->ValueOffset,
                            min(sizeof(fileInfo->data), ((RESIDENT_ATTRIBUTE*)attr)->ValueLength));

                        if (fileNameFound && fileSizeFound)
                            return true;
                        
                        dataFound = true;
                    }

                case AttributeType::ZeroValue: // falls through
                    if (AttributeLength(attr) > 0 || AttributeLengthAllocated(attr) > 0)
                    {
                        fileInfo->DataSize = max(fileInfo->DataSize, AttributeLength(attr));
                        fileInfo->AllocatedSize = max(fileInfo->AllocatedSize, AttributeLengthAllocated(attr));
                        
                        if (fileNameFound && dataFound)
                            return true;
                        
                        fileSizeFound = true;
                    }
                break;
                default:
                    break;
            };


            if (attr->Length > 0 && attr->Length < file->BytesInUse)
            {
                attr = (ATTRIBUTE*)(PUCHAR(attr) + attr->Length);
            }
            else if (attr->Nonresident == TRUE) 
            {
                attr = (ATTRIBUTE*)(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));
            }
        }
    }
    return fileNameFound;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool FixFileRecord(FILE_RECORD_HEADER* file)
{
    //int sec = 2048;
    auto usa = PUSHORT(PUCHAR(file) + file->Ntfs.UsaOffset);
    auto sector = PUSHORT(file);

    if (file->Ntfs.UsaCount > 4)
        return false;

    for (ULONG i = 1; i < file->Ntfs.UsaCount; i++)
    {
        sector[255] = usa[i];
        sector += 256;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool ReparseDisk(DISKHANDLE* disk, UINT option, DWORD* progressValue)
{
    if (disk != nullptr)
    {
        if (disk->isNTFS)
        {
            {
                delete disk->NTFS.MFT;
            }
            disk->NTFS.MFT = nullptr;
            {
                delete disk->NTFS.Bitmap;
            }
            disk->NTFS.Bitmap = nullptr;
        }
        disk->fFiles.clear();

        disk->filesSize = 0;
        disk->realFiles = 0;

        if (LoadMFT(disk, false) != 0)
            ParseMFT(disk, option, progressValue);

        return true;
    }

    return false;
};

#if 1
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LPWSTR GetPath(DISKHANDLE* disk, int id)
{
    int a = id;
    DWORD pt;
    DWORD PathStack[64];
    int PathStackPos = 0;
    static WCHAR glPath[0xffff];
    int CurrentPos = 0;

    PathStackPos = 0;
    for (int i = 0; i < 64; i++)
    {
        PathStack[PathStackPos++] = a;
        pt = a*disk->IsLong;
        a = disk->fFiles[pt].ParentId.LowPart;

        if (a == 0 || a == 5) {
            break;
        }
    }
    if (disk->DosDevice != NULL)
    {
        glPath[0] = disk->DosDevice;
        glPath[1] = L':';
        CurrentPos = 2;
    }
    else {
        glPath[0] = L'\0';
    }
    for (int i = PathStackPos - 1; i > 0; i--)
    {
        pt = PathStack[i] * disk->IsLong;
        glPath[CurrentPos++] = L'\\';
        memcpy(&glPath[CurrentPos], disk->fFiles[pt].FileName.c_str(), disk->fFiles[pt].FileName.size() * 2);
        CurrentPos += disk->fFiles[pt].FileName.size();
    }
    glPath[CurrentPos] = L'\\';
    glPath[CurrentPos + 1] = L'\0';
    return glPath;
}
#else
wstring GetPath(DISKHANDLE* disk, int id)
{
    int a = id;
    DWORD pt;
    DWORD PathStack[64];
    int PathStackPos = 0;
    wstring glPath;

    PathStackPos = 0;
    for (int i = 0; i < 64; i++)
    {
        PathStack[PathStackPos++] = a;
        pt = a*disk->IsLong;
        a = disk->fFiles[pt].ParentId.LowPart;

        if (a == 0 || a == 5) {
            break;
        }
    }

    int CurrentPos = 0;
    if (disk->DosDevice != NULL)
    {
        glPath.resize(2);
        glPath[0] = disk->DosDevice;
        glPath[1] = L':';
        CurrentPos = 2;
    }

    for (int i = PathStackPos - 1; i > 0; i--)
    {
        pt = PathStack[i] * disk->IsLong;
        glPath.push_back(L'\\');
        glPath.append(disk->fFiles[pt].FileName);
    }
    glPath.push_back(L'\\');
    glPath.push_back(L'\0');
    return glPath;
}
#endif

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#if 0
LPWSTR GetCompletePath(DISKHANDLE* disk, int id)
{
    int a = id;
    //int i;
    DWORD pt;
    //PUCHAR ptr = (PUCHAR)disk->sFiles;
    DWORD PathStack[64];
    int PathStackPos = 0;
    static WCHAR glPath[0xffff];
    int CurrentPos = 0;

    PathStackPos = 0;
    for (int i = 0; i < 64; i++)
    {
        PathStack[PathStackPos++] = a;
        pt = a*disk->IsLong;
        a = disk->fFiles[pt].ParentId.LowPart;

        if (a == 0 || a == 5) {
            break;
        }
    }
    if (disk->DosDevice != NULL)
    {
        glPath[0] = disk->DosDevice;
        glPath[1] = L':';
        CurrentPos = 2;
    }
    else {
        glPath[0] = L'\0';
    }
    for (int i = PathStackPos - 1; i >= 0; i--)
    {
        pt = PathStack[i] * disk->IsLong;
        glPath[CurrentPos++] = L'\\';
        memcpy(&glPath[CurrentPos], disk->fFiles[pt].FileName, disk->fFiles[pt].FileNameLength * 2);
        CurrentPos += disk->fFiles[pt].FileNameLength;
    }
    glPath[CurrentPos] = L'\0';
    return glPath;
}
#endif