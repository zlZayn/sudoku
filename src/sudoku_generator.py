"""n×n 数独随机生成器"""

import argparse
import subprocess
import sys
from datetime import datetime
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

ROOT = Path(__file__).parent.parent
CORE = ROOT / "bin" / "sudoku_core.exe"


def generate(submatrix_side=3, exist_ratio=0.38):
    """生成数独，返回棋盘二维列表"""
    n = submatrix_side * submatrix_side
    result = subprocess.run(
        [str(CORE), "gen", str(n), str(exist_ratio)],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    lines = result.stdout.strip().split("\n")
    size = int(lines[0])
    board = [list(map(int, line.split())) for line in lines[1 : 1 + size]]
    return board


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="n×n 数独生成器")
    parser.add_argument(
        "--submatrix_side",
        type=int,
        default=3,
        help="子矩阵边长（默认 3，总尺寸 = 子矩阵边长²）",
    )
    parser.add_argument(
        "--exist_ratio",
        type=float,
        default=0.38,
        help="保留比例 0.05~0.95，越小越难（默认 0.38）",
    )
    args = parser.parse_args()

    k = args.submatrix_side
    n = k * k
    ratio = args.exist_ratio

    board = generate(k, ratio)
    empty = sum(row.count(0) for row in board)
    actual_ratio = round(1 - empty / (n * n), 2)

    now = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"sudoku_{n}x{n}_{actual_ratio}_{now}.md"
    path = ROOT / "output" / filename

    # 格式化棋盘
    w = len(str(n))
    lines = []
    for i, row in enumerate(board):
        if i and i % k == 0:
            parts = []
            for _ in range(k):
                parts.append("-" * (k * (w + 1) - 1))
            lines.append("-+-".join(parts))
        cells = []
        for j, v in enumerate(row):
            if j and j % k == 0:
                cells.append("|")
            cells.append(str(v).rjust(w) if v else "." + " " * (w - 1))
        lines.append(" ".join(cells))
    grid = "\n".join(lines)

    md = f"# {n}×{n} 数独（ratio={actual_ratio}，空格={empty}）\n\n```text\n{grid}\n```\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(md, encoding="utf-8")
    print(f"已生成: {path}")
