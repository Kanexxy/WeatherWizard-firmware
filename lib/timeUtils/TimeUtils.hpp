#pragma once
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>

#include "debug_helper.hpp"


namespace TimeUtils {

    const unsigned long defaultWakeDurationSeconds = 5;
    const unsigned long defaultSleepDurationSeconds = 10;
    const unsigned long defaultCycleDurationSeconds = defaultWakeDurationSeconds + defaultSleepDurationSeconds;
    const uint8_t maxAccumulatedValuesCount = 10;
    // NTP Server settings
    extern const char* ntpServer;
    extern const long  gmtOffset_sec;    // Keep it UTC for easier sync logic
    extern const int   daylightOffset_sec;
    // variable that stores the amount of accumulated values for time sync averaging
    extern uint8_t accumulatedValuesCount;

    typedef struct{
        time_t remoteNodeTime;
        suseconds_t estimatedTimeOnAirUs;
    }SyncInfo_t;

    typedef struct {
        uint8_t wakeDurationSeconds;
        uint16_t sleepDurationSeconds;
    } WakeCycleConfig_t;

    extern WakeCycleConfig_t wakeCycleConfig;
    
    time_t getCurrentTime();
    unsigned long getCycleDurationSeconds();
    bool initializeTimeFromNTP();
    bool isInWakeWindow();
    void setLocalTimeFromRemote(SyncInfo_t syncInfo);
    void setLocalTimeFromRemoteAccumulated(SyncInfo_t syncInfo);
}