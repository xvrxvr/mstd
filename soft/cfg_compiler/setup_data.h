#pragma once

static constexpr size_t MAX_CFG_SIZE = 4096;

enum Options1 : uint8_t {
    // Wifi options
    WFOP_No   = 0x00,  // WiFi turned off
    WFOP_AP   = 0x01,  // WiFi in AccessPoint mode
    WFOP_Sta  = 0x02,  // WiFi in Station mode
    WFOP_Both = 0x03,  // WiFi in AP + Sta mode
    WFOP_Auto = 0x04,  // If SSID & Passwd set - run Station mode, else AP mode
    // 0x05, 0x06, 0x07 - reserved
    WFOP_MASK = 0x07,

    Nxt_Options = 0x08,
    // 0x10
    // 0x20
    // 0x40
    // 0x80
};

static constexpr uint8_t ConfigVersion = 1;
static constexpr uint8_t LC_ConfigVersion = 1;  // Last compatible config version. All version from LC_ConfigVersion and to ConfigVersion can be read in current config.

struct Config_V1 {
    uint32_t crc;   // crc from next field to end of config record
    uint16_t size;  // = sizeof(Config_V1)/4 - 1. Maximum is 0x3FF. Bits 0xFC00 reserved (should be 0)
    uint8_t version;
    char ssid[33] = {0};
    char passwd[64] = {0};
    uint8_t oled_contrast = 0xCF;
    Options1 options1 = WFOP_Auto;
    uint16_t reserved = 0;
};

void load_config();

extern Config_V0* config;
extern size_t cfg_size; // Current config starts from 'config' and span 'cfg_size' bytes
extern uint8_t cfg_full[MAX_CFG_SIZE]; // Image of config partiton
extern bool cfg_full_loaded; // Set to true if partititon config was loaded

// Save config image, return NULL
// On error return error string
// 'image' can be mofified - if CRC field contains -1 it will bne replaced by real crc
const char* save_config_image(void* image, size_t size);

// Write whole partition with config
const char* save_config_full(uint8_t image[MAX_CFG_SIZE]);
