/* sdk_sudoku_board.c - board helpers, validator, submit judgement. */
#include "sudoku/sdk_sudoku.h"

#include <string.h>

int sdk_row_of(int cell) { return cell / SDK_GRID; }
int sdk_col_of(int cell) { return cell % SDK_GRID; }
int sdk_box_of(int cell) {
    return (sdk_row_of(cell) / 3) * 3 + (sdk_col_of(cell) / 3);
}

int sdk_peers(int cell, int *out_peers) {
    int r = sdk_row_of(cell), c = sdk_col_of(cell);
    int seen[SDK_BOARD_CELLS];
    int i, n = 0;
    memset(seen, 0, sizeof seen);
    for (i = 0; i < SDK_GRID; ++i) {
        int rc = r * SDK_GRID + i;       /* same row */
        int cc = i * SDK_GRID + c;       /* same column */
        if (rc != cell) seen[rc] = 1;
        if (cc != cell) seen[cc] = 1;
    }
    {
        int br = (r / 3) * 3, bc = (c / 3) * 3, dr, dc;
        for (dr = 0; dr < 3; ++dr)
            for (dc = 0; dc < 3; ++dc) {
                int bc_cell = (br + dr) * SDK_GRID + (bc + dc);
                if (bc_cell != cell) seen[bc_cell] = 1;
            }
    }
    for (i = 0; i < SDK_BOARD_CELLS; ++i)
        if (seen[i]) out_peers[n++] = i;
    return n;
}

void sdk_board_copy(const sdk_board *src, sdk_board *dst) {
    memcpy(dst, src, sizeof *dst);
}

int sdk_board_set(sdk_board *b, int cell, int value, sdk_origin origin) {
    if (cell < 0 || cell >= SDK_BOARD_CELLS) return 0;
    b->cells[cell].value = (uint8_t)value;
    b->cells[cell].origin = (uint8_t)origin;
    b->cells[cell].notes = 0;
    return 1;
}

void sdk_board_serialize(const sdk_board *b, char *buf) {
    int i;
    for (i = 0; i < SDK_BOARD_CELLS; ++i) {
        int v = b->cells[i].value;
        buf[i] = v == 0 ? '.' : (char)('0' + v);
    }
    buf[SDK_BOARD_CELLS] = '\0';
}

int sdk_board_parse(const char *lines[SDK_GRID], sdk_board *out) {
    int r, c;
    memset(out, 0, sizeof *out);
    for (r = 0; r < SDK_GRID; ++r) {
        const char *line = lines[r];
        size_t len = strlen(line);
        /* Allow optional trailing whitespace by scanning exactly 9 glyphs. */
        int cc = 0;
        for (c = 0; c < SDK_GRID; ++c) {
            /* skip spaces within the line if present */
            while (cc < (int)len && line[cc] == ' ') ++cc;
            if (cc >= (int)len) return 0;
            char ch = line[cc++];
            int cell = r * SDK_GRID + c;
            if (ch == '.') {
                out->cells[cell].value = 0;
                out->cells[cell].origin = (uint8_t)SDK_O_EMPTY;
            } else if (ch >= '1' && ch <= '9') {
                out->cells[cell].value = (uint8_t)(ch - '0');
                out->cells[cell].origin = (uint8_t)SDK_O_GIVEN;
            } else {
                return 0;
            }
        }
    }
    return 1;
}

/* Computes conflict digit masks for one unit given the array of cell indices. */
static void unit_conflicts(const sdk_board *b, const int *cells,
                           sdk_cand *out_mask, uint8_t *cell_flag) {
    int counts[SDK_CAND_MAX + 1] = {0};
    int i, d;
    for (i = 0; i < SDK_GRID; ++i) {
        int v = b->cells[cells[i]].value;
        if (v >= 1 && v <= SDK_CAND_MAX) counts[v]++;
    }
    *out_mask = SDK_CAND_EMPTY;
    for (d = 1; d <= SDK_CAND_MAX; ++d)
        if (counts[d] > 1) *out_mask |= SDK_CAND_BIT(d);
    for (i = 0; i < SDK_GRID; ++i) {
        int v = b->cells[cells[i]].value;
        if (v >= 1 && v <= SDK_CAND_MAX && counts[v] > 1)
            cell_flag[cells[i]] = 1;
    }
}

