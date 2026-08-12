#define _POSIX_C_SOURCE 200809L
#include "darc_repo.h"
#include "darc_index.h"
#include "darc_snapshot.h"
#include "darc_verify.h"
#include "darc_restore.h"
#include "darc_diff.h"
#include "darc_config.h"

extern darc_config_t *g_darc_config;
static darc_config_t loaded_config;

static int cmd_diff(int argc, char **argv);
static int cmd_restore(int argc, char **argv);
#include "darc_object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

static const char *global_repo = NULL;
static int quiet = 0;
static const char *config_path = NULL;

static void usage(void) {
    fprintf(stderr,
        "darc — Deterministic Deduplicating Archive\n"
        "Usage: darc [global-options] <command> [args]\n"
        "Commands:\n"
        "  init PATH\n"
        "  snapshot create SOURCE... [--name NAME] [--parent SNAP] [--timestamp NS]\n"
        "  snapshot list\n"
        "  snapshot show SNAPSHOT\n"
        "  snapshot delete SNAPSHOT --yes [--dry-run]\n"
        "  verify [--level quick|full|scrub] [--repair]\n"
        "  gc [--dry-run]\n"
        "  index rebuild\n"
        "  repo inspect\n"
        "  --version\n");
}

static int cmd_init(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "E_USAGE: init requires PATH\n"); return 2; }
    if (darc_repo_init(argv[0]) != 0) {
        fprintf(stderr, "E_IO: failed to initialize repository at %s\n", argv[0]);
        return 5;
    }
    if (!quiet) printf("Initialized empty DARC repository at %s\n", argv[0]);
    return 0;
}

