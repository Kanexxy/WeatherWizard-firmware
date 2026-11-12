#include "TimeUtils.hpp"

namespace TimeUtils {
    bool initializeTimeFromNTP() {
        if (WiFi.status() != WL_CONNECTED) {
            S_DEBUGF("WiFi not connected, cannot initialize time from NTP.\n");
            return false;
        }
        S_DEBUGF("Initializing time from NTP server %s...", ntpServer);
        configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", ntpServer);
        time_t now = time(nullptr);
        //wait until time is set and is after 01.01.1970 (Unix epoch start)
        while (now < 24 * 3600) { 
            S_DEBUGF(".");
            delay(500);
            now = time(nullptr);
        }
        return true;
    }

    time_t getCurrentTime() {
        return time(nullptr);
    }

    unsigned long getCycleDurationSeconds(){
        return wakeCycleConfig.wakeDurationSeconds + wakeCycleConfig.sleepDurationSeconds;
    }

    bool isInWakeWindow(){
        time_t currentTime = time(nullptr);
        if (currentTime < 24 * 3600) return true;
        if (wakeCycleConfig.wakeDurationSeconds + wakeCycleConfig.sleepDurationSeconds < 2) return true; //always awake if one of the values is 0
        return (currentTime % getCycleDurationSeconds()) <= wakeCycleConfig.wakeDurationSeconds;
    }

    bool isInCooldown(Cooldown_t& cooldown){
        return millis() - cooldown.lastEventTimeMs < cooldown.durationMs;
    }

    void startCooldown(Cooldown_t& cooldown){
        cooldown.lastEventTimeMs = millis();
    }

    void setLocalTimeFromRemote(SyncInfo_t syncInfo){ 
        struct timeval tv = { .tv_sec = syncInfo.remoteNodeTime, .tv_usec = syncInfo.estimatedTimeOnAirUs};
        if (tv.tv_usec >= 1000000) {
            tv.tv_sec += tv.tv_usec / 1000000;
            tv.tv_usec %= 1000000;
        }
        settimeofday(&tv, nullptr);
        S_DEBUGF("Local time adjusted from remote time %ld with estimated ToA %d us.\n\tNew local time: %s", syncInfo.remoteNodeTime, syncInfo.estimatedTimeOnAirUs, ctime(&syncInfo.remoteNodeTime));
    }

    void setLocalTimeFromRemoteAccumulated(SyncInfo_t syncInfo){
        // take current time as accumulated value
        if (accumulatedValuesCount == 0){
            setLocalTimeFromRemote(syncInfo);
            accumulatedValuesCount++;
            return;
        }
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        time_t accumulatedRemoteTime = (tv.tv_sec * accumulatedValuesCount + syncInfo.remoteNodeTime) / (accumulatedValuesCount + 1);
        suseconds_t accumulatedToA = (tv.tv_usec * accumulatedValuesCount + syncInfo.estimatedTimeOnAirUs) / (accumulatedValuesCount + 1);
        accumulatedValuesCount++;
        accumulatedValuesCount %= maxAccumulatedValuesCount;
        SyncInfo_t accumulatedSyncInfo = {accumulatedRemoteTime, accumulatedToA};
        setLocalTimeFromRemote(accumulatedSyncInfo);
    }
}