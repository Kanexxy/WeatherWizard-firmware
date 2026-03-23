#pragma once
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>

#include "debug_helper.hpp"


namespace TimeUtils {

    inline const unsigned long defaultWakeDurationSeconds = 5;
    inline const unsigned long defaultSleepDurationSeconds = 10;
    inline const unsigned long defaultCycleDurationSeconds = defaultWakeDurationSeconds + defaultSleepDurationSeconds;
    inline const uint8_t maxAccumulatedValuesCount = 10;
    // NTP Server settings
    inline const char* ntpServer = "pool.ntp.org";
    inline const long  gmtOffset_sec = 0; // Keep it UTC for easier sync logic
    inline const int   daylightOffset_sec = 0;
    // variable that stores the amount of accumulated values for time sync averaging
    inline uint8_t accumulatedValuesCount = 0;

    typedef struct{
        time_t remoteNodeTime;
        suseconds_t estimatedTimeOnAirUs;
    }SyncInfo_t;

    typedef struct {
        uint8_t wakeDurationSeconds;
        uint16_t sleepDurationSeconds;
    } WakeCycleConfig_t;

    inline WakeCycleConfig_t wakeCycleConfig = {defaultWakeDurationSeconds, defaultSleepDurationSeconds};
    
    time_t getCurrentTime();
    unsigned long getCycleDurationSeconds();
    bool initializeTimeFromNTP();
    bool isInWakeWindow();
    void setLocalTimeFromRemote(SyncInfo_t syncInfo);
    void setLocalTimeFromRemoteAccumulated(SyncInfo_t syncInfo);
}