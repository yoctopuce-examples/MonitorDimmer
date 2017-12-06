// MonitorDimmer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Windows.h"
#include "HighLevelMonitorConfigurationAPI.h"
#include <iostream>
#include <strsafe.h>
#pragma comment(lib, "Dxva2.lib")
#include "yocto_api.h"
#include "yocto_lightsensor.h"
#include <stdlib.h>

using namespace std;


void fatal_error(LPTSTR lpszFunction)
{
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDisplayBuf,
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"),
        lpszFunction, dw, lpMsgBuf);
    std::wcerr << (LPTSTR)lpDisplayBuf << std::endl;
    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    Sleep(100000);
    ExitProcess(dw);
}

typedef struct
{
    HANDLE handle;
    DWORD minBrightness;
    DWORD curBrightness;
    DWORD maxBrightness;
    wstring* name;
} monitor_status;

monitor_status monitors[4];
int nb_usable_monitor = 0;
double min_lux = 0;
double max_lux = 200;


void timedCallback(YLightSensor* func, YMeasure measure)
{
    u64 start = YAPI::GetTickCount();
    double value = measure.get_averageValue();
    for (int i = 0; i < nb_usable_monitor; i++) {
        int luminosity;
        monitor_status* m = monitors + i;
        if (value < min_lux) {
            luminosity = m->minBrightness;
        } else if (value > max_lux) {
            luminosity = m->maxBrightness;
        } else {
            double monitor_range = monitors[i].maxBrightness - monitors[i].minBrightness;
            double corrected_luminosity = (value - min_lux) * monitor_range / (max_lux - min_lux);
            luminosity = (int)(corrected_luminosity + monitors[i].minBrightness + 0.5);
        }

        if (monitors[i].curBrightness != luminosity) {
            wcout << "change brightness of " << *(monitors[i].name) << " to " << luminosity << "% (" << value << " Lux)" << endl;
            BOOL res = SetMonitorBrightness(monitors[i].handle, luminosity);
            if (res) {
                monitors[i].curBrightness = luminosity;
            } else {
                fatal_error(TEXT("GetMonitorBrightness"));
            }
        }
    }
    u64 stop = YAPI::GetTickCount();
    cout << "update took " << (stop - start) << "ms" << endl;
}


int main(int argc, char *argv[])
{
    string errmsg;

    PHYSICAL_MONITOR* physical_monitor;
    DWORD number_of_physical_monitors;
    BOOL res;

    HWND hWnd = GetDesktopWindow();
    HMONITOR main_hmonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);

    res = GetNumberOfPhysicalMonitorsFromHMONITOR(main_hmonitor, &number_of_physical_monitors);
    if (!res) {
        fatal_error(TEXT("GetNumberOfPhysicalMonitorsFromHMONITOR"));
    }
    physical_monitor = (PHYSICAL_MONITOR*)malloc(number_of_physical_monitors * sizeof(PHYSICAL_MONITOR));
    res = GetPhysicalMonitorsFromHMONITOR(main_hmonitor, number_of_physical_monitors, physical_monitor);
    if (!res) {
        fatal_error(TEXT("GetPhysicalMonitorsFromHMONITOR"));
    }
    for (DWORD i = 0; i < number_of_physical_monitors; i++) {
        std::wcout << TEXT("Physical Monitor ") << physical_monitor[i].szPhysicalMonitorDescription << std::endl;
        monitor_status* p = monitors + nb_usable_monitor;
        res = GetMonitorBrightness(physical_monitor[i].hPhysicalMonitor, &p->minBrightness, &p->curBrightness, &p->maxBrightness);
        if (!res) {
            fatal_error(TEXT("GetMonitorBrightness"));
            continue;
        }
        std::cout << " min:" << p->minBrightness << " cur:" << p->curBrightness << " max:" << p->maxBrightness << std::endl;
        p->handle = physical_monitor[i].hPhysicalMonitor;
        p->name = new wstring(physical_monitor[i].szPhysicalMonitorDescription);
        nb_usable_monitor++;
    }


    // Setup the API to use local USB devices
    if (YAPI::RegisterHub("usb", errmsg) != YAPI_SUCCESS) {
        cerr << TEXT("YAPI::RegisterHub error: ") << errmsg << endl;
        return 1;
    }

    YLightSensor* sensor = YLightSensor::FirstLightSensor();
    if (sensor == NULL) {
        wcout << TEXT("No Yocto-Light connected (check USB cable)") << endl;
        return 1;
    }

    sensor->setReportFrequency("60/m");

    sensor->registerTimedReportCallback(timedCallback);

    int count = 0;
    while (1) {
        res = YAPI::Sleep(1000, errmsg);
        if (res != YAPI::SUCCESS) {
            cerr << "YAPI::Sleep error: " << errmsg << endl;
            return 1;
        }
        count++;
        if (count > 10) {
            res = YAPI::UpdateDeviceList(errmsg);
            if (res != YAPI::SUCCESS) {
                cerr << "YAPI::UpdateDeviceList error: " << errmsg << endl;
                return 1;
            }
        }
    }
    return 0;
}

