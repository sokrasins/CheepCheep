#include "fs.h"
#include "esp_littlefs.h"
#include "log.h"

#include <stdio.h>
#include <sys/stat.h>

#define FS_BASE_PATH       "/fs"
#define FS_PARTITION_LABEL "storage"

status_t fs_init(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path              = FS_BASE_PATH,
        .partition_label        = FS_PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount             = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) 
    {
        if (ret == ESP_FAIL) 
        {
            ERROR("Failed to mount or format filesystem");
        } 
        else if (ret == ESP_ERR_NOT_FOUND) 
        {
            ERROR("Failed to find LittleFS partition");
        } 
        else 
        {
            ERROR("Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return -STATUS_IO;
    }

    size_t total = 0; 
    size_t used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) 
    {
        ERROR("Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        esp_littlefs_format(conf.partition_label);
    } 
    else 
    {
        INFO("Partition size: total: %d, used: %d", total, used);
    }

    return STATUS_OK;
}

file_t fs_open(const char *name, const char *type)
{
    char path[64];
    int rc = snprintf(path, 64, "%s/%s", FS_BASE_PATH, name);
    if (rc > 64 || rc < 0)
    {
        ERROR("Error opening file: %d", rc);
        return NULL;
    }
    
    return (file_t) fopen(path, type);
}

status_t fs_read(file_t file, char *data, size_t chars)
{
    char *ret = fgets(data, chars, (FILE *) file);
    if (ret != data)
    {
        ERROR("Error reading file");
        return -STATUS_IO;
    }

    return STATUS_OK;
}

status_t fs_readuntil(file_t file, char *data, size_t data_bytes, char limit)
{
    assert(file);
    assert(data);

    char c;
    size_t bytes = 0;
    do {
        if (bytes >= data_bytes)
        {
            return -STATUS_NOMEM;
        }

        c = fgetc(file);
        if (c == (char)255)
        {
            return -STATUS_EOF;
        }

        data[bytes] = c;
        bytes++;
    } while (c != limit);

    return STATUS_OK;
}

status_t fs_write_str(file_t file, char *data)
{
    assert(file);
    assert(data);

    int rc = fputs(data, (FILE *) file);
    if (rc < 0)
    {
        ERROR("Error writing to file: %d", rc);
        return -STATUS_IO;
    }

    return STATUS_OK;
}

void fs_rewind(file_t file)
{
    assert(file);
    rewind(file);
}

status_t fs_close(file_t file)
{
    assert(file);

    int rc = fclose((FILE *) file);
    if (rc != 0)
    {
        ERROR("Error closing file: %d", rc);
        return -STATUS_IO;
    }

    return STATUS_OK;
}

status_t fs_rm(const char *name)
{
    assert(name);

    char path[64];
    int rc = 0;

    rc = snprintf(path, 64, "%s/%s", FS_BASE_PATH, name);
    if (rc > 64 || rc < 0)
    {
        ERROR("Error appending path: %d bytes required", rc);
        return -STATUS_NOMEM;
    }

    rc = remove(path);
    if (rc != 0)
    {
        ERROR("Error deleting file: %d", rc);
        return -STATUS_IO;
    }

    return STATUS_OK;
}

bool fs_exists(const char *name)
{
    assert(name);
    
    int rc = 0;
    struct stat st;
    char path[64];
    
    rc = sprintf(path, "%s/%s", FS_BASE_PATH, name);
    if (rc > 64 || rc < 0)
    {
        ERROR("Error appending path: %d bytes required", rc);
        return -STATUS_NOMEM;
    }

    return stat(path, &st) == 0;
}