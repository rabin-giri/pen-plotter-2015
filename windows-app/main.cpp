/*
 * main.cpp
 *
 * Description:
 * Win32 Desktop GUI application that serves as the G-code streamer interface.
 * Spawns a graphical interface alongside an allocated debug console, handles
 * file dialog inputs, and manages the character-by-character transmission 
 * protocol over Bluetooth.
 */

#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <Windows.h>
#include <iostream>
#include <tchar.h>
#include <string.h>
#include "resource.h"
#include "bluetooth.h"

// ---------------------------------------------------------
// Forward Declarations & Global Contexts
// ---------------------------------------------------------
void StreamGCodeFile(char* path);
void AllocateDebugConsole(void);
void RunCommandLineInterface(void); 

BluetoothSerial bluetooth;
PerformanceTimer timeoutTimer;

LRESULT CALLBACK WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------
// Win32 Main Entry Point
// ---------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd)
{
    WNDCLASSEX wClass;
    ZeroMemory(&wClass, sizeof(WNDCLASSEX));
    
    wClass.cbClsExtra    = NULL;
    wClass.cbSize        = sizeof(WNDCLASSEX);
    wClass.cbWndExtra    = NULL;
    wClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wClass.hIcon         = NULL;
    wClass.hIconSm       = NULL;
    wClass.hInstance     = hInst;
    wClass.lpfnWndProc   = (WNDPROC)WinProc;
    wClass.lpszClassName = "Window Class";
    wClass.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU1);
    wClass.style         = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassEx(&wClass))
    {
        MessageBox(NULL, "Window class creation failed", "Window Class Failed", MB_ICONERROR);
        return 0;
    }

    HWND hWnd = CreateWindowEx(NULL, "Window Class", "Pen Plotter", WS_OVERLAPPEDWINDOW, 600, 0, 600, 700, NULL, NULL, hInst, NULL);

    if (!hWnd)
    {
        MessageBox(NULL, "Window creation failed", "Error Message", MB_ICONERROR);
        return 0;
    }

    ShowWindow(hWnd, nShowCmd);
    AllocateDebugConsole(); // Attaches a standard command console for stdout debugging

    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));
    std::cout << "Welcome..";
    
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// ---------------------------------------------------------
// Window Message Event Processor (Window Procedure)
// ---------------------------------------------------------
LRESULT CALLBACK WinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC Hdc = BeginPaint(hWnd, &ps);
            
            HBITMAP hbmp1 = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BITMAP2));
            HDC hmemdc = CreateCompatibleDC(Hdc);
            HGDIOBJ holdbmp = SelectObject(hmemdc, hbmp1);
            
            // Draw background splash bitmap layout
            BitBlt(Hdc, 0, 0, 600, 700, hmemdc, 0, 0, SRCCOPY);
            
            SelectObject(hmemdc, holdbmp);
            DeleteObject(hbmp1);
            DeleteDC(hmemdc);
            EndPaint(hWnd, &ps);
        }
        break;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case ID_FILE_OPEN40001:
                {
                    TCHAR szFilters[] = _T("Scribble Files (*.gcode)\0*.gcode\0\0");
                    TCHAR szFilePathName[_MAX_PATH] = _T("");
                    OPENFILENAME ofn = { 0 };
                    
                    ofn.lStructSize = sizeof(OPENFILENAME);
                    ofn.hwndOwner   = hWnd;
                    ofn.lpstrFilter = szFilters;
                    ofn.lpstrFile   = szFilePathName;
                    ofn.nMaxFile    = _MAX_PATH;
                    ofn.lpstrTitle  = _T("Open File");
                    ofn.Flags       = OFN_FILEMUSTEXIST;

                    if (::GetOpenFileName(&ofn))
                    {
                        std::cout << szFilePathName << std::endl;
                        if (szFilePathName[0] != _T('\0'))
                        {
                            StreamGCodeFile(szFilePathName);
                        }
                    }
                }
                break;

                case 40005: // Credits Info Screen
                    std::system("cls");
                    std::cout << " Pen Plotter DriverApps [Version 1.0.0000]" << std::endl;
                    std::cout << " <c> 2015 Robotics Club, IOE Central Campus. All rights reserved." << std::endl << std::endl;
                    std::cout << " Project by: Bimal Paneru" << std::endl;
                    std::cout << "           : Niraj Basnet" << std::endl;
                    std::cout << "           : Sagar Shrestha" << std::endl;
                    std::cout << "           : Rabin Giri" << std::endl;
                    break;

                case ID_GCODE_CLIINSERT:
                    RunCommandLineInterface();
                    break;

                case ID_PRINTER_CONNECT:
                    bluetooth.ConnectToPlotter();
                    break;

                case ID_PRINTER_DISCONNECT:
                    bluetooth.Disconnect();
                    break;

                case ID_FILE_EXIT:
                    PostQuitMessage(0);
                    break;
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// ---------------------------------------------------------
// G-Code Transmission Engine
// ---------------------------------------------------------

