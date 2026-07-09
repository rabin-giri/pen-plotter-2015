/*
 * bluetooth.h
 *
 * Description:
 * Windows desktop driver interfaces for Serial-over-Bluetooth communication
 * and local port configuration state caching.
 */

#ifndef BLUETOOTH_H_
#define BLUETOOTH_H_

#include <Windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <tchar.h>
#include <iostream>

#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "winmm.lib")

// ---------------------------------------------------------
// High-Resolution Win32 Performance Timer
// ---------------------------------------------------------
class PerformanceTimer
{
private:
    LARGE_INTEGER frequency;        
    LARGE_INTEGER tStart;
    LARGE_INTEGER tEnd;

public:
    PerformanceTimer() 
    { 
        QueryPerformanceFrequency(&frequency); 
    }

    void LogTime() 
    { 
        QueryPerformanceCounter(&tStart); 
    }

    // Returns elapsed time in milliseconds since LogTime() was called
    double GetDeltaTime(void) 
    { 
        QueryPerformanceCounter(&tEnd); 
        return ((tEnd.QuadPart - tStart.QuadPart) * 1000.0 / frequency.QuadPart); 
    }
};

// ---------------------------------------------------------
// Bluetooth Serial Communication Handler
// ---------------------------------------------------------
class BluetoothSerial
{
private:
    char port_no[6];
    HANDLE hSerial;
    PerformanceTimer timer;

public:
    unsigned char receiveData;
    bool isConnected;

    BluetoothSerial(void);
    ~BluetoothSerial()
    { 
        if (this->isConnected) 
        {
            CloseHandle(this->hSerial); 
        }
    }

    bool ConnectToPlotter(void);
    bool Disconnect(void);
    bool SendByte(unsigned char ch);
    unsigned char ReadByte(void);
    void RefreshBuffer(void);
    void FlushBuffers(void);
};

// ---------------------------------------------------------
// Persistent Port State Cache File Handler
// ---------------------------------------------------------
class PortCacheManager
{
private:
    FILE *filePointer;
    int previousPlotterPort;

public:
    int currentPlotterPort;
    
    PortCacheManager();
    ~PortCacheManager();
};

#endif // BLUETOOTH_H_
