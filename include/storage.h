#pragma once
#include <stdint.h>
#include "utils.h"

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool lightOn;
    DanTime startTime;
    DanTime endTime;
} StorageData;

const uint32_t INITIALIZED_VALUE = 0x0DADBEEF + sizeof(StorageData);

bool initStorage(StorageData* defaultStorageData, StorageData& storageData);//returns true if memory was already initialized
void commitStorage(StorageData& storageData);