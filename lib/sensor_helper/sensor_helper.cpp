#include "sensor_helper.hpp"

namespace SensorHelper {
    bool initialize() {
        Wire.begin(SDA_PIN, SCL_PIN);
        if (!bme.begin()) {
            S_DEBUGF("Could not find a valid BME680 sensor, check wiring!\n");
            while(1);
            return false;
        }
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150); // 320*C for 150 ms
        S_DEBUGF("BME680 sensor initialized successfully.\n");
        return true;
    }

    bool performReading() {
        if (!bme.performReading()) {
            S_DEBUGF("Failed to perform reading from BME680 sensor!\n");
            return false;
        }
        S_DEBUGF("BME680 Reading:\n");
        S_DEBUGF("\tTemperature: %.2f °C\n", bme.temperature);
        S_DEBUGF("\tPressure: %.2f hPa\n", bme.pressure / 100.0);
        S_DEBUGF("\tHumidity: %.2f %%\n", bme.humidity);
        S_DEBUGF("\tGas Resistance: %.2f kOhms\n", bme.gas_resistance / 1000.0);
        return true;
    }
}