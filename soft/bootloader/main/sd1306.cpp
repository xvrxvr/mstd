#include <alloca.h>
#include "common.h"
#include "sd1306.h"
#include "pins.h"

static const char TAG[] = "SSD1306";

enum CtrlByte : uint8_t {
    CB_Cmd  = 0x00, // Command follow
    CB_Data = 0x40, // Data follow
    CB_Cont = 0x80  // Set this bit if you want to return to CtrlByte after next data/ctrl byte
};

enum SD1306Cmds : uint8_t {
    CMD_Contrast    = 0x81, // [Contrast byte] - Set contrast
    CMD_DispRAM     = 0xA4, // Display RAM contents
    CMD_DispOn      = 0xA5, // Turn all display On (RAM ignored)
    CMD_Normal      = 0xA6, // Normal display (not inverted)
    CMD_Inverted    = 0xA7, // Inverted display
    CMD_Off         = 0xAE, // Turn display off and go to sleep mode
    CMD_On          = 0xAF, // Turn display on (exit from Sleep mode)
    // CMD_HScroll  26/27
    // CMD_DiagScroll 29/2A
    // CMD_StopScroll = 0x2E
    // CMD_StartScroll = 2F
    // CMD_VertScrollArea = A3
    // CMDP_ContentScrollSetup = 2C/2D
    CMD_LowColStartPM = 0x00, // [| low 4 bit of page address] - Set Lower Column Start Address for Page Addressing Mode
    CMD_HiColStartPM = 0x10, // [| low 4 bit of page address] - Set High Column Start Address for Page Addressing Mode
    CMD_MemMode     = 0x20, // [mode - MemMode] Set Memory Addressing Mode
    CMD_SetColAddr  = 0x21, // [colum-start-address, colum-end-address] Set Column Address (This command is only for horizontal or vertical addressing mode.)
    CMD_SetPageAddr = 0x22, // [page-start-addres, page-end-address] Set Page Address (This command is only for horizontal or vertical addressing mode.)
    CMD_SetPage     = 0xB0, // [|page-address] Set Page Start Address for Page Addressing Mode
    CMD_ChargePump      = 0x8D,  // [ChPumpSeting] - Charge Pump Setting. 

    CMD_Nop         = 0xE3,

    // Hardware Configuration (Panel resolution & layout related) Command Table
    CMDC_StartLine      = 0x40,  // [|start-line-register] Set Display Start Line (0-63)
    CMDC_ColSegRemapDir = 0xA0,  // Set Segment Re-map - column address 0 is mapped to SEG0
    CMDC_ColSegRemapSwp = 0xA1,  // Set Segment Re-map - column address 127 is mapped to SEG0
    CMDC_MuxRatio       = 0xA8,  // [mux-ratio] Set Multiplex Ratio (15-63)
    CMDC_ComScanDir     = 0xC0,  // Set COM Output Scan Direction - Scan from COM0 to COM[N –1]
    CMDC_ComScanSwp     = 0xC8,  // Set COM Output Scan Direction - Scan from COM[N –1] to COM0
    CMDC_DispOffset     = 0xD3,  // [offset] Set Display Offset - Set vertical shift by COM from 0d~63d
    CMDC_ComPinCfg      = 0xDA,  // [cfg - ComPinCfg] Set COM Pins Hardware Configuration   
        // The Charge Pump must be enabled by the following command sequence:
        // 8Dh ; Charge Pump Setting
        // 14h / 94h / 95h ; Enable Charge Pump
        // AFh; Display ON

    // Timing & Driving Scheme Setting Command Table
    CMDC_Freq           = 0xD5,  // [div-ration-and-osc-freq] Set Display Clock Divide Ratio/Oscillator Frequency. Low nibble - Devide ration, High nibble - osc frequency
    CMDC_Precharge      = 0xD9,  // [precharge] Set Pre-charge Period. Low nibble - Phase 1 period of up to 15 DCLK, High nibble - Phase 2 period of up to 15 DCLK. Default - 2. 0 is prohibited
    CMDC_VComLevel      = 0xDB,  // [level - VComLevel] Set VCOMH Deselect Level


    // SSD1315 commands
    CMDP_IRef           = 0xAD,  // [ExtIRef] - Internal IREF Setting
    CMDP_FadeOut        = 0x23, // [FadeOutSet | <blink-time>] - Set Fade Out and Blinking. <blink-time> is 0-15 (8,16,24, ... 128 frames)
    CMDP_Zoom           = 0xD6, // [0/1] - Set Zoom In (The panel must be in alternative COM pin configuration (command DAh A[4] =1))
};

enum MemMode : uint8_t {
    MM_Horizontal   = 0x00, // Horizontal Addressing Mode
    MM_Vertical     = 0x01, // Vertical Addressing Mode
    MM_Page         = 0x02, // Page Addressing Mode
};

