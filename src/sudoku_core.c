/*
 * n×n 数独求解/生成核心（C 实现）
 * 编译: gcc -O2 -o sudoku_core.exe sudoku_core.c
 * 用法:
 *   sudoku_core.exe solve   < input.txt        # 求解（输出所有解，上限5）
 *   sudoku_core.exe gen N difficulty            # 生成题目
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXN 25
#define MAX_SOL 6

static int N, SUB_R, SUB_C;
static int board[MAXN][MAXN];
static int solutions[MAX_SOL][MAXN][MAXN];
static int sol_count;

static int row_used[MAXN][MAXN + 1];
static int col_used[MAXN][MAXN + 1];
static int box_used[MAXN][MAXN + 1];

static inline int box_id(int r, int c) {
    return (r / SUB_R) * (N / SUB_C) + c / SUB_C;
}

static void detect_sub(int n) {
    N = n;
    int sr = 1;
    for (int i = 1; i * i <= n; i++) {
        if (n % i == 0) sr = i;
    }
    SUB_R = sr;
    SUB_C = n / sr;
}

static void clear_masks(void) {
    memset(row_used, 0, sizeof(row_used));
    memset(col_used, 0, sizeof(col_used));
    memset(box_used, 0, sizeof(box_used));
}

static void set_masks_from_board(void) {
    clear_masks();
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            if (board[r][c]) {
                int v = board[r][c];
                row_used[r][v] = 1;
                col_used[c][v] = 1;
                box_used[box_id(r, c)][v] = 1;
            }
}

static inline int can_place(int r, int c, int v) {
    return !row_used[r][v] && !col_used[c][v] && !box_used[box_id(r, c)][v];
}

static inline void place(int r, int c, int v) {
    board[r][c] = v;
    row_used[r][v] = col_used[c][v] = box_used[box_id(r, c)][v] = 1;
}

static inline void unplace(int r, int c, int v) {
    board[r][c] = 0;
    row_used[r][v] = col_used[c][v] = box_used[box_id(r, c)][v] = 0;
}

/* 传播记录栈：记录 propagate 填入的格子，支持精确撤销 */
static int prop_stack[MAXN * MAXN][2];
static int prop_stack_top;

static inline void prop_push(int r, int c) {
    prop_stack[prop_stack_top][0] = r;
    prop_stack[prop_stack_top][1] = c;
    prop_stack_top++;
}

/* 约束传播: naked singles + hidden singles */
static int propagate(void) {
    int changed = 1;
    while (changed) {
        changed = 0;
        /* naked singles */
        for (int r = 0; r < N; r++) {
            for (int c = 0; c < N; c++) {
                if (board[r][c]) continue;
                int cnt = 0, last = 0;
                for (int v = 1; v <= N; v++) {
                    if (can_place(r, c, v)) { cnt++; last = v; }
                }
                if (cnt == 0) return 0;
                if (cnt == 1) { place(r, c, last); prop_push(r, c); changed = 1; }
            }
        }
        /* hidden singles: 行 */
        for (int r = 0; r < N; r++) {
            for (int v = 1; v <= N; v++) {
                if (row_used[r][v]) continue;
                int cnt = 0, last_c = -1;
                for (int c = 0; c < N; c++)
                    if (!board[r][c] && can_place(r, c, v)) { cnt++; last_c = c; }
                if (cnt == 0) return 0;
                if (cnt == 1) { place(r, last_c, v); prop_push(r, last_c); changed = 1; }
            }
        }
        /* hidden singles: 列 */
        for (int c = 0; c < N; c++) {
            for (int v = 1; v <= N; v++) {
                if (col_used[c][v]) continue;
                int cnt = 0, last_r = -1;
                for (int r = 0; r < N; r++)
                    if (!board[r][c] && can_place(r, c, v)) { cnt++; last_r = r; }
                if (cnt == 0) return 0;
                if (cnt == 1) { place(last_r, c, v); prop_push(last_r, c); changed = 1; }
            }
        }
        /* hidden singles: 宫 */
        for (int br = 0; br < N; br += SUB_R) {
            for (int bc = 0; bc < N; bc += SUB_C) {
                int bid = box_id(br, bc);
                for (int v = 1; v <= N; v++) {
                    if (box_used[bid][v]) continue;
                    int cnt = 0, lr = -1, lc = -1;
                    for (int i = 0; i < SUB_R; i++)
                        for (int j = 0; j < SUB_C; j++) {
                            int rr = br + i, cc = bc + j;
                            if (!board[rr][cc] && can_place(rr, cc, v)) {
                                cnt++; lr = rr; lc = cc;
                            }
                        }
                    if (cnt == 0) return 0;
                    if (cnt == 1) { place(lr, lc, v); prop_push(lr, lc); changed = 1; }
                }
            }
        }
    }
    return 1;
}

