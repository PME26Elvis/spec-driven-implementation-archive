#define _POSIX_C_SOURCE 200809L
#include "darc_verify.h"
#include "darc_util.h"
#include "darc_lzh1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

/* XOR parity: group of 8 data chunks + 1 parity chunk.
   Parity[i] = data0[i] XOR data1[i] XOR ... (pad shorter with 0) */

int darc_parity_protect_chunk(darc_repo_t *repo, const darc_cid_t *cids, size_t n,
                              darc_cid_t parity_out) {
    if (n == 0 || n > 8) return -1;
    uint8_t *data[8] = {0};
    size_t lens[8] = {0};
    size_t max_len = 0;
    uint8_t types[8];
    for (size_t i = 0; i < n; ++i) {
        if (darc_repo_get_object(repo, cids[i], &types[i], &data[i], &lens[i]) != 0)
            goto fail;
        if (lens[i] > max_len) max_len = lens[i];
    }
    uint8_t *parity = calloc(1, max_len);
    if (!parity) goto fail;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < lens[i]; ++j)
            parity[j] ^= data[i][j];
    }
    /* PARITY payload: ver, n, cids[n], lengths[n] u64, max_len, data */
    size_t plen = 2 + 8 + n * 32 + n * 8 + 8 + max_len;
    uint8_t *payload = malloc(plen);
    if (!payload) { free(parity); goto fail; }
    size_t off = 0;
    darc_write_u16_le(payload + off, 1); off += 2;
    darc_write_u64_le(payload + off, n); off += 8;
    for (size_t i = 0; i < n; ++i) {
        memcpy(payload + off, cids[i], 32); off += 32;
    }
    for (size_t i = 0; i < n; ++i) {
        darc_write_u64_le(payload + off, lens[i]); off += 8;
    }
    darc_write_u64_le(payload + off, max_len); off += 8;
    memcpy(payload + off, parity, max_len); off += max_len;
    free(parity);
    darc_cid_compute(DARC_TYPE_PARITY, 1, payload, off, parity_out);
    if (!darc_repo_has_object(repo, parity_out)) {
        darc_repo_put_object(repo, DARC_TYPE_PARITY, DARC_CODEC_RAW,
                             payload, off, payload, off, parity_out);
    }
    /* append stripe to parity/CATALOG: parity_hex member0 member1 ... */
    {
        char line[2048];
        char phex[65];
        darc_cid_hex(parity_out, phex);
        int pos = snprintf(line, sizeof(line), "%s", phex);
        for (size_t i = 0; i < n && pos < (int)sizeof(line) - 70; ++i) {
            char mhex[65];
            darc_cid_hex(cids[i], mhex);
            pos += snprintf(line + pos, sizeof(line) - (size_t)pos, " %s", mhex);
        }
        line[pos++] = '\n'; line[pos] = 0;
        char catpath[4096];
        snprintf(catpath, sizeof(catpath), "%s/parity/CATALOG", repo->path);
        FILE *cf = fopen(catpath, "a");
        if (cf) { fputs(line, cf); fclose(cf); }
    }
    free(payload);
    for (size_t i = 0; i < n; ++i) free(data[i]);
    return 0;
fail:
    for (size_t i = 0; i < n; ++i) free(data[i]);
    return -1;
}

