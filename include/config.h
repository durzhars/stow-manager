#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>
#include "utils.h"

typedef struct {
    char config_file_path[PATH_MAX];
    StringArray dotfiles_dirs;
    char target_dir[PATH_MAX];
} Config;

void config_init(Config *cfg);
void config_free(Config *cfg);
void get_config_file_path(char *buf, size_t buf_size);
bool config_load(Config *cfg);
bool config_save(const Config *cfg);

void config_set_dotfiles_dir(const char *path);
void config_add_dotfiles_dir(const char *path);
void config_set_target_dir(const char *path);
void config_show(void);

void get_active_dotfiles_dir(const char *cli_override, char *buf, size_t buf_size);
void get_active_target_dir(const char *cli_override, char *buf, size_t buf_size);

#endif /* CONFIG_H */
