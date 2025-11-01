/*
    MIT License

    Copyright (c) 2018, Alexey Dynda

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

/*
Modified for MSTD+ project by XVR-Product
*/

#include "tftp_ota_server.h"
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <string.h>

#include "../../common.h"
#include "../../hardware.h"
#include "../../setup_data.h"

const char TAG[] = "OTA";

static uint8_t ota_cfg_buffer[MAX_CFG_SIZE];

TftpOtaServer::Job TftpOtaServer::test_file_name(const char* fname, bool is_write)
{
    cur_ptr = 0;
    if (strcmp(fname, "full.cfg") == 0) return is_write ? J_LoadFCfg : J_SendFCfg;
    const char* p = strrchr(fname, '.');
    if (!p) return J_None;
    if (strcmp(p, ".bin")==0) return is_write ? J_LoadFW : J_None;
    if (strcmp(p, ".cfg")==0) return is_write ? J_LoadCfg : J_SendCfg;
    return J_None;
}


int TftpOtaServer::on_write(const char *file)
{
    job = test_file_name(file, true);
    memset(ota_cfg_buffer, 0xFF, sizeof(ota_cfg_buffer));
    switch(job)
    {
        case J_None:
            {
                ESP_LOGW(TAG, "Unknown file name '%s'", file);
                msg("OTA Error:\nWrong file name");
                return -1;
            }
        case J_LoadFW:
            {
                const esp_partition_t* active_partition = esp_ota_get_running_partition();
                m_next_partition = esp_ota_get_next_update_partition(active_partition);
                if (!m_next_partition)
                {
                    ESP_LOGE(TAG, "failed to prepare partition");
                    msg("OTA Error:\nFail to prep part");
                    return -1;
                }
                esp_err_t err = esp_ota_begin(m_next_partition, OTA_SIZE_UNKNOWN, &m_ota_handle);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "failed to prepare partition 2");
                    msg("OTA Error:\nFail to start part wr");
                    return -1;
                }
                msg("Loading firmware");
                break;
            }
        case J_LoadCfg:
            msg("Loading config");
            break;
        case J_LoadFCfg:
            if (!cfg_full_loaded)
            {
                ESP_LOGE(TAG, "Config partition doesn't exist");
                msg("OTA Error:\nNo cfg part to send");
                return -1;
            }
            msg("Loading cfg partition");
            break;
        default: 
            return -1;
    }
    oled.start_pi(OLED_MSG_PI);
    return 0;
}

int TftpOtaServer::on_read(const char *file)
{
    job = test_file_name(file, false);
    switch(job)
    {
        case J_None:
            {
                ESP_LOGW(TAG, "Unknown file name '%s'", file);
                msg("OTA Error:\nWrong file name");
                return -1;
            }
        case J_SendCfg:
            if (!config)
            {
                ESP_LOGE(TAG, "Config doesn't exist");
                msg("OTA Error:\nNo config to send");
                return -1;
            }
            msg("Sending config");
            break;
        case J_SendFCfg:
            if (!cfg_full_loaded)
            {
                ESP_LOGE(TAG, "Config partition doesn't exist");
                msg("OTA Error:\nNo cfg part to send");
                return -1;
            }
            msg("Sending cfg partition");
            break;
        default: return -1;
    }
    oled.start_pi(OLED_MSG_PI);
    return 0;
}

int TftpOtaServer::on_write_data(uint8_t *buffer, int len)
{
    switch(job)
    {
        case J_LoadFW:
            if (!m_ota_handle) return -1;
            if ( esp_ota_write(m_ota_handle, buffer, len) != ESP_OK)
            {
                ESP_LOGE(TAG, "failed to write to partition");
                msg("OTA Error:\nFail to write to part");
                return -1;
            }
            break;
        case J_LoadCfg: case J_LoadFCfg:
            if (len + cur_ptr <= MAX_CFG_SIZE) memcpy(ota_cfg_buffer+cur_ptr, buffer, len); else
            {
                ESP_LOGE(TAG, "Config buffer overflow");
                msg("OTA Error:\nCfg buffer overflow");
                return -1;
            }
            break;
        default: return -1;
    }
    inc_cur_ptr(len);
    return len;
}

int TftpOtaServer::on_read_data(uint8_t *buffer, int len)
{
    switch(job)
    {
        case J_SendCfg:
            if (!config) return -1;
            {
                int rest = cfg_size - cur_ptr;
                if (len > rest) len = rest;
                memcpy(buffer, cur_ptr + (uint8_t*)config, len);
            }
            break;
        case J_SendFCfg:
            if (!cfg_full_loaded) return -1;
            {
                int rest = MAX_CFG_SIZE - cur_ptr;
                if (len > rest) len = rest;
                memcpy(buffer, cur_ptr + cfg_full, len);
            }
            break;
        default: return -1;
    }
    inc_cur_ptr(len);
    return len;
}


void TftpOtaServer::on_close()
{
    const char* err = NULL;

    switch(job)
    {
        case J_LoadFW:
            if (m_ota_handle)
            {
                if ( esp_ota_end(m_ota_handle) == ESP_OK )
                {
                    ESP_LOGI(TAG, "Upgrade successful");
                    esp_ota_set_boot_partition(m_next_partition);
                    m_ota_handle = 0;
                    fflush(stdout);
                    msg("Update successful\nReboot in 5 second");
                    //esp_restart();
                    reboot();
                    for(;;);
                }
                err = "Fail img verification";
            }
            break;
        case J_LoadCfg:
            err = save_config_image(ota_cfg_buffer, cur_ptr);
            break;
        case J_LoadFCfg:
            err = save_config_full(ota_cfg_buffer);
            break;
        default: break;
    }
    if (err)
    {
        ESP_LOGE(TAG, "%s", err);
        strcpy((char*)ota_cfg_buffer, "OTA Error:\n");
        strcat((char*)ota_cfg_buffer, err);
        msg((char*)ota_cfg_buffer);
    }
}

// Add 'increment' to cur_ptr and update PI
void TftpOtaServer::inc_cur_ptr(size_t increment)
{
    size_t total_size = 0;
    switch(job)
    {
        case J_LoadFW:                                    total_size = 1024*1024*4; break; // 4M partition size
        case J_LoadCfg: case J_LoadFCfg: case J_SendFCfg: total_size = MAX_CFG_SIZE; break;
        case J_SendCfg:                                   total_size = cfg_size; break;
        default: break;
    }
    if (!total_size) {cur_ptr += increment; return;}
    int before = cur_ptr * TOTAL_PI / total_size;
    cur_ptr += increment;
    int after = cur_ptr * TOTAL_PI / total_size;
    while(before ++ < after) oled.inc_pi();
}

