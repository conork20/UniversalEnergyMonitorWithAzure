// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "src/iotc/iotc.h"
#include "src/iotc/common/string_buffer.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ArduinoJson.h> // Need to install ArduinoJson >= 6.9 in Arduino Library

#define WIFI_SSID <WIFI SSID> 
#define WIFI_PASSWORD <WIFI PASSWORD>

const char* scopeId = <IOT CENTRAL SCOPE ID>;
const char* deviceId = <IOT CENTRAL DEVICE ID>;
const char* deviceKey = <IOT CENTRAL DEVICE KEY>;

#define CONFIG_MAGIC 8482723

typedef struct deviceConfig_ {
    int magic;
    double lineVoltage;
    double KWhMeter;
    double configScaleFactor;
    double zeroPoint;
} deviceConfig;

static deviceConfig config;

void read_config_from_flash(){
    EEPROM.get<deviceConfig>(0, config);
    if (!(config.magic == CONFIG_MAGIC))
    {
        // Invalid config, use defaults.
        config.magic = CONFIG_MAGIC;
        config.lineVoltage = 115.0;
        config.KWhMeter = 0.0;
        config.configScaleFactor = 1.0;
        config.zeroPoint = 0.0;
        LOG_VERBOSE("Using default config.");
    }
    LOG_VERBOSE("CONFIG: \n   Line Voltage: %f\n   KWH: %f\n   Calibration: %f\n   ZeroPoint: %f", config.lineVoltage, config.KWhMeter, config.configScaleFactor, config.zeroPoint);
}

void write_config_to_flash(){
    EEPROM.put<deviceConfig>(0, config);
    EEPROM.commit();
}

static IOTContext context = NULL;

// ADC Buffer
#define ADC_BUFF_SIZE 10
static double adc_samples[ADC_BUFF_SIZE];
static short adc_pointer = 0;
short adc_pointer_increment(){ return adc_pointer = (++adc_pointer > ADC_BUFF_SIZE - 1) ? 0 : adc_pointer; }

double adc_buffer_max_val(){
    double m = 0;
    for (short i = 0; i < ADC_BUFF_SIZE; i++)
        m = max(m, adc_samples[i]);
    return m;
}


void connect_wifi() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    LOG_VERBOSE("Connecting WiFi..");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
}

static bool isConnected = false;

void doCalibration(double expectedWatts)
{
    for (int i = 0; i < ADC_BUFF_SIZE; i++)
    {
        adc_samples[adc_pointer_increment()] = analogRead(A0);
    }
    double peakAmpsMeasured = adc_buffer_max_val();
    double peakWattsMeasured = (peakAmpsMeasured) * config.lineVoltage;
    config.configScaleFactor = expectedWatts / peakWattsMeasured; 
    write_config_to_flash();
    LOG_VERBOSE("ExpectedWatts: %f, MeasuredWatts: %f, Setting Scale Factor to: %f", expectedWatts, peakWattsMeasured, config.configScaleFactor);
}

void doZeroPointCalibration()
{
    for (int i = 0; i < ADC_BUFF_SIZE; i++)
    {
        adc_samples[adc_pointer_increment()] = analogRead(A0);
    }

    double peakSample = adc_buffer_max_val();
    config.zeroPoint = peakSample;
    write_config_to_flash();
    LOG_VERBOSE("Setting ZeroPoint to %f", config.zeroPoint);
}

void onCommand(IOTContext ctx, IOTCallbackInfo *callbackInfo)
{
    if (strcmp(callbackInfo->tag, "clear_kwh") == 0)
    {
        config.KWhMeter = 0.0;
        LOG_VERBOSE("Clearing KWH Meter.");
    }
    else if (strcmp(callbackInfo->tag, "calibration") == 0)
    {
        StaticJsonDocument<512 /* Bytes */> doc;
        if (!deserializeJson(doc, callbackInfo->payload))
        {
            doCalibration(doc["calibration_watts"]);
        }
        else
        {
            LOG_ERROR("Error parsing calibration.");
        }
    }
    else if (strcmp(callbackInfo->tag, "set_zero") == 0)
    {
        doZeroPointCalibration();
    }
    else
    {
        LOG_VERBOSE("Unexpected command received: %s", callbackInfo->tag);
    }
}

