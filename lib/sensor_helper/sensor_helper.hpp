#include <Adafruit_BME680.h>
#include "debug_helper.hpp"

#if defined(HELTEC_V3_USED)
    #define SDA_PIN 7
    #define SCL_PIN 6
#else
    #define SDA_PIN 16
    #define SCL_PIN 17
#endif

namespace SensorHelper {
    inline Adafruit_BME680 bme;

    bool initialize();
    bool performReading();
}