/* sdk_sudoku_logic.c - human-logic solver T1-T8, fixed scan order,
 * full logic solve and difficulty classification (docs/07 sections 8-12,
 * 24-27).
 *
 * The logic solver is fully reproducible: candidate masks are recomputed from
 * formal values at each step, and the scan order (technique priority, then
 * row/col/box, then cell index, then digit) is fixed. It never consults any
 * stored solution.
 */
#include "sudoku/sdk_sudoku.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int popcount16(sdk_cand m) {
    unsigned v = (unsigned)m & 0xFFFFu;
    v = v - ((v >> 1) & 0x5555u);
    v = (v & 0x3333u) + ((v >> 2) & 0x3333u);
    v = (v + (v >> 4)) & 0x0F0Fu;
    v = v + (v >> 8);
    return (int)(v & 0x1Fu);
}

/* Cell index of every position of every unit, in the canonical order used by
 * the fixed scan order of docs/07 section 8: rows 0-8, columns 9-17, boxes
 * 18-26, and inside a unit ascending position. Precomputed because the
 * technique scanners walk it on every step of every logic solve. */
static const int UNIT_CELL[27][9] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8 },
    {  9, 10, 11, 12, 13, 14, 15, 16, 17 },
    { 18, 19, 20, 21, 22, 23, 24, 25, 26 },
    { 27, 28, 29, 30, 31, 32, 33, 34, 35 },
    { 36, 37, 38, 39, 40, 41, 42, 43, 44 },
    { 45, 46, 47, 48, 49, 50, 51, 52, 53 },
    { 54, 55, 56, 57, 58, 59, 60, 61, 62 },
    { 63, 64, 65, 66, 67, 68, 69, 70, 71 },
    { 72, 73, 74, 75, 76, 77, 78, 79, 80 },
    {  0,  9, 18, 27, 36, 45, 54, 63, 72 },
    {  1, 10, 19, 28, 37, 46, 55, 64, 73 },
    {  2, 11, 20, 29, 38, 47, 56, 65, 74 },
    {  3, 12, 21, 30, 39, 48, 57, 66, 75 },
    {  4, 13, 22, 31, 40, 49, 58, 67, 76 },
    {  5, 14, 23, 32, 41, 50, 59, 68, 77 },
    {  6, 15, 24, 33, 42, 51, 60, 69, 78 },
    {  7, 16, 25, 34, 43, 52, 61, 70, 79 },
    {  8, 17, 26, 35, 44, 53, 62, 71, 80 },
    {  0,  1,  2,  9, 10, 11, 18, 19, 20 },
    {  3,  4,  5, 12, 13, 14, 21, 22, 23 },
    {  6,  7,  8, 15, 16, 17, 24, 25, 26 },
    { 27, 28, 29, 36, 37, 38, 45, 46, 47 },
    { 30, 31, 32, 39, 40, 41, 48, 49, 50 },
    { 33, 34, 35, 42, 43, 44, 51, 52, 53 },
    { 54, 55, 56, 63, 64, 65, 72, 73, 74 },
    { 57, 58, 59, 66, 67, 68, 75, 76, 77 },
    { 60, 61, 62, 69, 70, 71, 78, 79, 80 }
};

static void name_of(int cell, char *buf) {
    int r = cell / 9 + 1, c = cell % 9 + 1;
    buf[0] = 'R'; buf[1] = (char)('0' + r);
    buf[2] = 'C'; buf[3] = (char)('0' + c);
    buf[4] = '\0';
}

static void unit_cells(int u, int *cells) {
    memcpy(cells, UNIT_CELL[u], 9 * sizeof(int));
}

static const char *unit_label(int u) {
    static char buf[16];
    if (u < 9) { buf[0]='r'; buf[1]='o'; buf[2]='w'; buf[3]=' '; buf[4]=(char)('0'+u+1); buf[5]='\0'; }
    else if (u < 18) { buf[0]='c'; buf[1]='o'; buf[2]='l'; buf[3]=' '; buf[4]=(char)('0'+(u-9)+1); buf[5]='\0'; }
    else { buf[0]='b'; buf[1]='o'; buf[2]='x'; buf[3]=' '; buf[4]=(char)('0'+(u-18)+1); buf[5]='\0'; }
    return buf;
}

