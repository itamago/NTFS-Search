// Code to fix the structures but only for filenames

#include "FixList.h"

LINKITEM *fixlist=nullptr;
LINKITEM *curfix=nullptr;


void AddToFixList(int entry, int data)
{
    curfix->entry = entry;
    curfix->data = data;
    curfix->next = new LINKITEM;
    curfix = curfix->next;
    curfix->next = nullptr;
}

void CreateFixList()
{
    fixlist = new LINKITEM;
    fixlist->next = nullptr;
    curfix = fixlist;
}

void ProcessFixList(DISKHANDLE* disk)
{
    while (fixlist->next!=nullptr)
    {
        auto info = &disk->fFiles[fixlist->entry];
        auto src = &disk->fFiles[fixlist->data];
        info->FileName = src->FileName;
        info->ParentId = src->ParentId;
        // hide all that we used for cleanup
        src->ParentId.QuadPart = 0;
        LINKITEM *item;
        item = fixlist;
        fixlist = fixlist->next;
        delete item;
    }
    fixlist = nullptr;
    curfix = nullptr;
}