static int cmd_snapshot(int argc, char **argv) {
    if (argc < 1) { usage(); return 2; }
    const char *repo_path = global_repo ? global_repo : ".darc";
    if (strcmp(argv[0], "create") == 0) {
        if (argc < 2) { fprintf(stderr, "E_USAGE: snapshot create requires SOURCE\n"); return 2; }
        const char *name = NULL;
        const char *parent_hex = NULL;
        uint64_t ts = 0;
        const char *srcbuf[64];
        size_t nsrc = 0;
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--name") == 0 && i+1 < argc) { name = argv[++i]; }
            else if (strcmp(argv[i], "--parent") == 0 && i+1 < argc) { parent_hex = argv[++i]; }
            else if (strcmp(argv[i], "--timestamp") == 0 && i+1 < argc) { ts = strtoull(argv[++i], NULL, 10); }
            else if (argv[i][0] != '-') {
                if (nsrc < 64) srcbuf[nsrc++] = argv[i];
            }
        }
        if (nsrc == 0) { fprintf(stderr, "E_USAGE: need SOURCE\n"); return 2; }
        if (ts == 0) {
            struct timespec tv;
            clock_gettime(CLOCK_REALTIME, &tv);
            ts = (uint64_t)tv.tv_sec * 1000000000ULL + tv.tv_nsec;
        }
        darc_repo_t *repo = darc_repo_open(repo_path);
        if (!repo) { fprintf(stderr, "E_REPO: cannot open %s\n", repo_path); return 3; }
        darc_index_t *idx = darc_index_load(repo);
        darc_cid_t parent_cid, *pp = NULL;
        if (parent_hex) {
            if (strlen(parent_hex) == 64 && darc_cid_from_hex(parent_hex, parent_cid) == 0) {
                pp = &parent_cid;
            } else {
                darc_snapshot_info_t *list = NULL; size_t n = 0;
                darc_snapshot_list(repo, &list, &n);
                int found = 0;
                for (size_t i = 0; i < n; ++i) {
                    char hex[65]; darc_cid_hex(list[i].cid, hex);
                    if (strncmp(hex, parent_hex, strlen(parent_hex)) == 0) {
                        memcpy(parent_cid, list[i].cid, 32); found = 1; break;
                    }
                }
                free(list);
                if (!found) { fprintf(stderr, "E_NOTFOUND: bad parent\n"); darc_index_free(idx); darc_repo_close(repo); return 4; }
                pp = &parent_cid;
            }
        }
        const char **sources = srcbuf;
        darc_cid_t out;
        int rc = darc_snapshot_create(repo, idx, sources, nsrc, name, pp, ts, out);
        if (rc != 0) {
            fprintf(stderr, "E_IO: snapshot create failed\n");
            darc_index_free(idx); darc_repo_close(repo); return 5;
        }
        /* parity protect chunks */
        darc_parity_protect_all(repo, idx);
        char hex[65];
        darc_cid_hex(out, hex);
        if (!quiet) printf("Created snapshot %s\n", hex);
        darc_index_free(idx);
        darc_repo_close(repo);
        return 0;
    } else if (strcmp(argv[0], "list") == 0) {
        darc_repo_t *repo = darc_repo_open(repo_path);
        if (!repo) { fprintf(stderr, "E_REPO: cannot open %s\n", repo_path); return 3; }
        darc_snapshot_info_t *list = NULL;
        size_t n = 0;
        if (darc_snapshot_list(repo, &list, &n) != 0) {
            darc_repo_close(repo); return 5;
        }
        printf("%-14s %-24s %-12s %6s %10s %10s\n",
               "SNAPSHOT", "CREATED", "PARENT", "FILES", "LOGICAL", "STORED");
        for (size_t i = 0; i < n; ++i) {
            char shortid[13];
            char hex[65];
            darc_cid_hex(list[i].cid, hex);
            memcpy(shortid, hex, 12); shortid[12] = 0;
            char parent[13] = "-";
            if (list[i].has_parent) {
                char ph[65];
                darc_cid_hex(list[i].parent, ph);
                memcpy(parent, ph, 12); parent[12] = 0;
            }
            time_t sec = (time_t)(list[i].created_ns / 1000000000ULL);
            struct tm tm;
            gmtime_r(&sec, &tm);
            char tbuf[32];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%SZ", &tm);
            printf("%-14s %-24s %-12s %6llu %10llu %10llu\n",
                   shortid, tbuf, parent,
                   (unsigned long long)list[i].file_count,
                   (unsigned long long)list[i].logical_bytes,
                   (unsigned long long)list[i].stored_bytes);
        }
        free(list);
        darc_repo_close(repo);
        return 0;
    } else if (strcmp(argv[0], "show") == 0) {
        if (argc < 2) return 2;
        darc_repo_t *repo = darc_repo_open(repo_path);
        if (!repo) return 3;
        darc_cid_t cid;
        /* accept short or full */
        char full[65] = {0};
        if (strlen(argv[1]) == 64) {
            if (darc_cid_from_hex(argv[1], cid) != 0) return 4;
        } else {
            /* resolve short from list */
            darc_snapshot_info_t *list = NULL; size_t n = 0;
            darc_snapshot_list(repo, &list, &n);
            int found = 0;
            for (size_t i = 0; i < n; ++i) {
                char hex[65]; darc_cid_hex(list[i].cid, hex);
                if (strncmp(hex, argv[1], strlen(argv[1])) == 0) {
                    memcpy(cid, list[i].cid, 32);
                    found = 1; break;
                }
            }
            free(list);
            if (!found) { darc_repo_close(repo); return 4; }
        }
        darc_snapshot_info_t info;
        if (darc_snapshot_load_info(repo, cid, &info) != 0) {
            darc_repo_close(repo); return 4;
        }
        char hex[65]; darc_cid_hex(info.cid, hex);
        printf("Snapshot: %s\n", hex);
        if (info.name[0]) printf("Name: %s\n", info.name);
        printf("Created_ns: %llu\n", (unsigned long long)info.created_ns);
        printf("Files: %llu  Dirs: %llu  Symlinks: %llu\n",
               (unsigned long long)info.file_count,
               (unsigned long long)info.dir_count,
               (unsigned long long)info.symlink_count);
        printf("Logical bytes: %llu\n", (unsigned long long)info.logical_bytes);
        printf("New chunks: %llu  Stored: %llu\n",
               (unsigned long long)info.new_chunks,
               (unsigned long long)info.stored_bytes);
        darc_repo_close(repo);
        return 0;
    } else if (strcmp(argv[0], "diff") == 0) {
        return cmd_diff(argc - 1, argv + 1);
    } else if (strcmp(argv[0], "delete") == 0) {
        if (argc < 2) return 2;
        int yes = 0, dry = 0;
        const char *snap = NULL;
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--yes") == 0) yes = 1;
            else if (strcmp(argv[i], "--dry-run") == 0) dry = 1;
            else snap = argv[i];
        }
        if (!snap) return 2;
        if (!dry && !yes) {
            fprintf(stderr, "E_USAGE: --yes required for deletion\n"); return 2;
        }
        darc_repo_t *repo = darc_repo_open(repo_path);
        if (!repo) return 3;
        darc_cid_t cid;
        if (strlen(snap) == 64) {
            if (darc_cid_from_hex(snap, cid) != 0) return 4;
        } else {
            darc_snapshot_info_t *list = NULL; size_t n = 0;
            darc_snapshot_list(repo, &list, &n);
            int found = 0;
            for (size_t i = 0; i < n; ++i) {
                char hex[65]; darc_cid_hex(list[i].cid, hex);
                if (strncmp(hex, snap, strlen(snap)) == 0) {
                    memcpy(cid, list[i].cid, 32); found = 1; break;
                }
            }
            free(list);
            if (!found) { darc_repo_close(repo); return 4; }
        }
        if (dry) {
            char hex[65]; darc_cid_hex(cid, hex);
            printf("Would delete ref %s (objects retained until gc)\n", hex);
        } else {
            darc_repo_delete_snapshot_ref(repo, cid);
            printf("Deleted snapshot ref (objects retained until gc)\n");
        }
        darc_repo_close(repo);
        return 0;
    }
    usage();
    return 2;
}

