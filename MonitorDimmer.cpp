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


void error(LPTSTR lpszFunction, BOOL fatal)
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
    if (fatal) {
        Sleep(100000);
        ExitProcess(dw);
    }
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
double min_lux;
double max_lux;
string hub_url = "usb";
bool debug = false;
bool liveValue = false;


void timedCallback(YLightSensor* func, YMeasure measure)
{
    double value = measure.get_averageValue();
    if (liveValue) {
        wcout << "Current value of light sensor: " << value << " Lux" << endl;
    }
    for (int i = 0; i < nb_usable_monitor; i++) {
        int luminosity;
        monitor_status* m = monitors + i;
        if (value < min_lux) {
            luminosity = m->minBrightness;
        } else {
            if (value > max_lux) {
                luminosity = m->maxBrightness;
            } else {
                double monitor_range = monitors[i].maxBrightness - monitors[i].minBrightness;
                double corrected_luminosity = (value - min_lux) * monitor_range / (max_lux - min_lux);
                luminosity = (int)(corrected_luminosity + monitors[i].minBrightness + 0.5);
            }
        }

        if (monitors[i].curBrightness != luminosity) {
            if (debug) {
                wcout << "change brightness of " << *(monitors[i].name) << " to " << luminosity << "% (" << value << " Lux)" << endl;
            }
            BOOL res = SetMonitorBrightness(monitors[i].handle, luminosity);
            if (res) {
                monitors[i].curBrightness = luminosity;
            } else {
                error(TEXT("GetMonitorBrightness"), true);
            }
        }
    }
}


int CALLBACK MyInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    PHYSICAL_MONITOR* physical_monitor;
    DWORD number_of_physical_monitors;
    bool res;
    res = GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor,
                                                  &number_of_physical_monitors);
    if (!res) {
        error(TEXT("GetNumberOfPhysicalMonitorsFromHMONITOR"), true);
    }
    physical_monitor = (PHYSICAL_MONITOR*)malloc(
        number_of_physical_monitors * sizeof(PHYSICAL_MONITOR));
    res = GetPhysicalMonitorsFromHMONITOR(hMonitor,
                                          number_of_physical_monitors,
                                          physical_monitor);
    if (!res) {
        error(TEXT("GetPhysicalMonitorsFromHMONITOR"), true);
    }
    for (DWORD i = 0; i < number_of_physical_monitors; i++) {
        std::wcout << TEXT("Physical Monitor ") << physical_monitor[i].szPhysicalMonitorDescription << std::endl;
        monitor_status* p = monitors + nb_usable_monitor;
        res = GetMonitorBrightness(physical_monitor[i].hPhysicalMonitor,
                                   &p->minBrightness,
                                   &p->curBrightness,
                                   &p->maxBrightness);
        if (!res) {
            error(TEXT("GetMonitorBrightness"), false);
            continue;
        }
        if (debug) {
            std::cout << " min:" << p->minBrightness << " cur:" << p->curBrightness << " max:" << p->maxBrightness << std::endl;
        }
        p->handle = physical_monitor[i].hPhysicalMonitor;
        p->name = new wstring(physical_monitor[i].szPhysicalMonitorDescription);
        nb_usable_monitor++;
    }
    return TRUE;
}


static void usage()
{
    printf("Usage: MonitorDimmer [options]\n");
    printf("\n");
    printf("options:\n");
    printf("  -r remoteAddr : Uses remote IP devices (or VirtalHub), instead of local USB.\n");
    printf("  -m min_lux    : the Lux value for the minimal brightness.\n");
    printf("  -M min_lux    : the Lux value for the maximal brightness.\n");
    printf("  -v            : verbose mode\n");
    printf("  -l            : display Lux value of Yocto-Light-V3\n");
    printf("  -h            : display help\n");
    printf("\n");
    printf("Please contact support@yoctopuce.com if you need assistance.\n");
    exit(0);
}


int ParseArguments(int argc, char* argv[])
{
    int i;
    min_lux = 50;
    max_lux = 300;


    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || strlen(argv[i]) > 2) {
            printf("Unknown option %s\n", argv[i]);
            exit(1);
        }
        switch (argv[i][1]) {
        case 'r':
            i++;
            if (i < argc) {
                hub_url = string(argv[i]);
            } else {
                printf("MonitorDimmer: -n option requires an argument\n");
                printf("Try \"MonitorDimmer -h\" for more information.\n");
                exit(1);
            }
            break;
        case 'm':
            i++;
            if (i < argc) {
                min_lux = (double)atoi(argv[i]);
            } else {
                printf("MonitorDimmer: -m option requires an integer\n");
                printf("Try \"MonitorDimmer -h\" for more information.\n");
                exit(1);
            }
            break;

        case 'M':
            i++;
            if (i < argc) {
                max_lux = (double)atoi(argv[i]);
            } else {
                printf("MonitorDimmer: -m option requires an integer\n");
                printf("Try \"MonitorDimmer -h\" for more information.\n");
                exit(1);
            }
            break;
        case 'v':
            debug = true;
            break;
        case 'l':
            liveValue = true;
            break;
        case 'h':
            usage();
            break;
        default:
            printf("Unknown option -%c\n", argv[i][1]);
            exit(1);
        }
    }

    return 1;
}

int main(int argc, char* argv[])
{
    string errmsg;

    ParseArguments(argc, argv);


    EnumDisplayMonitors(NULL, NULL, MyInfoEnumProc, 0);

    // Setup the API to use local USB devices
    if (YAPI::RegisterHub(hub_url, errmsg) != YAPI_SUCCESS) {
        cerr << TEXT("YAPI::RegisterHub error: ") << errmsg << endl;
        return 1;
    }

    YLightSensor* sensor = YLightSensor::FirstLightSensor();
    if (sensor == NULL) {
        wcout << TEXT("No Yocto-Light connected (check USB cable)") << endl;
        return 1;
    }

    sensor->setReportFrequency("60/m");
    sensor->get_module()->saveToFlash();
    sensor->registerTimedReportCallback(timedCallback);

    while (true) {
        int res = YAPI::Sleep(1000, errmsg);
        if (res != YAPI::SUCCESS) {
            cerr << "YAPI::Sleep error: " << errmsg << endl;
            break;
        }
        res = YAPI::UpdateDeviceList(errmsg);
        if (res != YAPI::SUCCESS) {
            cerr << "YAPI::UpdateDeviceList error: " << errmsg << endl;
            break;
        }
    }
    YAPI::FreeAPI();
    return 0;
}
