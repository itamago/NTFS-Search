
#include <iostream>

#pragma warning(disable : 4996)

#include "NTFS.h"
#include "SimplePattern.h"

#include "shlwapi.h"  // for PathCombineW
#include <conio.h>    // for _getch

#include <algorithm>
#include <memory>
#include <vector>
#include <string>

using std::wstring;

const auto MAX_RESULTS_NUMBER = 2000000;

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct SearchResult
{
    int icon;
    wstring extra;
    wstring filename;
    wstring path;

    ULONGLONG dataSize;
    ULONGLONG allocatedSize;

};


//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int SearchFiles2(DISKHANDLE* disk, TCHAR *filename, bool deleted, bool caseSensitive, SEARCHP* pat, std::vector<SearchResult> & outResults)
{
    if (disk->fFiles.empty())
        return 0;

    int hit = 0;
    WCHAR tmp[0xffff];
    if (caseSensitive == false)
    {
        _wcslwr(filename);
    }
    
    SEARCHFILEINFO* fileInfo = &disk->fFiles[0];

    for (int i = 0; i < disk->filesSize; i++)
    {
        if (deleted || ((fileInfo[i].Flags & 0x1) != 0))
        {
            if (fileInfo[i].FileName.empty() == false)
            {
                bool ok;
                if (caseSensitive == false)
                {
                    memcpy(tmp, fileInfo[i].FileName.c_str(), fileInfo[i].FileName.size() * sizeof(TCHAR) + 2);
                    _wcslwr(tmp);
                    ok = SearchStr(pat, (wchar_t*)tmp, fileInfo[i].FileName.size());
                }
                else
                {
                    ok = SearchStr(pat, const_cast<wchar_t*>(fileInfo[i].FileName.c_str()), fileInfo[i].FileName.size());
                }
                if (ok)
                {
                    const wstring path = GetPath(disk, i);
                    const auto filename = const_cast<LPTSTR>(fileInfo[i].FileName.c_str());
                    const auto icon = fileInfo[i].Flags;

                    if (fileInfo[i].DataSize == 0 && fileInfo[i].Flags != 0x002) // not directory
                    {
                        WCHAR filePath[0x10000];
                        PathCombineW(filePath, path.c_str(), filename);
                        HANDLE hFile = CreateFile(filePath,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

                        if (hFile != INVALID_HANDLE_VALUE)
                        {
                            FILE_STANDARD_INFO info {};
                            if (GetFileInformationByHandleEx(
                                hFile,
                                FileStandardInfo,
                                &info,
                                sizeof(info)))
                            {
                                fileInfo[i].DataSize = info.EndOfFile.QuadPart;
                                fileInfo[i].AllocatedSize = info.AllocationSize.QuadPart;
                            }

                            CloseHandle(hFile);
                        }
                    }

                    const auto dataSize = fileInfo[i].DataSize;
                    const auto allocatedSize = fileInfo[i].AllocatedSize;

                    //LPTSTR ret;
                    LPTSTR extra;
                    if ((fileInfo[i].Flags & 0x002) == 0)
                    {
                        auto ret = wcsrchr(filename, L'.');
                        if (ret != nullptr) {
                            extra = ret + 1;
                        }
                        else {
                            extra = (LPTSTR) TEXT(" ");
                        }
                    }
                    else
                    {
                        extra = (LPTSTR) TEXT(" ");
                    }

                    outResults.push_back({ icon, extra, filename, path, dataSize, allocatedSize });

                    hit++;
                    if (outResults.size() >= MAX_RESULTS_NUMBER)
                    {
                        // ERROR
                        break;
                    }
                }
            }

        }
    }

    return hit;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool IsRunningAsAdmin() 
{
    HANDLE hToken = nullptr;
    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) 
    {
        std::cerr << "Erreur : Impossible d'ouvrir le token de processus." << std::endl;
        return false;
    }

    if (!GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) 
    {
        std::cerr << "Erreur : Impossible d'obtenir les informations du token." << std::endl;
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);

    // Vérifier si le token est élevé (indique les droits admin)
    return elevation.TokenIsElevated != 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// WARNING : parsing NTFS tables requires Admin rights.
// This application embed a manifest to ask admin rights. 
// This manifest is generated by VisualStudio in : the Properties > Linker > Manifest File
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int main()
{
    if (IsRunningAsAdmin() == false)
    {
        printf("ERROR : Need admin rights to parse NTFS tables\n");
        return 1;
    }
    printf("Check : Admin rights are available to parse NTFS tables.\n");

    // Test
    {
        LPCTSTR diskName = L"\\\\.\\C:";   //  name =    \\.\C:

        DISKHANDLE* diskHandle = OpenDisk(diskName);
        if (diskHandle == NULL)
        {
            printf("ERROR : Cannot open the NTFS table\n");
        }
        else
        {
            ULONGLONG resLoad = LoadMFT(diskHandle, false /*complete*/);
            if (resLoad == 0)
            {
                printf("ERROR : Cannot load the NTFS table\n");
            }
            else
            {
                DWORD progressValue;
                ParseMFT(diskHandle, SEARCHINFO, &progressValue);
                printf("Number of loaded NTFS entries : %d\n", int(progressValue));

                wchar_t* filename = (wchar_t*) L"*.ftxb";
                SEARCHP* pat = StartSearch((wchar_t*)filename, wcslen(filename));
                if (pat != nullptr) 
                {
                    std::vector<SearchResult> outResults;
                    DWORD ret = SearchFiles2(diskHandle, filename, false/*deleted*/, false/*caseSensitive*/, pat, outResults);

                    printf("Count of search results : %d\n", int(outResults.size()));
                }
                else
                {
                    printf("ERROR : Cannot load the NTFS table\n");
                }
            }
            CloseDisk(diskHandle);
        }
    }

    _getch();

    return 0;
}