/* 撤销 propagate 填入的格子，回退到 level 指定的栈位置 */
static void undo_propagate(int level) {
    while (prop_stack_top > level) {
        prop_stack_top--;
        int r = prop_stack[prop_stack_top][0];
        int c = prop_stack[prop_stack_top][1];
        int v = board[r][c];
        unplace(r, c, v);
    }
}

/* MRV 选格 */
static int find_mrv(int *or, int *oc, int *cands, int *nc) {
    int best = N + 1;
    *or = -1;
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            if (board[r][c]) continue;
            int cnt = 0;
            for (int v = 1; v <= N; v++)
                if (can_place(r, c, v)) cnt++;
            if (cnt == 0) return 0;
            if (cnt < best) {
                best = cnt;
                *or = r; *oc = c;
                *nc = 0;
                for (int v = 1; v <= N; v++)
                    if (can_place(r, c, v)) cands[(*nc)++] = v;
                if (cnt == 1) return 1;
            }
        }
    }
    return *or >= 0;
}

static int snap_board[MAXN][MAXN];
static int snap_row[MAXN][MAXN + 1];
static int snap_col[MAXN][MAXN + 1];
static int snap_box[MAXN][MAXN + 1];

static void snapshot(void) {
    memcpy(snap_board, board, sizeof(board));
    memcpy(snap_row, row_used, sizeof(row_used));
    memcpy(snap_col, col_used, sizeof(col_used));
    memcpy(snap_box, box_used, sizeof(box_used));
}

static void restore(void) {
    memcpy(board, snap_board, sizeof(board));
    memcpy(row_used, snap_row, sizeof(row_used));
    memcpy(col_used, snap_col, sizeof(col_used));
    memcpy(box_used, snap_box, sizeof(box_used));
}

static void solve_count(int limit) {
    if (sol_count >= limit) return;
    /* 记录传播栈位置，传播失败时回退到此处 */
    int level = prop_stack_top;
    if (!propagate()) { undo_propagate(level); return; }
    /* 找空格（MRV） */
    int best_r = -1, best_c = -1, cands[MAXN], nc = 0;
    int best_cnt = N + 1;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            if (board[r][c]) continue;
            int cnt = 0;
            for (int v = 1; v <= N; v++)
                if (can_place(r, c, v)) cnt++;
            if (cnt == 0) { undo_propagate(level); return; }
            if (cnt < best_cnt) {
                best_cnt = cnt; best_r = r; best_c = c;
                nc = 0;
                for (int v = 1; v <= N; v++)
                    if (can_place(r, c, v)) cands[nc++] = v;
                if (cnt == 1) break;
            }
        }
    if (best_r < 0) {
        /* 无空格，保存解 */
        if (sol_count < limit)
            memcpy(solutions[sol_count++], board, sizeof(board));
        undo_propagate(level);
        return;
    }
    /* 逐个候选试探 */
    for (int i = 0; i < nc && sol_count < limit; i++) {
        place(best_r, best_c, cands[i]);
        solve_count(limit);
        unplace(best_r, best_c, cands[i]);
    }
    /* 撤销本轮传播 */
    undo_propagate(level);
}

/* 打印棋盘 */
static void print_board(int b[MAXN][MAXN]) {
    int w = 1;
    if (N > 9) w = 2;
    for (int i = 0; i < N; i++) {
        if (i && i % SUB_R == 0) {
            for (int k = 0; k < N / SUB_C; k++) {
                if (k) printf("-+-");
                for (int j = 0; j < SUB_C * (w + 1) - 1; j++) putchar('-');
            }
            putchar('\n');
        }
        for (int j = 0; j < N; j++) {
            if (j && j % SUB_C == 0) printf(" | ");
            if (b[i][j])
                printf("%*d", w, b[i][j]);
            else {
                printf(".");
                for (int k = 1; k < w; k++) putchar(' ');
            }
            if (j % SUB_C < SUB_C - 1) putchar(' ');
        }
        putchar('\n');
    }
}

