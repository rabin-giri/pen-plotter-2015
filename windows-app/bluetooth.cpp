/*
 * bluetooth.cpp
 *
 * Description:
 * Implementation of Windows Bluetooth serial handshaking and file caching.
 * Brute-forces available Virtual COM ports to identify and lock onto the 
 * plotter microcontroller hardware stream.
 */

#include "bluetooth.h"

// Global configuration file instance
PortCacheManager portCache;

// ---------------------------------------------------------
// BluetoothSerial Class Implementation
// ---------------------------------------------------------

BluetoothSerial::BluetoothSerial(void)
{ 
    this->isConnected = false; 
    this->hSerial = INVALID_HANDLE_VALUE;
} 

/*
 * Scans through COM ports 0-19 to find the active plotter hardware.
 * Sends a dummy byte '*' and listens for an acknowledgment bit mask (0xa0).
 */
bool BluetoothSerial::ConnectToPlotter(void)
{
    char comport[6];
    char temp[3];
    char dummy = '*';
    
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 50;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    DWORD dwBytesRead = 0;

    std::system("cls");
    std::cout << " Connecting pen plotter hardware............" << std::endl;
    
    for (int i = 0; i < 20; i++)
    {
        // First iteration checks the historically cached port number
        if (i == 0)
            _itoa_s(portCache.currentPlotterPort, temp, 10);
        else
            _itoa_s(i, temp, 10);

        strcpy_s(comport, "com");
        strcat_s(comport, temp);

        this->hSerial = CreateFile(comport, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        
        if (this->hSerial != INVALID_HANDLE_VALUE)
        {
            DCB dcbSerialParams = { 0 };
            dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
            
            if (!GetCommState(this->hSerial, &dcbSerialParams))
            {
                CloseHandle(this->hSerial);
                continue;
            }

            dcbSerialParams.BaudRate = CBR_38400;
            dcbSerialParams.ByteSize = 8;
            dcbSerialParams.StopBits = ONESTOPBIT;
            dcbSerialParams.Parity   = NOPARITY;
            
            SetCommState(this->hSerial, &dcbSerialParams);
            SetCommTimeouts(this->hSerial, &timeouts);
            
            // Handshake polling attempts
            for (int j = 0; j < 5; j++)
            {
                WriteFile(this->hSerial, &dummy, 1, NULL, NULL);
                Sleep(1);
                
                ReadFile(this->hSerial, &this->receiveData, 1, &dwBytesRead, NULL);
                this->receiveData &= 0xf0; // Mask out the upper nibble
                
                if (this->receiveData == 0xa0)
                {
                    this->isConnected = true;
                    std::system("cls");
                    std::cout << "Successfully connected to penplotter on " << comport << std::endl;
                    portCache.currentPlotterPort = i; // Cache successful connection port
                    return true;
                }
                Sleep(1);
            }
            CloseHandle(this->hSerial);
        }
    }
    
    std::system("cls");
    std::cout << " Error connecting penplotter" << std::endl;
    return false;
}

bool BluetoothSerial::Disconnect(void)
{
    this->receiveData = 0x00;
    if (this->isConnected)
    {
        this->FlushBuffers();
        CloseHandle(this->hSerial);
        std::system("cls");
        std::cout << "Penplotter disconnected" << std::endl;
        this->isConnected = false;
        return true;
    }
    return false;
}

bool BluetoothSerial::SendByte(unsigned char ch)
{
    if (this->isConnected)
    {
        DWORD dwBytesWritten = 0;
        if (!WriteFile(this->hSerial, &ch, 1, &dwBytesWritten, NULL))
            return false;
        return true;
    }
    return false;
}

unsigned char BluetoothSerial::ReadByte(void)
{
    unsigned char buf = 0;
    DWORD dwBytesRead = 0;
    ReadFile(this->hSerial, &buf, 1, &dwBytesRead, NULL);
    return buf;
}

void BluetoothSerial::RefreshBuffer(void)
{
    this->receiveData = 0;
}

void BluetoothSerial::FlushBuffers(void)
{
    if (this->isConnected)
    {
        PurgeComm(this->hSerial, PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
    }
}

// ---------------------------------------------------------
// PortCacheManager Class Implementation
// ---------------------------------------------------------

/*
 * Constructor attempts to load the last successful port number 
 * from physical drive caches (checking D: and E: drives).
 */
PortCacheManager::PortCacheManager()
{
    errno_t err;
    this->previousPlotterPort = 1;
    this->currentPlotterPort = 1;

    if ((err = fopen_s(&this->filePointer, "D:/Pen_Plotter_port.txt", "r")) == 0)
    {
        fscanf_s(filePointer, "%d", &this->currentPlotterPort);
        this->previousPlotterPort = this->currentPlotterPort;
        fclose(filePointer);
    }
    else if ((err = fopen_s(&this->filePointer, "E:/Pen_Plotter_port.txt", "r")) == 0)
    {
        fscanf_s(filePointer, "%d", &this->currentPlotterPort);
        this->previousPlotterPort = this->currentPlotterPort;
        fclose(filePointer);
    }
}

/*
 * Destructor commits the active port number back to the drive text configuration 
 * if it changed during runtime execution.
 */
PortCacheManager::~PortCacheManager()
{
    errno_t err;
    if (this->currentPlotterPort != this->previousPlotterPort)
    {
        if ((err = fopen_s(&this->filePointer, "D:/Pen_Plotter_port.txt", "w")) == 0)
        {
            fprintf(filePointer, "%d", this->currentPlotterPort);
            fclose(filePointer);
        }
        else if ((err = fopen_s(&this->filePointer, "E:/Pen_Plotter_port.txt", "w")) == 0)
        {
            fprintf(filePointer, "%d", this->currentPlotterPort);
            fclose(filePointer);
        }
    }
}
