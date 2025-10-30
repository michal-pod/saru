// 😝
#pragma code_page(65001)

#include "config.h"

1 VERSIONINFO
FILEVERSION     @VERSION_Y@, @VERSION_M@, @VERSION_D@, @VERSION_B@
PRODUCTVERSION  @VERSION_Y@, @VERSION_M@, @VERSION_D@, @VERSION_B@
FILEOS          0x40004
FILETYPE        0x1
FILESUBTYPE     0x0
{
    BLOCK "StringFileInfo"
    {
        BLOCK "040904E4"
        {
            VALUE "CompanyName", "nglab\0"
            VALUE "FileDescription", "SSH KeY Manager\0"
            VALUE "FileVersion", "@VERSION@\0"
            VALUE "LegalCopyright", "Copyright (C) 20@VERSION_Y@ Michał Podsiadlik. All rights reserved.\0"
            VALUE "ProductName", "SKYM\0"
            VALUE "ProductVersion", "@VERSION@\0"
        }
    }
    BLOCK "VarFileInfo"
    {
        VALUE "Translation", 0x0409, 0x04E4
    }
}
