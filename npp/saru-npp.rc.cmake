#pragma code_page(65001)

#include "@BINARY_DIR@/config.h"

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
            VALUE "FileDescription", "SSH Agent Replacement Utility - named pipe proxy\0"
            VALUE "FileVersion", "@VERSION@\0"
            VALUE "LegalCopyright", "Copyright (C) 20@VERSION_Y@ Michał Podsiadlik. License GPLv3+.\0"
            VALUE "ProductName", "SARU\0"
            VALUE "ProductVersion", "@VERSION@\0"
            VALUE "Comments", "This is free software licensed under GPLv3+. There is NO WARRANTY. See https://github.com/michal-pod/saru for more information.\0"
        }
    }
    BLOCK "VarFileInfo"
    {
        VALUE "Translation", 0x0409, 0x04E4
    }
}