static int cmd_verify(int argc, char **argv) {
    const char *repo_path = global_repo ? global_repo : ".darc";
    darc_verify_level_t level = DARC_VERIFY_FULL;
    bool repair = false;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--level") == 0 && i+1 < argc) {
            i++;
            if (strcmp(argv[i], "quick") == 0) level = DARC_VERIFY_QUICK;
            else if (strcmp(argv[i], "full") == 0) level = DARC_VERIFY_FULL;
            else if (strcmp(argv[i], "scrub") == 0) level = DARC_VERIFY_SCRUB;
        } else if (strcmp(argv[i], "--repair") == 0) repair = true;
    }
    darc_repo_t *repo = darc_repo_open(repo_path);
    if (!repo) { fprintf(stderr, "E_REPO\n"); return 3; }
    darc_index_t *idx = darc_index_load(repo);
    darc_verify_result_t res;
    int rc = darc_verify(repo, idx, level, repair, &res);
    printf("Checked: %zu  OK: %zu  Corrupt: %zu  Missing: %zu  Repaired: %zu  Unrecoverable: %zu\n",
           res.objects_checked, res.objects_ok, res.objects_corrupt,
           res.objects_missing, res.repaired, res.unrecoverable);
    darc_index_free(idx);
    darc_repo_close(repo);
    return rc;
}

static int cmd_gc(int argc, char **argv) {
    const char *repo_path = global_repo ? global_repo : ".darc";
    bool dry = false;
    for (int i = 0; i < argc; ++i)
        if (strcmp(argv[i], "--dry-run") == 0) dry = true;
    darc_repo_t *repo = darc_repo_open(repo_path);
    if (!repo) return 3;
    darc_index_t *idx = darc_index_load(repo);
    size_t reclaimed = 0;
    int rc = darc_gc(repo, idx, dry, &reclaimed);
    printf("%s reclaimed %zu objects\n", dry ? "Would have" : "Actually", reclaimed);
    darc_index_free(idx);
    darc_repo_close(repo);
    return rc;
}

static int cmd_index(int argc, char **argv) {
    if (argc < 1 || strcmp(argv[0], "rebuild") != 0) return 2;
    const char *repo_path = global_repo ? global_repo : ".darc";
    darc_repo_t *repo = darc_repo_open(repo_path);
    if (!repo) return 3;
    darc_index_t *idx = darc_index_rebuild(repo);
    if (!idx) { darc_repo_close(repo); return 5; }
    printf("Index rebuilt, %zu entries\n", idx->count);
    darc_index_free(idx);
    darc_repo_close(repo);
    return 0;
}

static int cmd_repo_inspect(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *repo_path = global_repo ? global_repo : ".darc";
    darc_repo_t *repo = darc_repo_open(repo_path);
    if (!repo) return 3;
    printf("Repository: %s\n", repo->path);
    printf("Format: 1 (sha256, buzhash64, lzh1, xor8+1)\n");
    darc_cid_t head;
    if (darc_repo_get_head(repo, head) == 0) {
        char hex[65]; darc_cid_hex(head, hex);
        printf("HEAD: %s\n", hex);
    } else {
        printf("HEAD: (empty)\n");
    }
    darc_repo_close(repo);
    return 0;
}


