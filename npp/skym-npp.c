/*
 SKYM - SSH KeY Manager
 Copyright (C) 2025 Michał Podsiadlik

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <windows.h>
#include <stdio.h>
#include <wintrust.h>
#include <softpub.h>

HANDLE hTerminationEvent = NULL;

/*
 * Thread function to read from standard input
 *
 * Unfortunately, ReadFile on console input cannot be used with OVERLAPPED I/O,
 * so we need a separate thread to monitor standard input and signal the main thread.
 *
 * When input is available, the thread sets an event to notify the main thread.
 */
DWORD WINAPI input_reader_thread(LPVOID lpParam)
{
    HANDLE hEvent = (HANDLE)lpParam;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE)
        return 1;

    while (TRUE)
    {
        if (!ReadFile(hStdin, NULL, 0, NULL, NULL))
        {
            /* If ReadFile fails, signal termination */
            fprintf(stderr, "Failed to read from stdin. Error: %lu\n", GetLastError());
            SetEvent(hTerminationEvent);
            return 1;
        }
        SetEvent(hEvent);
        /* Wait for main thread to process data */
        /*while (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0)
        {
            Sleep(50);
        }*/
    }
    return 0;
}

BOOL verify_client_process(HANDLE hPipe)
{
    ULONG ulClientProcessId;
    if (!GetNamedPipeClientProcessId(hPipe, &ulClientProcessId))
    {
        fprintf(stderr, "Failed to get client process ID. Error: %lu\n", GetLastError());
        return FALSE;
    }

    HANDLE hClientProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ulClientProcessId);
    if (hClientProcess == NULL)
    {
        fprintf(stderr, "Failed to open client process. Error: %lu\n", GetLastError());
        return FALSE;
    }

    WCHAR szProcessName[MAX_PATH];
    DWORD cchProcessName = MAX_PATH;
    if (QueryFullProcessImageNameW(hClientProcess, 0, szProcessName, &cchProcessName))
    {
        // Check signature of executable of process
        LONG lStatus = 0;
        GUID wvtPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        WINTRUST_DATA winTrustData = {0};
        WINTRUST_FILE_INFO fileData = {0};

        fileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
        fileData.pcwszFilePath = szProcessName;

        winTrustData.cbStruct = sizeof(WINTRUST_DATA);
        winTrustData.dwUIChoice = WTD_UI_NONE;
        winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
        winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
        winTrustData.pFile = &fileData;
        winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
        winTrustData.dwProvFlags = WTD_REVOCATION_CHECK_NONE;

        lStatus = WinVerifyTrust(NULL, &wvtPolicyGUID, &winTrustData);
        if (lStatus != ERROR_SUCCESS)
        {
            fprintf(stderr, "Failed to verify process signature. Error: %ld\n", lStatus);
            return FALSE;
        }

        fprintf(stderr, "Client process cert verified: %S (PID=%lu)\n", szProcessName, ulClientProcessId);
    }
    else
    {
        fprintf(stderr, "Failed to get process name. Error: %lu\n", GetLastError());
        CloseHandle(hClientProcess);
        return FALSE;
    }
    CloseHandle(hClientProcess);
    return TRUE;
}