void sdk_validate(const sdk_board *b, sdk_validation *out) {
    int i;
    int rows[SDK_GRID][SDK_GRID], cols[SDK_GRID][SDK_GRID], boxes[SDK_GRID][SDK_GRID];
    memset(out, 0, sizeof *out);
    for (i = 0; i < SDK_BOARD_CELLS; ++i) {
        int r = sdk_row_of(i), c = sdk_col_of(i), bx = sdk_box_of(i);
        rows[r][c] = i;
        cols[c][r] = i;
        boxes[bx][(r % 3) * 3 + (c % 3)] = i;
    }
    for (i = 0; i < SDK_GRID; ++i) {
        unit_conflicts(b, rows[i], &out->row_conflict[i], out->cell_conflict);
        unit_conflicts(b, cols[i], &out->col_conflict[i], out->cell_conflict);
        unit_conflicts(b, boxes[i], &out->box_conflict[i], out->cell_conflict);
    }
    out->empty_count = 0;
    for (i = 0; i < SDK_BOARD_CELLS; ++i)
        if (b->cells[i].value == 0) out->empty_count++;
    out->is_complete = (out->empty_count == 0);
    out->valid_complete = out->is_complete &&
        (out->row_conflict[0] == 0) && (out->row_conflict[1] == 0) &&
        (out->row_conflict[2] == 0) && (out->row_conflict[3] == 0) &&
        (out->row_conflict[4] == 0) && (out->row_conflict[5] == 0) &&
        (out->row_conflict[6] == 0) && (out->row_conflict[7] == 0) &&
        (out->row_conflict[8] == 0) &&
        (out->col_conflict[0] == 0) && (out->col_conflict[1] == 0) &&
        (out->col_conflict[2] == 0) && (out->col_conflict[3] == 0) &&
        (out->col_conflict[4] == 0) && (out->col_conflict[5] == 0) &&
        (out->col_conflict[6] == 0) && (out->col_conflict[7] == 0) &&
        (out->col_conflict[8] == 0) &&
        (out->box_conflict[0] == 0) && (out->box_conflict[1] == 0) &&
        (out->box_conflict[2] == 0) && (out->box_conflict[3] == 0) &&
        (out->box_conflict[4] == 0) && (out->box_conflict[5] == 0) &&
        (out->box_conflict[6] == 0) && (out->box_conflict[7] == 0) &&
        (out->box_conflict[8] == 0);
}

int sdk_submit_state(const sdk_validation *v) {
    if (v->empty_count > 0) return 0;          /* incomplete */
    if (!v->valid_complete) return 1;          /* complete but invalid */
    return 2;                                  /* valid complete */
}

int sdk_validate_partial(const sdk_board *in) {
    sdk_validation v;
    sdk_validate(in, &v);
    return (v.row_conflict[0] | v.row_conflict[1] | v.row_conflict[2] |
            v.row_conflict[3] | v.row_conflict[4] | v.row_conflict[5] |
            v.row_conflict[6] | v.row_conflict[7] | v.row_conflict[8] |
            v.col_conflict[0] | v.col_conflict[1] | v.col_conflict[2] |
            v.col_conflict[3] | v.col_conflict[4] | v.col_conflict[5] |
            v.col_conflict[6] | v.col_conflict[7] | v.col_conflict[8] |
            v.box_conflict[0] | v.box_conflict[1] | v.box_conflict[2] |
            v.box_conflict[3] | v.box_conflict[4] | v.box_conflict[5] |
            v.box_conflict[6] | v.box_conflict[7] | v.box_conflict[8]) == 0;
}