static int cmd_restore(int argc, char **argv) {
    const char *repo_path = global_repo ? global_repo : ".darc";
    const char *snap = NULL, *to = NULL, *pathf = NULL;
    int overwrite = 0;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--to") == 0 && i+1 < argc) to = argv[++i];
        else if (strcmp(argv[i], "--path") == 0 && i+1 < argc) pathf = argv[++i];
        else if (strcmp(argv[i], "--overwrite") == 0 && i+1 < argc) {
            i++;
            if (strcmp(argv[i], "always") == 0) overwrite = 1;
            else if (strcmp(argv[i], "never") == 0) overwrite = 0;
        } else if (argv[i][0] != '-') snap = argv[i];
    }
    if (!snap || !to) { fprintf(stderr, "E_USAGE: restore SNAPSHOT --to PATH\n"); return 2; }
    darc_repo_t *repo = darc_repo_open(repo_path);
    if (!repo) return 3;
    darc_cid_t cid;
    if (strlen(snap) == 64) {
        if (darc_cid_from_hex(snap, cid) != 0) { darc_repo_close(repo); return 4; }
    } else {
        darc_snapshot_info_t *list = NULL; size_t n = 0;
        darc_snapshot_list(repo, &list, &n);
        int found = 0;
        for (size_t i = 0; i < n; ++i) {
            char hex[65]; darc_cid_hex(list[i].cid, hex);
            if (strncmp(hex, snap, strlen(snap)) == 0) { memcpy(cid, list[i].cid, 32); found = 1; break; }
        }
        free(list);
        if (!found) { darc_repo_close(repo); return 4; }
    }
    int rc = darc_restore(repo, cid, to, pathf, overwrite);
    if (rc != 0) { fprintf(stderr, "E_IO: restore failed\n"); darc_repo_close(repo); return 5; }
    if (!quiet) printf("Restored to %s\n", to);
    darc_repo_close(repo);
    return 0;
}

static int cmd_diff(int argc, char **argv) {
    const char *repo_path = global_repo ? global_repo : ".darc";
    const char *old_s = NULL, *new_s = NULL, *fmt = "text", *pathf = NULL;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--format") == 0 && i+1 < argc) fmt = argv[++i];
        else if (strcmp(argv[i], "--path") == 0 && i+1 < argc) pathf = argv[++i];
        else if (argv[i][0] != '-') {
            if (!old_s) old_s = argv[i];
            else if (!new_s) new_s = argv[i];
        }
    }
    if (!old_s || !new_s) { fprintf(stderr, "E_USAGE: snapshot diff OLD NEW\n"); return 2; }
    darc_repo_t *repo = darc_repo_open(repo_path);
    if (!repo) return 3;
    darc_cid_t oc, nc;
    {
        darc_snapshot_info_t *list = NULL; size_t n = 0;
        darc_snapshot_list(repo, &list, &n);
        int fo=0, fn=0;
        for (size_t i = 0; i < n; ++i) {
            char hex[65]; darc_cid_hex(list[i].cid, hex);
            if (!fo && (strlen(old_s)==64 ? strcmp(hex,old_s)==0 : strncmp(hex,old_s,strlen(old_s))==0)) {
                memcpy(oc, list[i].cid, 32); fo=1;
            }
            if (!fn && (strlen(new_s)==64 ? strcmp(hex,new_s)==0 : strncmp(hex,new_s,strlen(new_s))==0)) {
                memcpy(nc, list[i].cid, 32); fn=1;
            }
        }
        free(list);
        if (!fo || !fn) { darc_repo_close(repo); return 4; }
    }
    int rc = darc_snapshot_diff(repo, oc, nc, pathf, fmt);
    darc_repo_close(repo);
    return rc == 0 ? 0 : 5;
}



static int cmd_config(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[0], "validate") != 0) {
        fprintf(stderr, "E_USAGE: config validate FILE\n"); return 2;
    }
    int rc = darc_config_validate_file(argv[1]);
    if (rc == -2) { fprintf(stderr, "E_CONFIG_FORMAT: unsupported extension\n"); return 2; }
    if (rc != 0) { fprintf(stderr, "E_CONFIG: invalid configuration\n"); return 2; }
    printf("OK\n");
    return 0;
}