int main()
{
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdout == INVALID_HANDLE_VALUE)
    {
        return 1;
    }
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    if (GetFileType(hStdout) != FILE_TYPE_PIPE || GetFileType(hStdin) != FILE_TYPE_PIPE)
    {
        MessageBoxA(NULL, "This program is intended to be used in a piped context.", "SkyM NPP", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!verify_client_process(hStdout))
    {
        MessageBoxA(NULL, "Failed to verify client process.", "SkyM NPP", MB_OK | MB_ICONERROR);
        return 1;
    }

    HANDLE hAgent = CreateFileA(
        "\\\\.\\pipe\\openssh-ssh-agent",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL);

    if (hAgent == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Failed to create agent pipe. Error: %lu\n", GetLastError());
        return 1;
    }

    DWORD dwMode = PIPE_READMODE_BYTE;
    /* Make sure the pipe is in byte mode */
    if (!SetNamedPipeHandleState(hAgent, &dwMode, NULL, NULL))
    {
        fprintf(stderr, "Failed to set pipe to byte mode. Error: %lu\n", GetLastError());
        CloseHandle(hAgent);
        return 1;
    }

    /* Initialize OVERLAPPED structures and events */
    OVERLAPPED ovIORead = {0};
    OVERLAPPED ovPipeRead = {0};

    HANDLE hEventIORead = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE hEventPipeRead = CreateEvent(NULL, TRUE, FALSE, NULL);
    hTerminationEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (hEventIORead == NULL || hEventPipeRead == NULL || hTerminationEvent == NULL || hEventIORead == INVALID_HANDLE_VALUE || hEventPipeRead == INVALID_HANDLE_VALUE || hTerminationEvent == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Failed to create events. Error: %lu\n", GetLastError());
        CloseHandle(hAgent);
        return 1;
    }

    ovIORead.hEvent = hEventIORead;
    ovPipeRead.hEvent = hEventPipeRead;

    HANDLE hEvents[3] = {
        hEventIORead,
        hEventPipeRead,
        hTerminationEvent};

    char bPipe[32 * 1024];
    char bIO[32 * 1024];
    // Init read from IO and pipe here
    DWORD dwBytesRead = 0;

    /* Create input reader thread */
    HANDLE hInputThread = CreateThread(NULL, 0, input_reader_thread, hEventIORead, 0, NULL);
    if (!hInputThread)
    {
        fprintf(stderr, "Failed to create input reader thread. Error: %lu\n", GetLastError());
        CloseHandle(hEventIORead);
        CloseHandle(hEventPipeRead);
        CloseHandle(hAgent);
        return 1;
    }

    dwBytesRead = 0;

    /* Read initial data from agent pipe */
    if (!ReadFile(hAgent, bPipe, 32 * 1024, &dwBytesRead, &ovPipeRead) && GetLastError() != ERROR_IO_PENDING)
    {
        fprintf(stderr, "ReadFile from Agent failed. Error: %lu\n", GetLastError());
        CloseHandle(hEventIORead);
        CloseHandle(hEventPipeRead);
        CloseHandle(hAgent);
        return 1;
    }

    BOOL bContinueLoop = TRUE;

    /*
     * All writes are done synchronously for simplicity, because after every write we need to wait for more input.
     * this makes the code simpler without impact on functionality because we always wait for input after every write.
     *
     * Write errors are handled immediately.
     */
    while (bContinueLoop)
    {
        DWORD dwWait = WaitForMultipleObjects(3, hEvents, FALSE, INFINITE);
        DWORD dwBytesTransferred;
        DWORD dwBytesWritten;
        switch (dwWait)
        {
        case WAIT_OBJECT_0:
            if (!ReadFile(hStdin, bIO, sizeof(bIO), &dwBytesTransferred, NULL))
            {
                fprintf(stderr, "Sync ReadFile from Stdin failed. Error: %lu\n", GetLastError());
                bContinueLoop = FALSE;
                continue;
            }

            if (!WriteFile(hAgent, bIO, dwBytesTransferred, &dwBytesWritten, NULL))
            {
                fprintf(stderr, "WriteFile to Agent failed. Error: %lu\n", GetLastError());
                bContinueLoop = FALSE;
                continue;
            }

            ResetEvent(hEventIORead);

            break;
        case WAIT_OBJECT_0 + 1:
            if (GetOverlappedResult(hAgent, &ovPipeRead, &dwBytesTransferred, FALSE))
            {
                if (dwBytesTransferred == 0)
                {
                    fprintf(stderr, "EOF on Agent pipe detected. Exiting.\n");
                    bContinueLoop = FALSE;
                    continue;
                }
                if (!WriteFile(hStdout, bPipe, dwBytesTransferred, &dwBytesWritten, NULL))
                {
                    fprintf(stderr, "WriteFile to Stdout failed. Error: %lu\n", GetLastError());
                    bContinueLoop = FALSE;
                    continue;
                }

                // Re-initiate read from agent pipe
                ZeroMemory(&ovPipeRead, sizeof(OVERLAPPED));
                ovPipeRead.hEvent = hEventPipeRead;
                ResetEvent(hEventPipeRead);
                if (ReadFile(hAgent, bPipe, sizeof(bPipe), &dwBytesRead, &ovPipeRead))
                {
                }
                else if (GetLastError() != ERROR_IO_PENDING)
                {
                    fprintf(stderr, "ReadFile from Agent failed. Error: %lu\n", GetLastError());
                    bContinueLoop = FALSE;
                    continue;
                }
            }
            else
            {
                fprintf(stderr, "GetOverlappedResult for Pipe Read failed. Error: %lu\n", GetLastError());
                bContinueLoop = FALSE;
                continue;
            }
            break;
        case WAIT_OBJECT_0 + 2:
            fprintf(stderr, "Termination event signaled. Exiting main loop.\n");
            bContinueLoop = FALSE;
            break;
        default:
            fprintf(stderr, "WaitForMultipleObjects failed. Error: %lu\n", GetLastError());
            bContinueLoop = FALSE;
            break;
        }
    }

    CloseHandle(hEventIORead);
    CloseHandle(hEventPipeRead);
    CloseHandle(hInputThread);
    CloseHandle(hAgent);

    fprintf(stderr, "SkyM NPP agent terminated, status: %d\n", bContinueLoop ? 0 : 1);

    return bContinueLoop ? 0 : 1;
}
