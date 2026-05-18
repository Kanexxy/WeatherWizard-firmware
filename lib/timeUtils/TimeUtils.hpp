#pragma once
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>

#include "debug_helper.hpp"


namespace TimeUtils {

    inline const unsigned int defaultWakeDurationSeconds = 5;
    inline const unsigned int defaultSleepDurationSeconds = 10;
    inline const unsigned long defaultSendCooldownMilliseconds = 500;
    inline const unsigned long defaultCollisionCooldownMilliseconds = 300;
    inline const unsigned int defaultCycleDurationSeconds = defaultWakeDurationSeconds + defaultSleepDurationSeconds;

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

    typedef struct{
        unsigned long durationMs;
        unsigned long lastEventTimeMs;
    }Cooldown_t;

    inline WakeCycleConfig_t wakeCycleConfig = {defaultWakeDurationSeconds, defaultSleepDurationSeconds};
    inline Cooldown_t sendCooldown = {defaultSendCooldownMilliseconds, 0};
    inline Cooldown_t collisionCooldown = {defaultCollisionCooldownMilliseconds, 0};
    
    time_t getCurrentTime();
    unsigned long getCycleDurationSeconds();
    bool initializeTimeFromNTP();
    bool isInWakeWindow();

    bool isInCooldown(Cooldown_t& cooldown);
    void startCooldown(Cooldown_t& cooldown);
    
    void setLocalTimeFromRemote(SyncInfo_t syncInfo);
    void setLocalTimeFromRemoteAccumulated(SyncInfo_t syncInfo);
}