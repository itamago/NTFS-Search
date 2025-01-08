
#include <iostream>

#pragma warning(disable : 4996)

#include "NTFS_STRUCT.h"
#include "SimplePattern.h"

#include "shlwapi.h"  // for PathCombineW
#include <conio.h>    // for _getch

#include <algorithm>
#include <memory>
#include <vector>

PHEAPBLOCK FileStrings;
PHEAPBLOCK PathStrings;
const auto MAX_RESULTS_NUMBER = 2000000;

typedef struct SearchResult
{
    int icon;
    LPTSTR extra;
    LPTSTR filename;
    LPTSTR path;

    ULONGLONG dataSize;
    ULONGLONG allocatedSize;

}*PSearchResult;


int SearchFiles2(PDISKHANDLE disk, TCHAR *filename, bool deleted, bool caseSensitive, SEARCHP* pat, std::vector<SearchResult> & outResults)
{
    int hit = 0;
    WCHAR tmp[0xffff];
    SEARCHFILEINFO *info;
    if (caseSensitive == false)
    {
        _wcslwr(filename);
    }
    info = disk->fFiles;

    for (int i = 0; i < disk->filesSize; i++)
    {
        if (deleted || ((info[i].Flags & 0x1) != 0))
        {
            if (info[i].FileName != nullptr)
            {
                bool ok;
                if (caseSensitive == false)
                {
                    memcpy(tmp, info[i].FileName, info[i].FileNameLength * sizeof(TCHAR) + 2);
                    _wcslwr(tmp);
                    ok = SearchStr(pat, (wchar_t*)tmp, info[i].FileNameLength);
                }
                else
                {
                    ok = SearchStr(pat, const_cast<wchar_t*>(info[i].FileName), info[i].FileNameLength);
                }
                if (ok)
                {
                    const auto t = GetPath(disk, i);
                    const auto s = wcslen(t);
                    const auto filename = const_cast<LPTSTR>(info[i].FileName);
                    const auto path = AllocAndCopyString(PathStrings, t, s);
                    const auto icon = info[i].Flags;

                    if (info[i].DataSize == 0 && info[i].Flags != 0x002) // not directory
                    {
                        WCHAR filePath[0x10000];
                        PathCombineW(filePath, path, filename);
                        HANDLE hFile = CreateFile(filePath,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);
                        if (hFile != INVALID_HANDLE_VALUE)
                        {
                            FILE_STANDARD_INFO fileInfo {};
                            if (GetFileInformationByHandleEx(
                                hFile,
                                FileStandardInfo,
                                &fileInfo,
                                sizeof(fileInfo)))
                            {
                                info[i].DataSize = fileInfo.EndOfFile.QuadPart;
                                info[i].AllocatedSize = fileInfo.AllocationSize.QuadPart;
                            }

                            CloseHandle(hFile);
                        }
                    }

                    const auto dataSize = info[i].DataSize;
                    const auto allocatedSize = info[i].AllocatedSize;

                    //LPTSTR ret;
                    LPTSTR extra;
                    if ((info[i].Flags & 0x002) == 0)
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

bool IsRunningAsAdmin() 
{
    HANDLE hToken = nullptr;
    TOKEN_ELEVATION elevation;
    DWORD size;

    // Ouvrir un handle au token d'accès du processus courant
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) 
    {
        std::cerr << "Erreur : Impossible d'ouvrir le token de processus." << std::endl;
        return false;
    }

    // Obtenir les informations d'élévation du token
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

// WARNING : parsing NTFS tables requires Admin rights.
// This application embed a manifest to ask admin rights. 
// This manifest is generated by VisualStudio in : the Properties > Linker > Manifest File
int main()
{
    if (IsRunningAsAdmin() == false)
    {
        printf("ERROR : Need admin rights to parse NTFS tables\n");
        return 1;
    }
    printf("Check : Admin rights are available to parse NTFS tables.\n");

    FileStrings = CreateHeap(0xffff * sizeof(SearchResult));
    PathStrings = CreateHeap(0xfff * MAX_PATH);

    // Test
    {
        LPCTSTR diskName = L"\\\\.\\C:";   //  name =    \\.\C:

        PDISKHANDLE diskHandle = OpenDisk(diskName);
        if (diskHandle == NULL)
        {
            printf("ERROR : Cannot open the NTFS table\n");
        }
        else
        {
            ULONGLONG resLoad = LoadMFT(diskHandle, FALSE /*complete*/);
            if (resLoad == 0)
            {
                printf("ERROR : Cannot load the NTFS table\n");
            }
            else
            {
                DWORD progressValue;
                ParseMFT2(diskHandle, SEARCHINFO, &progressValue);
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

    FreeHeap(PathStrings);
    FreeHeap(FileStrings);

    _getch();

    return 0;
}