/*
 * Reads a raw text G-code file and streams lines over Bluetooth.
 * Custom Transmission Protocol:
 * 1. Read byte 'ch' from file.
 * 2. Send 'ch' via Bluetooth, then loop/block until AVR returns 'ch + 1' as acknowledgment.
 * 3. At line breaks ('\n' or EOF), send down a '#' end-of-line marker.
 * 4. Wait for AVR to confirm with '# + 1' before loading the next line, or timeout after 20s.
 */
void StreamGCodeFile(char* path)
{
    FILE *fptr;
    char ch;
    int lineCount = 0;
    
    if (fopen_s(&fptr, path, "r") != 0 || !fptr)
    {
        std::cout << "Error opening file";
        return;
    }
    
    std::system("cls");
    
    while ((ch = fgetc(fptr)) != EOF)
    {
        if (ch == '\n')
        {
        ProcessNextLine:
            ch = fgetc(fptr);
            if (ch == 'G' || ch == 'M')
            {
                std::cout << "\n" << lineCount++ << ":";
                while (1)
                {
                    std::cout << ch;
                    bluetooth.SendByte(ch);
                    
                    // Synchronous character echo verification loop
                    while (bluetooth.ReadByte() != (ch + 1));
                    
                    ch = fgetc(fptr);
                    
                    if (ch == '\n')
                    {
                        bluetooth.SendByte('#');
                        timeoutTimer.LogTime();
                        
                        // Synchronous line-end acknowledgment loop with a physical timeout guard
                        while (bluetooth.ReadByte() != ('#' + 1))
                        { 
                            if (timeoutTimer.GetDeltaTime() >= 20000) 
                                break;
                        }
                        goto ProcessNextLine;
                    }
                    else if (ch == '(') // Skip comments blocks
                    {
                        break;
                    }
                    else if (ch == EOF)
                    {
                        bluetooth.SendByte('#');
                        timeoutTimer.LogTime();
                        while (bluetooth.ReadByte() != ('#' + 1))
                        {
                            if (timeoutTimer.GetDeltaTime() >= 20000) 
                                break;
                        }
                        goto EndOfFileStream;
                    }
                }
                bluetooth.SendByte('#');
                while (bluetooth.ReadByte() != ('#' + 1));
            }
        }
    EndOfFileStream:
        if (ch == EOF) 
            break;
    }

    fclose(fptr);
    std::cout << "\nTransmission complete." << std::endl;
}

/*
 * Allocates a separate console environment subsystem to route standard input/output
 * stream handles (stdin/stdout) for terminal debugging alongside a Win32 GUI window.
 */
void AllocateDebugConsole(void)
{
    AllocConsole();
    
    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    int hCrt = _open_osfhandle((intptr_t)handle_out, _O_TEXT);
    FILE* hf_out = _fdopen(hCrt, "w");
    setvbuf(hf_out, NULL, _IONBF, 1);
    *stdout = *hf_out;

    HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
    hCrt = _open_osfhandle((intptr_t)handle_in, _O_TEXT);
    FILE* hf_in = _fdopen(hCrt, "r");
    setvbuf(hf_in, NULL, _IONBF, 128);
    *stdin = *hf_in;
}

/*
 * Interactive Command Line Interface block allowing raw manual 
 * entry testing of targeted G-code lines directly to the serial queue.
 */
void RunCommandLineInterface(void)
{
    char commandInput[200];
    char confirmation[100];
    
    while (1)
    {
        std::system("cls");
        std::cout << "Insert gcode or write rtr for normal mode" << std::endl;
        std::cin >> commandInput;
        
        if (!_stricmp(commandInput, "rtr"))
        {
            std::system("cls");
            return;
        }
        
        std::cout << std::endl << "Do you want to send to printer? Write Y or N: ";
        std::cin >> confirmation;
        
        if (!_stricmp(confirmation, "Y"))
        {
            std::cout << commandInput;
            // Note: Manual transmission execution logic bridges here
        }
    }
}