enum ComPinCfg : uint8_t {
    CPC_Sequential      = 0x02,  // Sequential COM pin configuration
    CPC_Alternative     = 0x12,  // Alternative COM pin configuration (default)
    CPC_SequentialSwp   = 0x22,  // Sequential + COM Left/Right remapped pin configuration
    CPC_AlternativeSwp  = 0x32,  // Alternative + COM Left/Right remapped pin configuration
};

enum VComLevel : uint8_t {
    VCL_065         = 0x00, // 0.65 x Vcc
    VCLP_071        = 0x10, // 0.71 x Vcc (SSD1315 only)
    VCL_077         = 0x20, // 0.77 x Vcc
    VCL_083         = 0x30  // 0.83 x Vcc
};

// I2C read value (status). Bitset
enum Status : uint8_t {
    S_DispOff = 0x40    // Display is OFF
};

// SSD1315
enum ExtIRef : uint8_t {
    EIR_External    = 0x00, // Select external IREF (RESET)
    EIR_19uA        = 0x20, // Internal IREF setting: 19uA, output a maximum ISEG=150uA (RESET)
    EIR_30uA        = 0x30, // Internal IREF setting: 30uA, output a maximum ISEG=240uA
};

enum ChPumpSeting : uint8_t {
    CPS_Off     = 0x10, // Turn off (Default)
    CPS_75      = 0x14, // 7.5V
    CPSP_85      = 0x94, // 8.5V  <<< SSD1315 only
    CPSP_90      = 0x95  // 9.0V  <<< SSD1315 only
};

enum FadeOutSet : uint8_t {
    FOS_Off     = 0x00, // Turn off Fade Out/Blinking
    FOS_FadeOut = 0x10, // Enable Fade Out mode
    FOS_Blinking = 0x30 // Enable Blinking mode  
};

