"""n×n 数独求解器"""

import copy
import math
import subprocess
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

CORE_EXE = Path(__file__).parent.parent / "bin" / "sudoku_core.exe"


class Sudoku:
    def __init__(self, board):
        self.N = len(board)
        self.board = [row[:] for row in board]
        sr = int(math.isqrt(self.N))
        while sr > 1 and self.N % sr != 0:
            sr -= 1
        self.SUB_R, self.SUB_C = sr, self.N // sr

    # -- 合法性 --

    def is_valid(self, r, c, num):
        for i in range(self.N):
            if self.board[r][i] == num or self.board[i][c] == num:
                return False
        sr, sc = (r // self.SUB_R) * self.SUB_R, (c // self.SUB_C) * self.SUB_C
        for i in range(sr, sr + self.SUB_R):
            for j in range(sc, sc + self.SUB_C):
                if self.board[i][j] == num:
                    return False
        return True

    def validate(self):
        """检查题目是否合法，返回 (ok, 错误信息)"""
        for r in range(self.N):
            for c in range(self.N):
                v = self.board[r][c]
                if v == 0:
                    continue
                if v < 1 or v > self.N:
                    return False, f"({r},{c}) 值 {v} 超出范围 1~{self.N}"
                self.board[r][c] = 0
                if not self.is_valid(r, c, v):
                    self.board[r][c] = v
                    return False, f"({r},{c}) 值 {v} 与同行/列/宫冲突"
                self.board[r][c] = v
        return True, "合法"

    # -- 求解 --

    def _candidates(self, r, c):
        return [n for n in range(1, self.N + 1) if self.is_valid(r, c, n)]

    def _mrv_cell(self):
        best, best_cands = None, None
        for r in range(self.N):
            for c in range(self.N):
                if self.board[r][c] == 0:
                    cands = self._candidates(r, c)
                    if len(cands) == 0:
                        return None
                    if best_cands is None or len(cands) < len(best_cands):
                        best, best_cands = (r, c), cands
                        if len(cands) == 1:
                            return best, best_cands
        return (best, best_cands) if best else (None, None)

    def solve(self):
        result = self._mrv_cell()
        if result is None:
            return False
        cell, cands = result
        if cell is None:
            return True
        r, c = cell
        for n in cands:
            self.board[r][c] = n
            if self.solve():
                return True
            self.board[r][c] = 0
        return False

    def find_all(self, limit=5):
        solutions = []
        self._collect(solutions, limit)
        return solutions

    def _collect(self, solutions, limit):
        if len(solutions) >= limit:
            return
        result = self._mrv_cell()
        if result is None:
            return
        cell, cands = result
        if cell is None:
            solutions.append(self.copy())
            return
        r, c = cell
        for n in cands:
            self.board[r][c] = n
            self._collect(solutions, limit)
            self.board[r][c] = 0

    # -- 工具 --

    def copy(self):
        return copy.deepcopy(self.board)

    def __str__(self):
        w = len(str(self.N))
        lines = []
        for i, row in enumerate(self.board):
            if i and i % self.SUB_R == 0:
                parts = []
                for k in range(self.N // self.SUB_C):
                    parts.append("-" * (self.SUB_C * (w + 1) - 1))
                lines.append("-+-".join(parts))
            cells = []
            for j, v in enumerate(row):
                if j and j % self.SUB_C == 0:
                    cells.append("|")
                cells.append(str(v).rjust(w) if v else "." + " " * (w - 1))
            lines.append(" ".join(cells))
        return "\n".join(lines)


# ── 输入解析 ──


def parse_grid(text):
    """从网格文本解析棋盘（. 或 0 表示空格，| 列分隔，-+ 行分隔）"""
    rows = []
    for line in text.strip().splitlines():
        line = line.strip()
        if not line or "-" in line or "+" in line:
            continue
        line = line.replace("|", " ").replace(".", "0")
        parts = line.split()
        if all(p.isdigit() for p in parts):
            rows.append([int(p) for p in parts])
    if not rows:
        raise ValueError("未找到有效的数字行")
    # 首行仅 1 个数字且后续行更长，视为尺寸行，跳过
    if len(rows) > 1 and len(rows[0]) == 1 and len(rows[1]) > 1:
        rows = rows[1:]
    n = len(rows)
    if any(len(r) != n for r in rows):
        raise ValueError(f"应为 {n}x{n} 矩阵，但某行长度不对")
    return rows


def read_input(source=None):
    """读取棋盘，支持矩阵字面量、文件、stdin"""
    if source:
        s = source.strip()
        if s.startswith("["):
            return _parse_matrix(s)
        text = open(source, encoding="utf-8").read()
    elif not sys.stdin.isatty():
        text = sys.stdin.read()
    else:
        raise SystemExit("用法: sudoku_solver.py <文件|矩阵>")
    # 文本中可能含矩阵字面量（如 md 代码块内）
    for line in text.strip().splitlines():
        line = line.strip()
        if line.startswith("["):
            try:
                return _parse_matrix(line)
            except Exception:
                pass
    return parse_grid(text)


def _parse_matrix(s):
    import ast

    board = ast.literal_eval(s)
    if not isinstance(board, list) or not all(isinstance(r, list) for r in board):
        raise ValueError("矩阵格式应为 [[0,3,...],[...],...]")
    return board


# ── C 核心加速 ──


def solve_with_core(board, limit=6):
    """调用 C 核心求解，返回解列表。C 不可用时返回 None。"""
    if not CORE_EXE.exists():
        return None
    n = len(board)
    inp = f"{n}\n" + "\n".join(" ".join(str(v) for v in row) for row in board)
    try:
        result = subprocess.run(
            [str(CORE_EXE), "solve"],
            input=inp,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
    except OSError:
        return None
    if result.returncode != 0:
        return None
    lines = result.stdout.strip().split("\n")
    if not lines:
        return None
    try:
        int(lines[0])
    except ValueError:
        return None
    # 解析网格行：跳过分隔线（含 - 或 +），从含 | 的行中提取数字
    solutions = []
    sol = []
    for line in lines[1:]:
        stripped = line.strip()
        if not stripped:
            if len(sol) == n:
                solutions.append(sol)
            sol = []
            continue
        if "-" in stripped or "+" in stripped:
            continue
        # 去掉 | 分隔符，提取数字和 .
        tokens = stripped.replace("|", " ").split()
        row = []
        for t in tokens:
            if t == ".":
                row.append(0)
            elif t.isdigit():
                row.append(int(t))
        if len(row) == n:
            sol.append(row)
    if len(sol) == n:
        solutions.append(sol)
    return solutions


# ── 命令行入口 ──

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="n×n 数独求解器")
    parser.add_argument("puzzle", nargs="?", help="矩阵字面量或文件路径")
    parser.add_argument("--max", type=int, default=5, help="最多显示解数（默认 5）")
    args = parser.parse_args()

    try:
        board = read_input(args.puzzle)
    except Exception as e:
        print(f"输入解析失败: {e}")
        sys.exit(1)

    s = Sudoku(board)

    ok, msg = s.validate()
    if not ok:
        print(f"题目不合法: {msg}")
        sys.exit(1)

    empty = sum(row.count(0) for row in board)
    print(f"[{s.N}x{s.N}] 空格: {empty}\n")

    max_show = args.max

    # 优先调 C 核心，不可用时回退纯 Python
    solutions = solve_with_core(board, limit=max_show + 1)
    if solutions is not None:
        total = len(solutions)
    else:
        solutions = s.find_all(limit=max_show + 1)
        total = len(solutions)

    if total == 0:
        print("无解")
    elif total == 1:
        print("唯一解:")
        print(Sudoku(solutions[0]))
    else:
        label = (
            f"{total} 个解"
            if total <= max_show
            else f">{max_show} 个解（显示前 {max_show} 个）"
        )
        print(f"多解: {label}")
        for i, sol in enumerate(solutions[:max_show]):
            print(f"\n--- 解 {i + 1} ---")
            print(Sudoku(sol))
