#pragma once

enum class ClusterId : uint16_t {
    genBasic = 0,
    genOnOff = 6,
    ssIasZone = 1280,
};

enum class GenBasicAttrId : uint16_t {
    zclVersion = 0,
    appVersion = 1,
    stackVersion = 2,
    hwVersion = 3,
    manufacturerName = 4,
    modelId = 5,
    dateCode = 6,
    powerSource = 7,
    appProfileVersion = 8,
    swBuildId = 16384,
    locationDesc = 16,
    physicalEnv = 17,
    deviceEnabled = 18,
    alarmMask = 19,
    disableLocalConfig = 20
};