/* Standard ASCII 5x8 font */
static const uint8_t font5x8 [] = {
  0x00, 0x00, 0x00, 0x00, 0x00, // sp
  0x00, 0x00, 0x2f, 0x00, 0x00, // !
  0x00, 0x07, 0x00, 0x07, 0x00, // "
  0x14, 0x7f, 0x14, 0x7f, 0x14, // #
  0x24, 0x2a, 0x7f, 0x2a, 0x12, // $
  0x62, 0x64, 0x08, 0x13, 0x23, // %
  0x36, 0x49, 0x55, 0x22, 0x50, // &
  0x00, 0x05, 0x03, 0x00, 0x00, // '
  0x00, 0x1c, 0x22, 0x41, 0x00, // (
  0x00, 0x41, 0x22, 0x1c, 0x00, // )
  0x14, 0x08, 0x3E, 0x08, 0x14, // *
  0x08, 0x08, 0x3E, 0x08, 0x08, // +
  0x00, 0x00, 0xA0, 0x60, 0x00, // ,
  0x08, 0x08, 0x08, 0x08, 0x08, // -
  0x00, 0x60, 0x60, 0x00, 0x00, // .
  0x20, 0x10, 0x08, 0x04, 0x02, // /
  0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
  0x00, 0x42, 0x7F, 0x40, 0x00, // 1
  0x42, 0x61, 0x51, 0x49, 0x46, // 2
  0x21, 0x41, 0x45, 0x4B, 0x31, // 3
  0x18, 0x14, 0x12, 0x7F, 0x10, // 4
  0x27, 0x45, 0x45, 0x45, 0x39, // 5
  0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
  0x01, 0x71, 0x09, 0x05, 0x03, // 7
  0x36, 0x49, 0x49, 0x49, 0x36, // 8
  0x06, 0x49, 0x49, 0x29, 0x1E, // 9
  0x00, 0x36, 0x36, 0x00, 0x00, // :
  0x00, 0x56, 0x36, 0x00, 0x00, // ;
  0x08, 0x14, 0x22, 0x41, 0x00, // <
  0x14, 0x14, 0x14, 0x14, 0x14, // =
  0x00, 0x41, 0x22, 0x14, 0x08, // >
  0x02, 0x01, 0x51, 0x09, 0x06, // ?
  0x32, 0x49, 0x59, 0x51, 0x3E, // @
  0x7C, 0x12, 0x11, 0x12, 0x7C, // A
  0x7F, 0x49, 0x49, 0x49, 0x36, // B
  0x3E, 0x41, 0x41, 0x41, 0x22, // C
  0x7F, 0x41, 0x41, 0x22, 0x1C, // D
  0x7F, 0x49, 0x49, 0x49, 0x41, // E
  0x7F, 0x09, 0x09, 0x09, 0x01, // F
  0x3E, 0x41, 0x49, 0x49, 0x7A, // G
  0x7F, 0x08, 0x08, 0x08, 0x7F, // H
  0x00, 0x41, 0x7F, 0x41, 0x00, // I
  0x20, 0x40, 0x41, 0x3F, 0x01, // J
  0x7F, 0x08, 0x14, 0x22, 0x41, // K
  0x7F, 0x40, 0x40, 0x40, 0x40, // L
  0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
  0x7F, 0x04, 0x08, 0x10, 0x7F, // N
  0x3E, 0x41, 0x41, 0x41, 0x3E, // O
  0x7F, 0x09, 0x09, 0x09, 0x06, // P
  0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
  0x7F, 0x09, 0x19, 0x29, 0x46, // R
  0x46, 0x49, 0x49, 0x49, 0x31, // S
  0x01, 0x01, 0x7F, 0x01, 0x01, // T
  0x3F, 0x40, 0x40, 0x40, 0x3F, // U
  0x1F, 0x20, 0x40, 0x20, 0x1F, // V
  0x3F, 0x40, 0x38, 0x40, 0x3F, // W
  0x63, 0x14, 0x08, 0x14, 0x63, // X
  0x07, 0x08, 0x70, 0x08, 0x07, // Y
  0x61, 0x51, 0x49, 0x45, 0x43, // Z
  0x00, 0x7F, 0x41, 0x41, 0x00, // [
  0x55, 0x2A, 0x55, 0x2A, 0x55, //
  0x00, 0x41, 0x41, 0x7F, 0x00, // ]
  0x04, 0x02, 0x01, 0x02, 0x04, // ^
  0x40, 0x40, 0x40, 0x40, 0x40, // _
  0x00, 0x01, 0x02, 0x04, 0x00, // '
  0x20, 0x54, 0x54, 0x54, 0x78, // a
  0x7F, 0x48, 0x44, 0x44, 0x38, // b
  0x38, 0x44, 0x44, 0x44, 0x20, // c
  0x38, 0x44, 0x44, 0x48, 0x7F, // d
  0x38, 0x54, 0x54, 0x54, 0x18, // e
  0x08, 0x7E, 0x09, 0x01, 0x02, // f
  0x18, 0xA4, 0xA4, 0xA4, 0x7C, // g
  0x7F, 0x08, 0x04, 0x04, 0x78, // h
  0x00, 0x44, 0x7D, 0x40, 0x00, // i
  0x40, 0x80, 0x84, 0x7D, 0x00, // j
  0x7F, 0x10, 0x28, 0x44, 0x00, // k
  0x00, 0x41, 0x7F, 0x40, 0x00, // l
  0x7C, 0x04, 0x18, 0x04, 0x78, // m
  0x7C, 0x08, 0x04, 0x04, 0x78, // n
  0x38, 0x44, 0x44, 0x44, 0x38, // o
  0xFC, 0x24, 0x24, 0x24, 0x18, // p
  0x18, 0x24, 0x24, 0x18, 0xFC, // q
  0x7C, 0x08, 0x04, 0x04, 0x08, // r
  0x48, 0x54, 0x54, 0x54, 0x20, // s
  0x04, 0x3F, 0x44, 0x40, 0x20, // t
  0x3C, 0x40, 0x40, 0x20, 0x7C, // u
  0x1C, 0x20, 0x40, 0x20, 0x1C, // v
  0x3C, 0x40, 0x30, 0x40, 0x3C, // w
  0x44, 0x28, 0x10, 0x28, 0x44, // x
  0x1C, 0xA0, 0xA0, 0xA0, 0x7C, // y
  0x44, 0x64, 0x54, 0x4C, 0x44, // z
};


#define I2C_MASTER_TIMEOUT 100

void SSD1306::send_cmd(const std::initializer_list<i2c_master_transmit_multi_buffer_info_t>& args)
{
    if (!dev) return;
    if (ESP_OK != i2c_master_multi_buffer_transmit(dev, (i2c_master_transmit_multi_buffer_info_t*)args.begin(), args.size(), I2C_MASTER_TIMEOUT))
    {
        ESP_LOGE(TAG, "OLED display write failed");
    }
}

#define CMD(array) i2c_master_transmit_multi_buffer_info_t{(uint8_t*)array, sizeof(array)}