void onSettingsUpdate(IOTContext ctx, IOTCallbackInfo *callbackInfo) {
    if (strcmp(callbackInfo->tag, "voltage") == 0)
    {
        StaticJsonDocument<512 /* Bytes */> doc;
        
        if (!deserializeJson(doc, callbackInfo->payload))
        {
            config.lineVoltage = doc["voltage"]["value"];
            write_config_to_flash();
            LOG_VERBOSE("Setting voltage to %f", config.lineVoltage);
        }
        else
        {
            LOG_ERROR("Could not deserialize setting update %s.", callbackInfo->tag);
        }
    }
    else
    {
        {
            LOG_ERROR("Unknown setting %s", callbackInfo->tag);
        }
    }
    
}

void onEvent(IOTContext ctx, IOTCallbackInfo *callbackInfo) {
    if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
        LOG_VERBOSE("Is connected ? %s (%d)", callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO", callbackInfo->statusCode);
        isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    }

    AzureIOT::StringBuffer buffer;
    if (callbackInfo->payloadLength > 0) {
        buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
    }
    LOG_VERBOSE("- [%s] event was received. Payload => %s", callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");
}

static unsigned prevMillis = 0;
static unsigned minuteCounter = 0;
static unsigned hourCounter = 0;

void setup()
{
    connect_wifi();

    // Azure IOT Central setup
    int errorCode = iotc_init_context(&context);
    if (errorCode != 0) {
        LOG_ERROR("Error initializing IOTC. Code %d", errorCode);
        return;
    }

    iotc_set_logging(IOTC_LOGGING_API_ONLY);

    // for the simplicity of this sample, used same callback for all the events below
    iotc_on(context, "MessageSent", onEvent, NULL);
    iotc_on(context, "Command", onCommand, NULL);
    iotc_on(context, "ConnectionStatus", onEvent, NULL);
    iotc_on(context, "SettingsUpdated", onSettingsUpdate, NULL);
    iotc_on(context, "Error", onEvent, NULL);
    errorCode = iotc_connect(context, scopeId, deviceKey, deviceId, IOTC_CONNECT_SYMM_KEY);
    if (errorCode != 0) {
        LOG_ERROR("Error @ iotc_connect. Code %d", errorCode);
        return;
    }
    prevMillis = millis();

    // Read saved device state from flash
    EEPROM.begin(512);
    delay(1000);
    read_config_from_flash();
}

void loop()
{
    if (isConnected) {
        unsigned long ms = millis();
        // Send telemetry every 10 seconds
        if (abs(ms - prevMillis) > 10000) {
            char msg[64] = {0};
            int pos = 0, errorCode = 0;

            prevMillis = ms;
            pos = snprintf(msg, sizeof(msg) - 1, "{\"power\": %f}", ((adc_buffer_max_val() - config.zeroPoint)) * (double) config.lineVoltage * config.configScaleFactor);
            errorCode = iotc_send_telemetry(context, msg, pos);

            if (errorCode != 0) {
                LOG_ERROR("Sending message has failed with error code %d", errorCode);
            }

            pos = snprintf(msg, sizeof(msg) - 1, "{\"kwh\": %f}", config.KWhMeter);
            errorCode = iotc_send_telemetry(context, msg, pos);

            if (errorCode != 0) {
                LOG_ERROR("Sending message has failed with error code %d", errorCode);
            }
        }

        iotc_do_work(context); // do background work for iotc

        // Update KWH every minute. 
        if (abs(ms - minuteCounter) > 60000)
        {
            minuteCounter = ms;
            config.KWhMeter += (((adc_buffer_max_val() - config.zeroPoint)) * (double) config.lineVoltage) * config.configScaleFactor / 1000 / 60.0;
        }

        // Write KWH Meter to flash every hour.
        if (abs(ms - hourCounter) > 3600000)
        {
            hourCounter = ms;
            write_config_to_flash(); // Makes KWh meter persistent.
        }

        // Sample ADC
        adc_samples[adc_pointer_increment()] = analogRead(A0);
        delay(4); // Bug in ESP8266 - sampling ADC too fast causes WIFI disconnect.
    }
}