static sdk_cand base_candidates(const sdk_board *b, int cell) {
    int r = cell / 9, c = cell % 9, br = (r / 3) * 3, bc = (c / 3) * 3, i;
    sdk_cand m = SDK_CAND_ALL;
    for (i = 0; i < 9; ++i) {
        int v;
        v = b->cells[r * 9 + i].value; if (v >= 1 && v <= 9) m &= ~SDK_CAND_BIT(v);
        v = b->cells[i * 9 + c].value; if (v >= 1 && v <= 9) m &= ~SDK_CAND_BIT(v);
    }
    for (i = 0; i < 3; ++i) {
        int j;
        for (j = 0; j < 3; ++j) {
            int v = b->cells[(br + i) * 9 + (bc + j)].value;
            if (v >= 1 && v <= 9) m &= ~SDK_CAND_BIT(v);
        }
    }
    return m;
}

static int is_empty(const sdk_board *b, int cell) {
    return b->cells[cell].value == 0;
}

/* ------------------------------------------------------------------ */
/* Candidate derivation                                                */
/* ------------------------------------------------------------------ */

int sdk_derive_candidates(const sdk_board *b, sdk_cand *out) {
    int i;
    for (i = 0; i < 81; ++i) {
        if (is_empty(b, i)) {
            sdk_cand m = base_candidates(b, i);
            if (m == SDK_CAND_EMPTY) return 0;   /* unsatisfiable */
            out[i] = m;
        } else {
            out[i] = SDK_CAND_EMPTY;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Step construction helpers                                           */
/* ------------------------------------------------------------------ */

static void step_init(sdk_logic_step *s, sdk_tech t, sdk_step_kind k) {
    memset(s, 0, sizeof *s);
    s->tech = t;
    s->kind = k;
    s->target_cell = -1;
    s->score_weight = SDK_TECH_WEIGHT(t);
}

static void add_target(sdk_logic_step *s, int cell) {
    if (s->target_count < SDK_STEP_CELLS) s->target_cells[s->target_count++] = cell;
}
static void add_support(sdk_logic_step *s, int cell) {
    if (s->support_count < SDK_STEP_CELLS) s->support_cells[s->support_count++] = cell;
}
static void add_unit(sdk_logic_step *s, int u) {
    if (s->support_unit_count < SDK_STEP_UNITS) s->support_units[s->support_unit_count++] = u;
}

/* ------------------------------------------------------------------ */
/* Technique scanners (priority order)                                 */
/* ------------------------------------------------------------------ */

static int scan_naked_single(const sdk_board *b, sdk_cand *m, sdk_logic_step *s) {
    int cell;
    for (cell = 0; cell < 81; ++cell) {
        if (is_empty(b, cell) && popcount16(m[cell]) == 1) {
            int d = 0, bit;
            for (bit = 1; bit <= 9; ++bit) if (m[cell] & SDK_CAND_BIT(bit)) { d = bit; break; }
            char nm[8];
            step_init(s, SDK_TECH_NAKED_SINGLE, SDK_STEP_PLACE);
            s->target_cell = cell; s->placed_digit = d;
            s->candidate_before = m[cell]; s->candidate_after = SDK_CAND_EMPTY;
            add_support(s, cell);
            name_of(cell, nm);
            snprintf(s->conclusion, sizeof s->conclusion,
                     "Naked Single at %s: only %d fits, so place %d.", nm, d, d);
            snprintf(s->reason, sizeof s->reason,
                     "After peers exclude all other digits, %s has the single "
                     "candidate %d.", nm, d);
            return 1;
        }
    }
    return 0;
}

static int scan_hidden_single(const sdk_board *b, sdk_cand *m, sdk_logic_step *s) {
    int u;
    for (u = 0; u < 27; ++u) {
        int cells[9], i, d;
        unit_cells(u, cells);
        for (d = 1; d <= 9; ++d) {
            int cnt = 0, last = -1;
            for (i = 0; i < 9; ++i) {
                int c = cells[i];
                if (is_empty(b, c) && (m[c] & SDK_CAND_BIT(d))) { cnt++; last = c; }
            }
            if (cnt == 1) {
                char nm[8];
                step_init(s, SDK_TECH_HIDDEN_SINGLE, SDK_STEP_PLACE);
                s->target_cell = last; s->placed_digit = d;
                s->candidate_before = m[last]; s->candidate_after = SDK_CAND_EMPTY;
                add_support(s, last); add_unit(s, u);
                name_of(last, nm);
                snprintf(s->conclusion, sizeof s->conclusion,
                         "Hidden Single in %s: only %s can contain %d, so place %d at %s.",
                         unit_label(u), nm, d, d, nm);
                snprintf(s->reason, sizeof s->reason,
                         "In %s, digit %d appears as a candidate in exactly one "
                         "cell (%s), so it must go there.", unit_label(u), d, nm);
                return 1;
            }
        }
    }
    return 0;
}

static int scan_pointing(const sdk_board *b, sdk_cand *m, sdk_logic_step *s) {
    int d, u;
    for (d = 1; d <= 9; ++d) {
        for (u = 18; u < 27; ++u) {  /* boxes only */
            int cells[9], i, lst[9], ln = 0, r = -1, c = -1;
            unit_cells(u, cells);
            for (i = 0; i < 9; ++i) {
                int cc = cells[i];
                if (is_empty(b, cc) && (m[cc] & SDK_CAND_BIT(d))) lst[ln++] = cc;
            }
            if (ln == 0) continue;
            /* all in same row? */
            { int rr = lst[0] / 9; int same = 1;
              for (i = 0; i < ln; ++i) if (lst[i] / 9 != rr) { same = 0; break; }
              if (same) r = rr; }
            /* all in same col? */
            { int cc0 = lst[0] % 9; int same = 1;
              for (i = 0; i < ln; ++i) if (lst[i] % 9 != cc0) { same = 0; break; }
              if (same) c = cc0; }
            if (r >= 0) {
                int targets[9], tn = 0;
                for (i = 0; i < 9; ++i) {
                    int cc = r * 9 + i;
                    if ((cc % 9) / 3 != (u - 18) % 3 && is_empty(b, cc) &&
                        (m[cc] & SDK_CAND_BIT(d))) targets[tn++] = cc;
                }
                if (tn > 0) {
                    char nm[8];
                    step_init(s, SDK_TECH_LOCKED_POINTING, SDK_STEP_ELIM);
                    s->affected_candidates = SDK_CAND_BIT(d);
                    for (i = 0; i < ln; ++i) add_support(s, lst[i]);
                    for (i = 0; i < tn; ++i) add_target(s, targets[i]);
                    add_unit(s, u); add_unit(s, r);
                    name_of(lst[0], nm);
                    snprintf(s->conclusion, sizeof s->conclusion,
                             "Locked Candidates (Pointing) %s: %d confined to row %d, "
                             "remove %d from rest of row %d.", unit_label(u), d, r + 1, d, r + 1);
                    snprintf(s->reason, sizeof s->reason,
                             "In %s, every candidate for %d lies in row %d, so %d "
                             "cannot appear elsewhere in that row.", unit_label(u), d, r + 1, d);
                    return 1;
                }
            }
            if (c >= 0) {
                int targets[9], tn = 0;
                for (i = 0; i < 9; ++i) {
                    int cc = i * 9 + c;
                    if ((cc / 9) / 3 != (u - 18) / 3 && is_empty(b, cc) &&
                        (m[cc] & SDK_CAND_BIT(d))) targets[tn++] = cc;
                }
                if (tn > 0) {
                    step_init(s, SDK_TECH_LOCKED_POINTING, SDK_STEP_ELIM);
                    s->affected_candidates = SDK_CAND_BIT(d);
                    for (i = 0; i < ln; ++i) add_support(s, lst[i]);
                    for (i = 0; i < tn; ++i) add_target(s, targets[i]);
                    add_unit(s, u); add_unit(s, 9 + c);
                    snprintf(s->conclusion, sizeof s->conclusion,
                             "Locked Candidates (Pointing) %s: %d confined to column %d, "
                             "remove %d from rest of column %d.", unit_label(u), d, c + 1, d, c + 1);
                    snprintf(s->reason, sizeof s->reason,
                             "In %s, every candidate for %d lies in column %d, so %d "
                             "cannot appear elsewhere in that column.", unit_label(u), d, c + 1, d);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int scan_claiming(const sdk_board *b, sdk_cand *m, sdk_logic_step *s) {
    int d, u;
    for (d = 1; d <= 9; ++d) {
        for (u = 0; u < 18; ++u) {  /* rows and columns */
            int cells[9], i, lst[9], ln = 0, bx = -1;
            unit_cells(u, cells);
            for (i = 0; i < 9; ++i) {
                int cc = cells[i];
                if (is_empty(b, cc) && (m[cc] & SDK_CAND_BIT(d))) lst[ln++] = cc;
            }
            if (ln == 0) continue;
            { int b0 = lst[0] / 9 / 3 * 3 + (lst[0] % 9) / 3; int same = 1;
              for (i = 0; i < ln; ++i) {
                  int bb = lst[i] / 9 / 3 * 3 + (lst[i] % 9) / 3;
                  if (bb != b0) { same = 0; break; }
              }
              if (same) bx = b0; }
            if (bx >= 0) {
                int targets[9], tn = 0, bu = 18 + bx, bcells[9], j;
                unit_cells(bu, bcells);
                for (j = 0; j < 9; ++j) {
                    int cc = bcells[j];
                    int in_unit = 0;
                    for (i = 0; i < 9; ++i) if (cells[i] == cc) in_unit = 1;
                    if (!in_unit && is_empty(b, cc) && (m[cc] & SDK_CAND_BIT(d))) targets[tn++] = cc;
                }
                if (tn > 0) {
                    step_init(s, SDK_TECH_LOCKED_CLAIMING, SDK_STEP_ELIM);
                    s->affected_candidates = SDK_CAND_BIT(d);
                    for (i = 0; i < ln; ++i) add_support(s, lst[i]);
                    for (i = 0; i < tn; ++i) add_target(s, targets[i]);
                    add_unit(s, u); add_unit(s, bu);
                    snprintf(s->conclusion, sizeof s->conclusion,
                             "Locked Candidates (Claiming) %s: %d candidates all in %s, "
                             "remove %d from rest of %s.", unit_label(u), d, unit_label(bu), d, unit_label(bu));
                    snprintf(s->reason, sizeof s->reason,
                             "In %s, every candidate for %d shares %s, so %d cannot "
                             "appear elsewhere in that box.", unit_label(u), d, unit_label(bu), d);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int scan_naked_pair(const sdk_board *b, sdk_cand *m, sdk_logic_step *s) {
    int u;
    for (u = 0; u < 27; ++u) {
        const int *cells = UNIT_CELL[u];
        int pick[9], pn = 0, i, j;

        /* A naked pair needs two empty cells holding the *same* two candidates,
         * so both members must have exactly two candidates. Restricting the
         * quadratic scan to that ascending subset is order preserving: every
         * pair it skips is a pair the full scan rejected as well. */
        for (i = 0; i < 9; ++i) {
            int ci = cells[i];
            if (!is_empty(b, ci)) continue;
            if (popcount16(m[ci]) == 2) pick[pn++] = i;
        }
        for (i = 0; i < pn; ++i) {
            for (j = i + 1; j < pn; ++j) {
                int ci = cells[pick[i]], cj = cells[pick[j]];
                sdk_cand mi = m[ci], mj = m[cj];
                int k, targets[9], tn = 0;
                if (mi != mj) continue;
                for (k = 0; k < 9; ++k) {
                    int ck = cells[k];
                    if (ck == ci || ck == cj) continue;
                    if (!is_empty(b, ck)) continue;
                    if (m[ck] & mi) targets[tn++] = ck;
                }
                if (tn > 0) {
                    char ni[8], nj[8];
                    int pa = 0, pb = 0, bit;
                    for (bit = 1; bit <= 9; ++bit)
                        if (mi & SDK_CAND_BIT(bit)) { if (pa == 0) pa = bit; else pb = bit; }
                    step_init(s, SDK_TECH_NAKED_PAIR, SDK_STEP_ELIM);
                    s->affected_candidates = mi;
                    add_support(s, ci); add_support(s, cj);
                    for (k = 0; k < tn; ++k) add_target(s, targets[k]);
                    add_unit(s, u);
                    name_of(ci, ni); name_of(cj, nj);
                    snprintf(s->conclusion, sizeof s->conclusion,
                             "Naked Pair in %s: %s and %s both hold {%d,%d}; remove those "
                             "digits from their peers.", unit_label(u), ni, nj, pa, pb);
                    snprintf(s->reason, sizeof s->reason,
                             "Two cells in %s contain exactly the same two candidates, "
                             "so no other cell in the unit can take them.", unit_label(u));
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int scan_hidden_pair(const sdk_board *b, sdk_cand *m, sdk_logic_step *s) {
    int u;
    for (u = 0; u < 27; ++u) {
        const int *cells = UNIT_CELL[u];
        unsigned pos[10];
        int d1, d2, k;

        /* pos[d] = bitmap of the unit positions whose candidate set still holds
         * digit d. Computed once per unit so the digit-pair scan below is a few
         * bit operations instead of a nine-cell walk per pair. */
        for (d1 = 1; d1 <= 9; ++d1) pos[d1] = 0u;
        for (k = 0; k < 9; ++k) {
            int ck = cells[k];
            if (!is_empty(b, ck)) continue;
            for (d1 = 1; d1 <= 9; ++d1)
                if (m[ck] & SDK_CAND_BIT(d1)) pos[d1] |= 1u << k;
        }

        for (d1 = 1; d1 <= 9; ++d1) {
            if (pos[d1] == 0u) continue;   /* digit already placed in this unit */
            for (d2 = d1 + 1; d2 <= 9; ++d2) {
                sdk_cand union_bits = SDK_CAND_BIT(d1) | SDK_CAND_BIT(d2);
                unsigned hitmap = pos[d1] | pos[d2];
                int hits[9], hn = 0;
                /* Both digits must still be unplaced and placeable in this unit;
                   otherwise the pigeonhole argument does not hold. */
                if (pos[d2] == 0u) continue;
                if (popcount16((sdk_cand)hitmap) != 2) continue;
                for (k = 0; k < 9; ++k)
                    if (hitmap & (1u << k)) hits[hn++] = cells[k];
                { sdk_cand remove = (m[hits[0]] | m[hits[1]]) & ~union_bits;
                  if (remove == 0) continue;
                  step_init(s, SDK_TECH_HIDDEN_PAIR, SDK_STEP_ELIM);
                  s->affected_candidates = remove;
                  add_target(s, hits[0]); add_target(s, hits[1]);
                  add_support(s, hits[0]); add_support(s, hits[1]);
                  add_unit(s, u);
                  snprintf(s->conclusion, sizeof s->conclusion,
                           "Hidden Pair in %s: digits %d and %d share exactly two cells; "
                           "clear other candidates there.", unit_label(u), d1, d2);
                  snprintf(s->reason, sizeof s->reason,
                           "In %s, only two cells can hold %d or %d, so those cells must "
                           "be one of them.", unit_label(u), d1, d2);
                  return 1;
                }
            }
        }
    }
    return 0;
}

static int scan_naked_triple(const sdk_board *b, sdk_cand *m, sdk_logic_step *s) {
    int u;
    for (u = 0; u < 27; ++u) {
        const int *cells = UNIT_CELL[u];
        int pick[9], pn = 0, i, j, k, pc;

        /* Every member of a naked triple has two or three candidates, so the
         * cubic scan only has to consider that ascending subset - the triples it
         * skips are exactly the ones the full scan rejected on the same test. */
        for (i = 0; i < 9; ++i) {
            int ci = cells[i];
            if (!is_empty(b, ci)) continue;
            pc = popcount16(m[ci]);
            if (pc >= 2 && pc <= 3) pick[pn++] = i;
        }
        for (i = 0; i < pn; ++i)
        for (j = i + 1; j < pn; ++j)
        for (k = j + 1; k < pn; ++k) {
            int ci = cells[pick[i]], cj = cells[pick[j]], ck = cells[pick[k]];
            sdk_cand mi = m[ci], mj = m[cj], mk = m[ck];
            sdk_cand union3 = mi | mj | mk;
            int t, targets[9], tn = 0;
            /* union3 is by construction a superset of each member, so the three
               "member escapes the union" tests of the naive form are vacuous. */
            if (popcount16(union3) != 3) continue;
            for (t = 0; t < 9; ++t) {
                int ct = cells[t];
                if (ct == ci || ct == cj || ct == ck) continue;
                if (!is_empty(b, ct)) continue;
                if (m[ct] & union3) targets[tn++] = ct;
            }
            if (tn > 0) {
                step_init(s, SDK_TECH_NAKED_TRIPLE, SDK_STEP_ELIM);
                s->affected_candidates = union3;
                add_support(s, ci); add_support(s, cj); add_support(s, ck);
                { int q; for (q = 0; q < tn; ++q) add_target(s, targets[q]); }
                add_unit(s, u);
                snprintf(s->conclusion, sizeof s->conclusion,
                         "Naked Triple in %s: three cells share exactly three candidates; "
                         "remove them from peers.", unit_label(u));
                snprintf(s->reason, sizeof s->reason,
                         "Three cells in %s collectively hold exactly three candidate "
                         "digits, none of which can appear elsewhere in the unit.",
                         unit_label(u));
                return 1;
            }
        }
    }
    return 0;
}

static int scan_hidden_triple(const sdk_board *b, sdk_cand *m, sdk_logic_step *s) {
    int u;
    for (u = 0; u < 27; ++u) {
        const int *cells = UNIT_CELL[u];
        unsigned pos[10];
        int d1, d2, d3, t;

        /* Same per-unit digit position bitmaps as the hidden pair scan. */
        for (d1 = 1; d1 <= 9; ++d1) pos[d1] = 0u;
        for (t = 0; t < 9; ++t) {
            int ck = cells[t];
            if (!is_empty(b, ck)) continue;
            for (d1 = 1; d1 <= 9; ++d1)
                if (m[ck] & SDK_CAND_BIT(d1)) pos[d1] |= 1u << t;
        }

        for (d1 = 1; d1 <= 9; ++d1) {
            if (pos[d1] == 0u) continue;
        for (d2 = d1 + 1; d2 <= 9; ++d2) {
            if (pos[d2] == 0u) continue;
        for (d3 = d2 + 1; d3 <= 9; ++d3) {
            sdk_cand union_bits = SDK_CAND_BIT(d1) | SDK_CAND_BIT(d2) | SDK_CAND_BIT(d3);
            unsigned hitmap = pos[d1] | pos[d2] | pos[d3];
            int hits[9], hn = 0;
            /* All three digits must still be unplaced and placeable in this unit;
               otherwise the pigeonhole argument does not hold. */
            if (pos[d3] == 0u) continue;
            if (popcount16((sdk_cand)hitmap) != 3) continue;
            for (t = 0; t < 9; ++t)
                if (hitmap & (1u << t)) hits[hn++] = cells[t];
            { sdk_cand remove = (m[hits[0]] | m[hits[1]] | m[hits[2]]) & ~union_bits;
              if (remove == 0) continue;
              step_init(s, SDK_TECH_HIDDEN_TRIPLE, SDK_STEP_ELIM);
              s->affected_candidates = remove;
              add_target(s, hits[0]); add_target(s, hits[1]); add_target(s, hits[2]);
              add_support(s, hits[0]); add_support(s, hits[1]); add_support(s, hits[2]);
              add_unit(s, u);
              snprintf(s->conclusion, sizeof s->conclusion,
                       "Hidden Triple in %s: digits %d,%d,%d share exactly three cells; "
                       "clear other candidates there.", unit_label(u), d1, d2, d3);
              snprintf(s->reason, sizeof s->reason,
                       "In %s, only three cells can hold %d, %d or %d, so those cells "
                       "must be among them.", unit_label(u), d1, d2, d3);
              return 1;
            }
        }   /* d3 */
        }   /* d2 */
        }   /* d1 */
    }
    return 0;
}

int sdk_logic_find_step_from(const sdk_board *b, const sdk_cand *in_masks,
                             sdk_tech min_tech, sdk_logic_step *out) {
    sdk_cand local[81];
    int i;
    if (in_masks) {
        memcpy(local, in_masks, sizeof local);
    } else {
        if (!sdk_derive_candidates(b, local)) return 0;
    }
    /* Keep masks a subset of true candidates (preserves eliminations). */
    for (i = 0; i < 81; ++i)
        if (is_empty(b, i)) local[i] &= base_candidates(b, i);

    /* Fixed priority order of docs/07 section 8; `min_tech` only skips a
       prefix of that order, it never reorders it. */
    if (min_tech <= SDK_TECH_NAKED_SINGLE  && scan_naked_single(b, local, out)) return 1;
    if (min_tech <= SDK_TECH_HIDDEN_SINGLE && scan_hidden_single(b, local, out)) return 1;
    if (min_tech <= SDK_TECH_LOCKED_POINTING && scan_pointing(b, local, out)) return 1;
    if (min_tech <= SDK_TECH_LOCKED_CLAIMING && scan_claiming(b, local, out)) return 1;
    if (min_tech <= SDK_TECH_NAKED_PAIR    && scan_naked_pair(b, local, out)) return 1;
    if (min_tech <= SDK_TECH_HIDDEN_PAIR   && scan_hidden_pair(b, local, out)) return 1;
    if (min_tech <= SDK_TECH_NAKED_TRIPLE  && scan_naked_triple(b, local, out)) return 1;
    if (min_tech <= SDK_TECH_HIDDEN_TRIPLE && scan_hidden_triple(b, local, out)) return 1;
    return 0;
}

int sdk_logic_find_step(const sdk_board *b, const sdk_cand *in_masks,
                        sdk_logic_step *out) {
    return sdk_logic_find_step_from(b, in_masks, SDK_TECH_NAKED_SINGLE, out);
}

int sdk_logic_apply(const sdk_board *b, const sdk_logic_step *step,
                    sdk_board *out) {
    int i;
    sdk_board_copy(b, out);
    if (step->kind == SDK_STEP_PLACE) {
        if (step->target_cell < 0 || step->target_cell >= 81) return 0;
        sdk_board_set(out, step->target_cell, step->placed_digit, SDK_O_PLAYER);
        /* remove placed digit from peers */
        int peers[20], pn = sdk_peers(step->target_cell, peers);
        for (i = 0; i < pn; ++i)
            if (out->cells[peers[i]].value == 0) {
                /* mask bookkeeping not stored in board; nothing else to do */
            }
    } else {
        for (i = 0; i < step->target_count; ++i) {
            int c = step->target_cells[i];
            if (c >= 0 && c < 81 && out->cells[c].value == 0)
                out->cells[c].notes &= (uint16_t)(~step->affected_candidates & 0x01FFu);
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Full logic solve + difficulty                                       */
/* ------------------------------------------------------------------ */

static void apply_to_masks(sdk_cand *m, const sdk_board *work,
                           const sdk_logic_step *step) {
    int i;
    (void)work;
    if (step->kind == SDK_STEP_PLACE) {
        int c = step->target_cell, d = step->placed_digit;
        int peers[20], pn = sdk_peers(c, peers);
        m[c] = SDK_CAND_EMPTY;
        for (i = 0; i < pn; ++i) m[peers[i]] &= ~SDK_CAND_BIT(d);
    } else {
        for (i = 0; i < step->target_count; ++i)
            m[step->target_cells[i]] &= ~step->affected_candidates;
    }
}

int sdk_logic_solve(const sdk_board *in, sdk_board *out,
                    sdk_logic_trace *trace) {
    sdk_board work;
    sdk_cand m[81];
    int i;
    memset(trace, 0, sizeof *trace);
    sdk_board_copy(in, &work);
    if (!sdk_derive_candidates(&work, m)) { trace->stalled = 1; if (out) sdk_board_copy(&work, out); return 1; }

    for (;;) {
        int empty = 0;
        for (i = 0; i < 81; ++i) {
            if (is_empty(&work, i)) {
                empty++;
                m[i] &= base_candidates(&work, i);   /* consistency, keep eliminations */
                if (m[i] == SDK_CAND_EMPTY) { trace->stalled = 1; if (out) sdk_board_copy(&work, out); return 1; }
            }
        }
        if (empty == 0) { trace->solved = 1; break; }
        {
            sdk_logic_step step;
            if (!sdk_logic_find_step(&work, m, &step)) { trace->stalled = 1; break; }
            if (step.kind == SDK_STEP_PLACE)
                sdk_board_set(&work, step.target_cell, step.placed_digit, SDK_O_PLAYER);
            apply_to_masks(m, &work, &step);
            trace->logic_score += step.score_weight;
            trace->step_count++;
            if (step.tech > trace->max_technique) trace->max_technique = step.tech;
            if (step.tech >= 1 && step.tech <= 8) trace->tech_counts[step.tech]++;
        }
    }
    if (out) sdk_board_copy(&work, out);
    return 1;
}

static int count_given(const sdk_board *b) {
    int i, n = 0;
    for (i = 0; i < 81; ++i) if (b->cells[i].value != 0) n++;
    return n;
}

void sdk_classify_difficulty(const sdk_board *puzzle,
                             sdk_difficulty_result *out) {
    sdk_board solved;
    sdk_logic_trace tr;
    memset(out, 0, sizeof *out);
    out->difficulty = SDK_DIFF_UNKNOWN;
    sdk_logic_solve(puzzle, &solved, &tr);
    if (tr.stalled) { out->stalled = 1; return; }

    {
        int clue = count_given(puzzle);
        int t2 = tr.tech_counts[SDK_TECH_HIDDEN_SINGLE];
        int t3t6 = tr.tech_counts[SDK_TECH_LOCKED_POINTING] +
                   tr.tech_counts[SDK_TECH_LOCKED_CLAIMING] +
                   tr.tech_counts[SDK_TECH_NAKED_PAIR] +
                   tr.tech_counts[SDK_TECH_HIDDEN_PAIR];
        int t5t6 = tr.tech_counts[SDK_TECH_NAKED_PAIR] +
                   tr.tech_counts[SDK_TECH_HIDDEN_PAIR];
        int t7t8 = tr.tech_counts[SDK_TECH_NAKED_TRIPLE] +
                   tr.tech_counts[SDK_TECH_HIDDEN_TRIPLE];
        int score = tr.logic_score;
        out->clue_count = clue;
        out->logic_score = score;
        out->max_technique = tr.max_technique;
        { int t; for (t = 1; t <= 8; ++t) out->tech_counts[t] = tr.tech_counts[t]; }

        /* Hard */
        if ((t7t8 >= 1 || (t5t6 >= 3 && score >= 220)) &&
            score >= 180 && score <= 520 && clue >= 24 && clue <= 34 &&
            tr.max_technique <= SDK_TECH_HIDDEN_TRIPLE) {
            out->difficulty = SDK_DIFF_HARD;
            return;
        }
        /* Medium */
        if (t3t6 >= 1 && t7t8 == 0 &&
            score >= 70 && score <= 260 && clue >= 30 && clue <= 40) {
            out->difficulty = SDK_DIFF_MEDIUM;
            return;
        }
        /* Easy */
        if (tr.max_technique <= SDK_TECH_HIDDEN_SINGLE && t2 >= 1 &&
            score >= 20 && score <= 120 && clue >= 36 && clue <= 49) {
            out->difficulty = SDK_DIFF_EASY;
            return;
        }
    }
}
