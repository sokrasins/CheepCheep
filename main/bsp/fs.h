#ifndef STORAGE_H_
#define STORAGE_H_

#include "status.h"
#include <stddef.h>
#include <stdbool.h>

typedef void *file_t;

status_t fs_init(void);

file_t fs_open(const char *name, const char *type);

status_t fs_read(file_t file, char *data, size_t chars);

status_t fs_readuntil(file_t file, char *data, size_t data_bytes, char limit);

#define fs_readline(file, data, data_size) fs_readuntil(file, data, data_size, '\n')

status_t fs_write_str(file_t file, char *data);

void fs_rewind(file_t file);

status_t fs_close(file_t file);

status_t fs_rm(const char *name);

bool fs_exists(const char *name);

#endif /*STORAGE_H_*/