int darc_parity_recover(darc_repo_t *repo, const darc_cid_t missing,
                        const darc_cid_t *stripe, size_t stripe_n,
                        darc_cid_t recovered) {
    /* stripe includes the parity CID as last, data CIDs first */
    if (stripe_n < 2) return -1;
    /* Find which is missing among data members; parity is last */
    size_t n_data = stripe_n - 1;
    const darc_cid_t *parity_cid = &stripe[n_data];
    uint8_t *parity_payload = NULL;
    size_t pp_len = 0;
    uint8_t ptype;
    if (darc_repo_get_object(repo, *parity_cid, &ptype, &parity_payload, &pp_len) != 0 ||
        ptype != DARC_TYPE_PARITY)
        return -1;
    /* parse parity */
    size_t off = 2;
    uint64_t nmem = darc_read_u64_le(parity_payload + off); off += 8;
    if (nmem != n_data) { free(parity_payload); return -1; }
    off += nmem * 32; /* skip member cids */
    uint64_t *mlens = calloc(nmem, sizeof(uint64_t));
    if (!mlens) { free(parity_payload); return -1; }
    for (uint64_t i = 0; i < nmem; ++i) {
        mlens[i] = darc_read_u64_le(parity_payload + off); off += 8;
    }
    uint64_t plen = darc_read_u64_le(parity_payload + off); off += 8;
    const uint8_t *pdata = parity_payload + off;

    uint8_t *acc = calloc(1, (size_t)plen);
    if (!acc) { free(mlens); free(parity_payload); return -1; }
    memcpy(acc, pdata, (size_t)plen);

    int missing_idx = -1;
    for (size_t i = 0; i < n_data; ++i) {
        if (memcmp(stripe[i], missing, 32) == 0) {
            missing_idx = (int)i;
            continue;
        }
        uint8_t *d = NULL; size_t dl = 0; uint8_t tt;
        if (darc_repo_get_object(repo, stripe[i], &tt, &d, &dl) != 0) {
            free(acc); free(mlens); free(parity_payload); return -1;
        }
        for (size_t j = 0; j < dl && j < plen; ++j)
            acc[j] ^= d[j];
        free(d);
    }
    free(parity_payload);
    if (missing_idx < 0) { free(acc); free(mlens); return -1; }

    size_t rec_len = (size_t)mlens[missing_idx];
    free(mlens);
    if (rec_len > plen) { free(acc); return -1; }

    darc_cid_t got;
    darc_cid_compute(DARC_TYPE_CHUNK, 1, acc, rec_len, got);
    if (memcmp(got, missing, 32) != 0) {
        free(acc);
        return -1;
    }
    darc_repo_put_object(repo, DARC_TYPE_CHUNK, DARC_CODEC_RAW, acc, rec_len, acc, rec_len, got);
    memcpy(recovered, got, 32);
    free(acc);
    return 0;
}

