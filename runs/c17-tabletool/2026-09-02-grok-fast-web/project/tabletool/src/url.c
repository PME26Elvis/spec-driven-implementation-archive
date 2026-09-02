#include "common.h"
#include "url.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int url_canonicalize(const char *s, size_t n, char **out, size_t *out_len) {
    if (n < 8) return -1;
    size_t i = 0;
    char scheme[8]; size_t sl = 0;
    while (i < n && s[i] != ':' && sl < 7) {
        char c = s[i++];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c < 'a' || c > 'z') return -1;
        scheme[sl++] = c;
    }
    scheme[sl] = 0;
    if (strcmp(scheme, "http") && strcmp(scheme, "https")) return -1;
    if (i + 2 >= n || s[i] != ':' || s[i+1] != '/' || s[i+2] != '/') return -1;
    i += 3;
    if (i < n && s[i] == '[') return -1;
    size_t auth_start = i;
    while (i < n && s[i] != '/' && s[i] != '?' && s[i] != '#') {
        if (s[i] == '@') return -1;
        i++;
    }
    size_t auth_end = i;
    if (auth_end == auth_start) return -1;
    size_t colon = (size_t)-1;
    for (size_t k = auth_start; k < auth_end; k++) if (s[k] == ':') { colon = k; break; }
    size_t host_start = auth_start, host_end = colon == (size_t)-1 ? auth_end : colon;
    int port = -1;
    if (colon != (size_t)-1) {
        if (colon + 1 >= auth_end) return -1;
        port = 0;
        for (size_t k = colon + 1; k < auth_end; k++) {
            if (s[k] < '0' || s[k] > '9') return -1;
            port = port * 10 + (s[k]-'0');
            if (port > 65535) return -1;
        }
        if (port < 1) return -1;
    }
    size_t hlen = host_end - host_start;
    if (hlen == 0 || hlen > 253) return -1;
    int is_ipv4 = 1;
    for (size_t k = host_start; k < host_end; k++) {
        char c = s[k];
        if (!((c >= '0' && c <= '9') || c == '.')) { is_ipv4 = 0; break; }
    }
    char hostbuf[256];
    if (is_ipv4) {
        int octets[4], oc = 0; size_t k = host_start;
        while (k < host_end && oc < 4) {
            if (s[k] < '0' || s[k] > '9') return -1;
            int v = 0, digits = 0;
            while (k < host_end && s[k] >= '0' && s[k] <= '9') {
                if (digits == 1 && v == 0) return -1;
                v = v * 10 + (s[k]-'0');
                if (v > 255) return -1;
                digits++; k++;
            }
            if (digits == 0) return -1;
            octets[oc++] = v;
            if (k < host_end) { if (s[k] != '.') return -1; k++; }
        }
        if (oc != 4 || k != host_end) return -1;
        snprintf(hostbuf, sizeof(hostbuf), "%d.%d.%d.%d", octets[0], octets[1], octets[2], octets[3]);
    } else {
        size_t k = host_start; int label_len = 0, first = 1;
        while (k < host_end) {
            char c = s[k];
            if (c == '.') {
                if (label_len == 0 || label_len > 63) return -1;
                if (s[k-1] == '-') return -1;
                label_len = 0; first = 1; k++; continue;
            }
            char lc = c; if (lc >= 'A' && lc <= 'Z') lc += 32;
            if (!((lc >= 'a' && lc <= 'z') || (lc >= '0' && lc <= '9') || lc == '-')) return -1;
            if (first && lc == '-') return -1;
            first = 0; label_len++;
            if (label_len > 63) return -1;
            k++;
        }
        if (label_len == 0 || s[host_end-1] == '-') return -1;
        for (size_t j = 0; j < hlen; j++) {
            char c = s[host_start+j];
            if (c >= 'A' && c <= 'Z') c += 32;
            hostbuf[j] = c;
        }
        hostbuf[hlen] = 0;
    }
    /* path query frag */
    size_t path_start = i, query_start = (size_t)-1, frag_start = (size_t)-1;
    while (i < n) {
        if (s[i] == '?' && query_start == (size_t)-1 && frag_start == (size_t)-1) query_start = i;
        else if (s[i] == '#' && frag_start == (size_t)-1) frag_start = i;
        i++;
    }
    size_t path_end = query_start != (size_t)-1 ? query_start : (frag_start != (size_t)-1 ? frag_start : n);
    size_t query_end = frag_start != (size_t)-1 ? frag_start : n;

    /* validate and uppercase percent in path/query/frag */
    char pathbuf[1024]; size_t plen = 0;
    if (path_start >= path_end) {
        pathbuf[plen++] = '/';
    } else {
        for (size_t k = path_start; k < path_end; k++) {
            char c = s[k];
            if (c == '%' && k + 2 < path_end && is_hex(s[k+1]) && is_hex(s[k+2])) {
                pathbuf[plen++] = '%';
                pathbuf[plen++] = (char)toupper((unsigned char)s[k+1]);
                pathbuf[plen++] = (char)toupper((unsigned char)s[k+2]);
                k += 2;
            } else if (c == '%') return -1;
            else pathbuf[plen++] = c;
        }
        if (pathbuf[0] != '/') return -1;
    }
    /* dot-segment removal */
    {
        const char *segs[128]; size_t segl[128]; int ns = 0;
        int force_slash = 0;
        if (plen >= 1 && pathbuf[plen-1] == '/') force_slash = 1;
        if (plen >= 2 && pathbuf[plen-2] == '/' && pathbuf[plen-1] == '.') force_slash = 1;
        if (plen >= 3 && pathbuf[plen-3] == '/' && pathbuf[plen-2] == '.' && pathbuf[plen-1] == '.') force_slash = 1;
        size_t q = 1;
        while (q < plen) {
            size_t e = q;
            while (e < plen && pathbuf[e] != '/') e++;
            size_t len = e - q;
            if (len == 1 && pathbuf[q] == '.') { /* skip */ }
            else if (len == 2 && pathbuf[q] == '.' && pathbuf[q+1] == '.') {
                if (ns > 0) ns--;
            } else {
                if (ns < 128) { segs[ns] = pathbuf + q; segl[ns] = len; ns++; }
            }
            q = e + (e < plen ? 1 : 0);
        }
        char npath[1024]; size_t np = 0;
        npath[np++] = '/';
        for (int si = 0; si < ns; si++) {
            if (si) npath[np++] = '/';
            memcpy(npath + np, segs[si], segl[si]); np += segl[si];
        }
        if (force_slash && np > 1 && npath[np-1] != '/') npath[np++] = '/';
        memcpy(pathbuf, npath, np); plen = np;
    }

    char qbuf[512]; size_t qlen = 0;
    if (query_start != (size_t)-1) {
        for (size_t k = query_start; k < query_end; k++) {
            char c = s[k];
            if (c == '%' && k+2 < query_end && is_hex(s[k+1]) && is_hex(s[k+2])) {
                qbuf[qlen++] = '%';
                qbuf[qlen++] = (char)toupper((unsigned char)s[k+1]);
                qbuf[qlen++] = (char)toupper((unsigned char)s[k+2]);
                k += 2;
            } else if (c == '%') return -1;
            else qbuf[qlen++] = c;
        }
    }
    char fbuf[512]; size_t flen = 0;
    if (frag_start != (size_t)-1) {
        for (size_t k = frag_start; k < n; k++) {
            char c = s[k];
            if (c == '%' && k+2 < n && is_hex(s[k+1]) && is_hex(s[k+2])) {
                fbuf[flen++] = '%';
                fbuf[flen++] = (char)toupper((unsigned char)s[k+1]);
                fbuf[flen++] = (char)toupper((unsigned char)s[k+2]);
                k += 2;
            } else if (c == '%') return -1;
            else fbuf[flen++] = c;
        }
    }

    char final[2048]; size_t fp = 0;
    #define A(ch) do { if (fp+1>=sizeof(final)) return -1; final[fp++]=(ch);} while(0)
    #define AS(str) do { for(const char*_p=(str);*_p;_p++) A(*_p); } while(0)
    AS(scheme); AS("://"); AS(hostbuf);
    if (port > 0) {
        int def = strcmp(scheme, "http") == 0 ? 80 : 443;
        if (port != def) {
            A(':');
            char pb[16]; int pn = snprintf(pb, sizeof(pb), "%d", port);
            for (int x=0;x<pn;x++) A(pb[x]);
        }
    }
    for (size_t x=0;x<plen;x++) A(pathbuf[x]);
    for (size_t x=0;x<qlen;x++) A(qbuf[x]);
    for (size_t x=0;x<flen;x++) A(fbuf[x]);
    final[fp] = 0;
    *out = tt_strndup(final, fp);
    *out_len = fp;
    return *out ? 0 : -1;
}