void SSD1306::init()
{
    i2c_master_bus_config_t i2c_config = {
        .i2c_port = -1,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7
    };
    i2c_master_bus_handle_t bus_handle;

    if (ESP_OK != i2c_new_master_bus(&i2c_config, &bus_handle))
    {
        ESP_LOGE(TAG, "Can't create I2C bus");
        return;        
    }

    uint16_t address = 0x3C;

    if (ESP_OK != i2c_master_probe(bus_handle, address, I2C_MASTER_TIMEOUT))
    {
        ++address;
        if (ESP_OK != i2c_master_probe(bus_handle, address, I2C_MASTER_TIMEOUT))
        {
            ESP_LOGE(TAG, "OLED display not detected");
            return;
        }
    }
    ESP_LOGI(TAG, "OLED display detected on address 0x%02X", address);

    i2c_device_config_t i2c_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100'000,
    };
    if (ESP_OK != i2c_master_bus_add_device(bus_handle, &i2c_dev_cfg, &dev))
    {
        ESP_LOGE(TAG, "Can't create I2C device for OLED display");
        return;        
    }

    static const uint8_t init_seq[] = {
        CB_Cmd,
        CMD_Off,
        CMDC_Freq, 0x80,
        CMDC_MuxRatio, 0x3F,
        CMDC_DispOffset, 0,
        CMDC_StartLine | 0,
        CMDC_ColSegRemapDir,
        CMDC_ComScanDir,
        CMDC_ComPinCfg, CPC_Alternative,
        CMD_Contrast, 0xCF, // (variable)
        CMDC_Precharge, 0xF1,
        CMDC_VComLevel, VCL_083,
        CMD_DispRAM,
        CMD_Normal,
        CMD_ChargePump, CPS_75, // (variable)
        CMD_MemMode, MM_Page,
    };
    send_cmd({CMD(init_seq)});

    clr_screen();

    static const uint8_t disp_on[] = {CB_Cmd, CMD_On};
    send_cmd({CMD(disp_on)});

    vTaskDelay(ms2ticks(100));
}

void SSD1306::clr_screen(int row_start, int row_end)
{
    while(row_start < row_end) draw_text_imp(row_start++, "");
}

void SSD1306::set_contrast(uint8_t ct)
{
    uint8_t cmd[] = {CB_Cmd, CMD_Contrast, ct};
    send_cmd({CMD(cmd)});
}

void SSD1306::swap_display(bool swp)
{
    static const uint8_t c_norm[] = {CB_Cmd, CMDC_ColSegRemapDir, CMDC_ComScanDir};
    static const uint8_t c_swp[]  = {CB_Cmd, CMDC_ColSegRemapSwp, CMDC_ComScanSwp};
    static_assert(sizeof(c_norm) == sizeof(c_swp));

    send_cmd({i2c_master_transmit_multi_buffer_info_t{(uint8_t*)(swp ? c_swp : c_norm), sizeof(c_norm)}});
}

const char* SSD1306::draw_text_imp(int y, const char* text)
{
    uint8_t cmd[] = {
        CB_Cmd,
        CMD_LowColStartPM|0,
        CMD_HiColStartPM|0,
        uint8_t(CMD_SetPage | (y&7))
    };
    send_cmd({CMD(cmd)});
    const uint8_t zero = 0;
    const uint8_t data = CB_Data;
    int len=0;
    while(*text && *text != '\n')
    {
        uint8_t sym = *text++ - ' ';
        if (len < 21)
        {
            ++len;
            const uint8_t* gr = font5x8+sym*5;
            send_cmd({
                i2c_master_transmit_multi_buffer_info_t{(uint8_t*)&data, 1},
                i2c_master_transmit_multi_buffer_info_t{(uint8_t*)gr, 5},
                i2c_master_transmit_multi_buffer_info_t{(uint8_t*)&zero, 1}
            });
        }
    }

    // Now send 128*(8-row_start) zero bytes of Data
    const size_t total = 129-len*6;
    uint8_t* p = (uint8_t*)alloca(total);
    p[0] = CB_Data;
    memset(p+1, 0, total-1);
    send_cmd({{p, total}});
    return text;
}

void SSD1306::draw_text(int y, const char* text)
{
    for(;;)
    {
        text = draw_text_imp(y, text);
        if (!*text) break;
        ++text;
        ++y &= 7;
    }
}

void SSD1306::setup_pi_row(int row)
{
    if (row >= 8) {clr_screen(); row=0;}
    pi_row = row;
    pi_data = 1;
    pi_count = 0;

    static const uint8_t cmd[] = {
        CB_Cmd,
        CMD_LowColStartPM,
        CMD_HiColStartPM,
        CMD_MemMode, MM_Page,
        CMD_SetPage
    };
    uint8_t pg = CMD_SetPage | pi_row;
    send_cmd({CMD(cmd), i2c_master_transmit_multi_buffer_info_t{&pg, 1}});
}

void SSD1306::inc_pi()
{
    if (pi_count >= 128)
    {
        if (pi_data != 0xFF) {(pi_data <<= 1) |= 1; pi_count=0;}
        else setup_pi_row(pi_row+1);
    }
    uint8_t d[] = {CB_Data, pi_data};
    send_cmd({CMD(d)});
}

SSD1306 oled;