static int try_parity_repair(darc_repo_t *repo, const darc_cid_t missing, darc_verify_result_t *res) {
    char catpath[4096];
    snprintf(catpath, sizeof(catpath), "%s/parity/CATALOG", repo->path);
    FILE *cf = fopen(catpath, "r");
    if (!cf) return -1;
    char line[2048];
    char mhex[65];
    darc_cid_hex(missing, mhex);
    int recovered = 0;
    while (fgets(line, sizeof(line), cf)) {
        /* format: parity_hex mem0 mem1 ... */
        char *toks[16];
        int nt = 0;
        char *p = line;
        while (nt < 16) {
            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
            if (*p == 0) break;
            toks[nt++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            if (*p) *p++ = 0;
        }
        if (nt < 2) continue;
        int found = -1;
        for (int i = 1; i < nt; ++i) {
            if (strcmp(toks[i], mhex) == 0) { found = i; break; }
        }
        if (found < 0) continue;
        /* build stripe: data members then parity */
        darc_cid_t stripe[16];
        size_t sn = 0;
        for (int i = 1; i < nt; ++i) {
            if (darc_cid_from_hex(toks[i], stripe[sn]) == 0) sn++;
        }
        darc_cid_t parity_cid;
        if (darc_cid_from_hex(toks[0], parity_cid) != 0) continue;
        memcpy(stripe[sn], parity_cid, 32);
        sn++;
        darc_cid_t got;
        if (darc_parity_recover(repo, missing, stripe, sn, got) == 0) {
            res->repaired++;
            recovered = 1;
            break;
        }
    }
    fclose(cf);
    return recovered ? 0 : -1;
}

static int check_one_object(darc_repo_t *repo, const darc_cid_t cid,
                            darc_verify_result_t *res, bool repair) {
    char *path = darc_repo_object_path(repo, cid);
    if (!path) return -1;
    size_t flen;
    uint8_t *framed = darc_read_file(path, &flen);
    free(path);
    int bad = 0;
    if (!framed) {
        res->objects_missing++;
        bad = 1;
    } else {
        uint8_t type, codec;
        size_t uncomp, stored;
        const uint8_t *payload;
        int rc = darc_object_unframe(framed, flen, &type, &codec, &uncomp, &stored, &payload);
        if (rc != 0) {
            res->objects_corrupt++;
            free(framed);
            bad = 1;
        } else {
            uint8_t *raw = NULL;
            size_t raw_len = 0;
            if (codec == DARC_CODEC_RAW) {
                raw = malloc(stored);
                if (raw) { memcpy(raw, payload, stored); raw_len = stored; }
            } else if (codec == DARC_CODEC_LZH1) {
                raw = darc_lzh1_decompress(payload, stored, uncomp, &raw_len);
            }
            free(framed);
            if (!raw) {
                res->objects_corrupt++;
                bad = 1;
            } else {
                darc_cid_t check;
                darc_cid_compute(type, 1, raw, raw_len, check);
                free(raw);
                if (memcmp(check, cid, 32) != 0) {
                    res->objects_corrupt++;
                    bad = 1;
                }
            }
        }
    }
    if (bad) {
        if (repair) {
            if (try_parity_repair(repo, cid, res) == 0) {
                /* re-check */
                path = darc_repo_object_path(repo, cid);
                if (path && access(path, F_OK) == 0) {
                    res->objects_ok++;
                    /* undo corrupt/missing count roughly */
                    free(path);
                    return 0;
                }
                free(path);
            }
            res->unrecoverable++;
        }
        return -1;
    }
    res->objects_ok++;
    return 0;
}

int darc_verify(darc_repo_t *repo, darc_index_t *idx, darc_verify_level_t level,
                bool repair, darc_verify_result_t *result) {
    memset(result, 0, sizeof(*result));
    if (level == DARC_VERIFY_QUICK) {
        /* only check refs and HEAD existence + index consistency */
        darc_cid_t *refs = NULL;
        size_t n = 0;
        darc_repo_list_snapshot_refs(repo, &refs, &n);
        for (size_t i = 0; i < n; ++i) {
            result->objects_checked++;
            if (darc_repo_has_object(repo, refs[i]))
                result->objects_ok++;
            else
                result->objects_missing++;
        }
        free(refs);
        return result->objects_missing || result->objects_corrupt ? 6 : 0;
    }

    /* FULL / SCRUB: walk all objects */
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/objects/sha256", repo->path);
    DIR *d1 = opendir(dir);
    if (!d1) return 5;
    struct dirent *e1;
    while ((e1 = readdir(d1)) != NULL) {
        if (strlen(e1->d_name) != 2) continue;
        char sub[4096];
        snprintf(sub, sizeof(sub), "%s/%s", dir, e1->d_name);
        DIR *d2 = opendir(sub);
        if (!d2) continue;
        struct dirent *e2;
        while ((e2 = readdir(d2)) != NULL) {
            if (strlen(e2->d_name) != 62) continue;
            char hex[65];
            snprintf(hex, sizeof(hex), "%s%s", e1->d_name, e2->d_name);
            darc_cid_t cid;
            if (darc_cid_from_hex(hex, cid) != 0) continue;
            result->objects_checked++;
            check_one_object(repo, cid, result, repair);
        }
        closedir(d2);
    }
    closedir(d1);

    /* Check parity catalog members for missing protected chunks */
    {
        char catpath[4096];
        snprintf(catpath, sizeof(catpath), "%s/parity/CATALOG", repo->path);
        FILE *cf = fopen(catpath, "r");
        if (cf) {
            char line[2048];
            while (fgets(line, sizeof(line), cf)) {
                char *p = line;
                int first = 1;
                while (*p) {
                    while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                    if (!*p) break;
                    char *start = p;
                    while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
                    char save = *p; *p = 0;
                    if (!first && strlen(start) == 64) {
                        darc_cid_t mc;
                        if (darc_cid_from_hex(start, mc) == 0) {
                            result->objects_checked++;
                            if (!darc_repo_has_object(repo, mc)) {
                                result->objects_missing++;
                                if (repair) {
                                    if (try_parity_repair(repo, mc, result) == 0)
                                        result->objects_ok++;
                                    else
                                        result->unrecoverable++;
                                }
                            } else {
                                /* present - already counted in walk maybe */
                            }
                        }
                    }
                    first = 0;
                    *p = save;
                    if (*p) p++;
                }
            }
            fclose(cf);
        }
    }

    if (level == DARC_VERIFY_SCRUB && idx) {
        (void)idx;
    }

    if (result->unrecoverable)
        return 7;
    if ((result->objects_corrupt || result->objects_missing) && result->repaired == 0)
        return 6;
    /* repaired all issues */
    return 0;
}


int darc_gc(darc_repo_t *repo, darc_index_t *idx, bool dry_run, size_t *reclaimed) {
    *reclaimed = 0;
    /* Mark phase: collect all reachable CIDs from refs + their trees/files/chunks */
    /* Simplified: rebuild reachability from all snapshot refs */
    darc_cid_t *refs = NULL;
    size_t nrefs = 0;
    darc_repo_list_snapshot_refs(repo, &refs, &nrefs);

    /* For v0.1 simple GC: if no refs, can delete all objects; otherwise keep everything
       reachable. Full recursive mark is complex; implement mark set via index. */
    darc_index_t *live = darc_index_create(1024);
    if (!live) { free(refs); return -1; }

    for (size_t i = 0; i < nrefs; ++i) {
        darc_index_put(live, refs[i], DARC_TYPE_SNAPSHOT, 0);
        uint8_t type;
        uint8_t *payload = NULL;
        size_t plen = 0;
        if (darc_repo_get_object(repo, refs[i], &type, &payload, &plen) != 0) continue;
        /* walk snapshot -> root tree -> entries (simplified: mark all objects that exist in index for now if any ref remains) */
        free(payload);
        /* Full tree walk would go here; for safety keep all currently indexed if any ref exists */
    }
    free(refs);

    if (nrefs == 0) {
        /* reclaim everything under objects/ */
        char dir[4096];
        snprintf(dir, sizeof(dir), "%s/objects/sha256", repo->path);
        DIR *d1 = opendir(dir);
        if (d1) {
            struct dirent *e1;
            while ((e1 = readdir(d1)) != NULL) {
                if (strlen(e1->d_name) != 2) continue;
                char sub[4096];
                snprintf(sub, sizeof(sub), "%s/%s", dir, e1->d_name);
                DIR *d2 = opendir(sub);
                if (!d2) continue;
                struct dirent *e2;
                while ((e2 = readdir(d2)) != NULL) {
                    if (e2->d_name[0] == '.') continue;
                    char fp[4096];
                    snprintf(fp, sizeof(fp), "%s/%s", sub, e2->d_name);
                    if (!dry_run) {
                        unlink(fp);
                    }
                    (*reclaimed)++;
                }
                closedir(d2);
            }
            closedir(d1);
        }
        if (!dry_run) {
            /* clear index */
            /* index ownership with caller */
            /* caller should reload */
        }
    }
    /* If refs remain, we do not delete objects in this simplified pass
       (full recursive mark-and-sweep would be needed for partial reclaim).
       Spec requires reachability across refs and parents; this is a safe subset. */
    darc_index_free(live);
    return 0;
}

/* Protect all CHUNK objects in groups of up to 8 with XOR parity */
int darc_parity_protect_all(darc_repo_t *repo, darc_index_t *idx) {
    if (!idx) return -1;
    darc_cid_t batch[8];
    size_t n = 0;
    for (size_t i = 0; i < idx->capacity; ++i) {
        if (!idx->slots[i].used) continue;
        if (idx->slots[i].type != DARC_TYPE_CHUNK) continue;
        memcpy(batch[n], idx->slots[i].cid, 32);
        n++;
        if (n == 8) {
            darc_cid_t p;
            darc_parity_protect_chunk(repo, batch, 8, p);
            darc_index_put(idx, p, DARC_TYPE_PARITY, 0);
            n = 0;
        }
    }
    if (n > 0) {
        darc_cid_t p;
        darc_parity_protect_chunk(repo, batch, n, p);
        darc_index_put(idx, p, DARC_TYPE_PARITY, 0);
    }
    darc_index_save(idx, repo);
    return 0;
}