static int cmd_stats(int argc, char **argv) {
    const char *repo_path = global_repo ? global_repo : ".darc";
    const char *fmt = "text";
    for (int i = 0; i < argc; ++i)
        if (strcmp(argv[i], "--format") == 0 && i+1 < argc) fmt = argv[++i];
    darc_repo_t *repo = darc_repo_open(repo_path);
    if (!repo) return 3;
    darc_index_t *idx = darc_index_load(repo);
    size_t nobj = idx ? idx->count : 0;
    darc_snapshot_info_t *list = NULL; size_t ns = 0;
    darc_snapshot_list(repo, &list, &ns);
    uint64_t logical = 0, stored = 0;
    for (size_t i = 0; i < ns; ++i) {
        logical += list[i].logical_bytes;
        stored += list[i].stored_bytes;
    }
    if (strcmp(fmt, "json") == 0) {
        printf("{\"snapshots\":%zu,\"index_entries\":%zu,\"logical_bytes\":%llu,\"stored_bytes\":%llu}\n",
               ns, nobj, (unsigned long long)logical, (unsigned long long)stored);
    } else if (strcmp(fmt, "ndjson") == 0) {
        printf("{\"type\":\"stats\",\"snapshots\":%zu,\"index_entries\":%zu}\n", ns, nobj);
        for (size_t i = 0; i < ns; ++i) {
            char hex[65]; darc_cid_hex(list[i].cid, hex);
            printf("{\"type\":\"snapshot\",\"id\":\"%s\",\"logical\":%llu,\"stored\":%llu}\n",
                   hex, (unsigned long long)list[i].logical_bytes, (unsigned long long)list[i].stored_bytes);
        }
    } else if (strcmp(fmt, "svg") == 0) {
        /* deterministic simple bar chart */
        printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        printf("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"400\" height=\"%zu\">\n", 40 + ns * 24);
        printf("<text x=\"10\" y=\"20\">DARC stats</text>\n");
        for (size_t i = 0; i < ns; ++i) {
            int w = stored ? (int)(200.0 * list[i].stored_bytes / (stored ? stored : 1)) : 0;
            if (w < 1) w = 1;
            printf("<rect x=\"10\" y=\"%zu\" width=\"%d\" height=\"16\" fill=\"#4a90d9\"/>\n", 30 + i*24, w);
            char hex[13]; char full[65]; darc_cid_hex(list[i].cid, full);
            memcpy(hex, full, 12); hex[12]=0;
            printf("<text x=\"%d\" y=\"%zu\">%s</text>\n", 20+w, 42 + i*24, hex);
        }
        printf("</svg>\n");
    } else {
        printf("Snapshots: %zu\nIndex entries: %zu\nLogical: %llu\nStored: %llu\n",
               ns, nobj, (unsigned long long)logical, (unsigned long long)stored);
    }
    free(list);
    darc_index_free(idx);
    darc_repo_close(repo);
    return 0;
}


int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 2; }
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--repo") == 0 && i+1 < argc) {
            global_repo = argv[++i];
        } else if (strcmp(argv[i], "--config") == 0 && i+1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("darc 0.1.0\n"); return 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(); return 0;
        } else {
            fprintf(stderr, "Unknown option %s\n", argv[i]); return 2;
        }
        i++;
    }
    if (i >= argc) { usage(); return 2; }
    if (config_path) {
        darc_config_defaults(&loaded_config);
        if (darc_config_load(config_path, &loaded_config) != 0) {
            fprintf(stderr, "E_CONFIG: failed to load %s\n", config_path); return 2;
        }
        g_darc_config = &loaded_config;
    }
    const char *cmd = argv[i++];
    int remaining = argc - i;
    char **rest = argv + i;

    if (strcmp(cmd, "init") == 0) return cmd_init(remaining, rest);
    if (strcmp(cmd, "snapshot") == 0) return cmd_snapshot(remaining, rest);
    if (strcmp(cmd, "verify") == 0) return cmd_verify(remaining, rest);
    if (strcmp(cmd, "restore") == 0) return cmd_restore(remaining, rest);
    if (strcmp(cmd, "config") == 0) return cmd_config(remaining, rest);
    if (strcmp(cmd, "stats") == 0) return cmd_stats(remaining, rest);
    if (strcmp(cmd, "gc") == 0) return cmd_gc(remaining, rest);
    if (strcmp(cmd, "index") == 0) return cmd_index(remaining, rest);
    if (strcmp(cmd, "repo") == 0 && remaining >= 1 && strcmp(rest[0], "inspect") == 0)
        return cmd_repo_inspect(remaining - 1, rest + 1);
    usage();
    return 2;
}
