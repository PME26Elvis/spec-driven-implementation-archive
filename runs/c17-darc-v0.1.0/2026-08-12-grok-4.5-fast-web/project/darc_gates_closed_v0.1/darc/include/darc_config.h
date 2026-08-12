#ifndef DARC_CONFIG_H
#define DARC_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    /* chunking */
    uint64_t chunk_min;
    uint64_t chunk_avg;
    uint64_t chunk_max;
    /* compression */
    uint64_t min_savings_bytes;
    bool compression_enabled;
    /* parity */
    bool parity_enabled;
    int parity_data_members; /* 8 */
    /* scan */
    char **include_globs;
    size_t n_include;
    char **exclude_globs;
    size_t n_exclude;
    /* output */
    char format[16]; /* text|json|ndjson|svg */
    bool quiet;
    bool verbose;
    /* raw hash of normalized config */
    uint8_t config_hash[32];
    uint8_t profile_hash[32];
} darc_config_t;

void darc_config_defaults(darc_config_t *c);
void darc_config_free(darc_config_t *c);
int darc_config_load_json(const char *path, darc_config_t *c);
int darc_config_load_yaml(const char *path, darc_config_t *c);
int darc_config_load(const char *path, darc_config_t *c); /* by extension */
int darc_config_validate_file(const char *path); /* 0 ok */
void darc_config_compute_hashes(darc_config_t *c);

#endif