/* 读取题目 */
static int read_puzzle(void) {
    if (scanf("%d", &N) != 1) return 0;
    detect_sub(N);
    clear_masks();
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            scanf("%d", &board[r][c]);
            if (board[r][c]) {
                row_used[r][board[r][c]] = 1;
                col_used[c][board[r][c]] = 1;
                box_used[box_id(r, c)][board[r][c]] = 1;
            }
        }
    return 1;
}

/* 生成完整解 */
static void fill_board(void) {
    int r, c, cands[MAXN], nc;
    if (!find_mrv(&r, &c, cands, &nc)) return;
    if (r < 0) return;
    /* 随机顺序 */
    for (int i = nc - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = cands[i]; cands[i] = cands[j]; cands[j] = t;
    }
    for (int i = 0; i < nc; i++) {
        place(r, c, cands[i]);
        fill_board();
        /* 检查是否填满 */
        int full = 1;
        for (int rr = 0; rr < N && full; rr++)
            for (int cc = 0; cc < N && full; cc++)
                if (!board[rr][cc]) full = 0;
        if (full) return;
        unplace(r, c, cands[i]);
    }
}

static int gen_main(int n, double ratio) {
    srand((unsigned)time(NULL));
    detect_sub(n);
    clear_masks();

    /* 第一行随机排列 */
    for (int c = 0; c < N; c++) board[0][c] = c + 1;
    for (int c = N - 1; c > 0; c--) {
        int j = rand() % (c + 1);
        int t = board[0][c]; board[0][c] = board[0][j]; board[0][j] = t;
    }
    for (int c = 0; c < N; c++) {
        row_used[0][board[0][c]] = 1;
        col_used[c][board[0][c]] = 1;
        box_used[box_id(0, c)][board[0][c]] = 1;
    }
    fill_board();

    /* 检查是否生成成功 */
    int full = 1;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            if (!board[r][c]) full = 0;
    if (!full) return 1;

    /* 保存完整解 */
    int full_sol[MAXN][MAXN];
    memcpy(full_sol, board, sizeof(board));

    /* 随机移除格子，保证唯一解 */
    /* ratio: 保留比例，如 0.38 表示保留 38% 格子 */
    if (ratio < 0.05) ratio = 0.05;
    if (ratio > 0.95) ratio = 0.95;
    int total = N * N;
    int target_keep = (int)(total * ratio);
    if (target_keep < N) target_keep = N;
    int to_remove = total - target_keep;

    /* 多轮尝试移除，每轮随机顺序 */
    int removed = 0;
    int cells[MAXN * MAXN][2];
    for (int round = 0; round < 10 && removed < to_remove; round++) {
        int idx = 0;
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                if (board[r][c]) { cells[idx][0] = r; cells[idx][1] = c; idx++; }
        for (int i = idx - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int tr = cells[i][0], tc = cells[i][1];
            cells[i][0] = cells[j][0]; cells[i][1] = cells[j][1];
            cells[j][0] = tr; cells[j][1] = tc;
        }
        for (int i = 0; i < idx && removed < to_remove; i++) {
            int r = cells[i][0], c = cells[i][1];
            if (!board[r][c]) continue;
            int orig = board[r][c];
            board[r][c] = 0;
            set_masks_from_board();
            sol_count = 0;
            solve_count(2);
            if (sol_count == 1) {
                removed++;
            } else {
                board[r][c] = orig;
            }
        }
    }

    /* 移除不足时警告 */
    if (removed < to_remove) {
        double actual = (double)(total - removed) / total;
        fprintf(stderr, "警告: 目标 ratio=%.2f（移除 %d 格），实际 ratio=%.2f（移除 %d 格）\n",
                ratio, to_remove, actual, removed);
    }

    /* 输出 */
    printf("%d\n", N);
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            if (c) putchar(' ');
            printf("%d", board[r][c]);
        }
        putchar('\n');
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "用法: sudoku_core solve < input | sudoku_core gen N ratio\n");
        return 1;
    }
    if (strcmp(argv[1], "solve") == 0) {
        if (!read_puzzle()) return 1;
        set_masks_from_board();
        sol_count = 0;
        solve_count(MAX_SOL);
        printf("%d\n", sol_count);
        for (int i = 0; i < sol_count; i++) {
            if (i) putchar('\n');
            print_board(solutions[i]);
        }
    } else if (strcmp(argv[1], "gen") == 0) {
        int n = argc > 2 ? atoi(argv[2]) : 9;
        double ratio = argc > 3 ? atof(argv[3]) : 0.38;
        return gen_main(n, ratio);
    }
    return 0;
}
