# 数独工具

n×n 数独求解与生成。生成器输出 `.md` 文件，求解器可直接读取。

## 编译 C 核心

如果有 GCC（如 MinGW / MSYS2），直接编译：

```bash
gcc -O2 -o bin/sudoku_core.exe src/sudoku_core.c
```

或者使用 Zig 编译器，下载解压到项目根目录：

```bash
winget install zig.zig
```

或手动下载（以 0.16.0 为例，改版本号见 <https://ziglang.org/download/>）：

```bash
curl -L https://ziglang.org/builds/zig-windows-x86_64-0.16.0.zip -o zig.zip
tar -xf zig.zip
mv zig-windows-x86_64-0.16.0 zig
rm zig.zip
```

Zig 编译：

```bash
zig/zig.exe cc -O2 -o bin/sudoku_core.exe src/sudoku_core.c
```

## 生成器

### 原理

先填满一个合法棋盘（`--attempts` 控制不同种子的尝试次数），再逐格挖空并校验唯一解（`--removal_passes` 控制每轮扫描移除的轮数）。结果受随机性影响，参数大只扩大搜索空间，不保证更优。

### 生成示例

```bash
python src/sudoku_generator.py                                  # 默认 9×9
python src/sudoku_generator.py --submatrix_side 2               # 4×4
python src/sudoku_generator.py --exist_ratio 0.28               # 更难
python src/sudoku_generator.py --submatrix_side 4 --exist_ratio 0.28  # 16×16 困难
python src/sudoku_generator.py --exist_ratio 0.2 --attempts 100 # 低 ratio 多尝试
python src/sudoku_generator.py --exist_ratio 0.2 --attempts 200 --removal_passes 200  # 极限压低
python src/sudoku_generator.py --submatrix_side 2 --attempts 5  # 4×4 快速生成
```

### 生成器参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `--submatrix_side` | int | 3 | 子矩阵边长，总尺寸 = 边长² |
| `--exist_ratio` | float | 0.38 | 保留比例，越小越难 |
| `--attempts` | int | 50 | 生成尝试次数，越多越可能达到目标 ratio |
| `--removal_passes` | int | 50 | 每次尝试的移除扫描轮数，越多移除越多格子 |

### 细节

- 题目保存到 `output/`，格式为 `.md`
- 每道题保证唯一解
- ratio 越低越难保证唯一解，所以极端值可能达不到。达不到时会输出警告，文件名用实际 ratio 命名

## 求解器

### 求解示例

```bash
# 直接解生成器产出的文件
python src/sudoku_solver.py output/sudoku_9x9_0.38_20260526_112727.md

# 矩阵字面量
python src/sudoku_solver.py "[[5,3,0,0,7,0,0,0,0],[6,0,0,1,9,5,0,0,0],[0,9,8,0,0,0,0,6,0],[8,0,0,0,6,0,0,0,3],[4,0,0,8,0,3,0,0,1],[7,0,0,0,2,0,0,0,6],[0,6,0,0,0,0,2,8,0],[0,0,0,4,1,9,0,0,5],[0,0,0,0,8,0,0,7,9]]"

# 管道
cat puzzle.md | python src/sudoku_solver.py

# 显示更多解（默认最多 5 个）
python src/sudoku_solver.py puzzle.md --max 999
```

### 求解器参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `puzzle` | str | stdin | 矩阵字面量或文件路径 |
| `--max` | int | 5 | 最多显示解数 |

### 输入格式

**矩阵字面量** — Python 列表，`0` 表示空格：

```text
[[5,3,0,0,7,0,0,0,0],[6,0,0,1,9,5,0,0,0],[0,9,8,0,0,0,0,6,0],[8,0,0,0,6,0,0,0,3],[4,0,0,8,0,3,0,0,1],[7,0,0,0,2,0,0,0,6],[0,6,0,0,0,0,2,8,0],[0,0,0,4,1,9,0,0,5],[0,0,0,0,8,0,0,7,9]]
```

**网格格式** — `.` 或 `0` 表示空格，`|` 列分隔，`-+-` 行分隔：

```text
5 3 . | . 7 . | . . .
6 . . | 1 9 5 | . . .
. 9 8 | . . . | . 6 .
------+-------+------
8 . . | . 6 . | . . 3
4 . . | 8 . 3 | . . 1
7 . . | . 2 . | . . 6
------+-------+------
. 6 . | . . . | 2 8 .
. . . | 4 1 9 | . . 5
. . . | . 8 . | . 7 9
```

### 求解细节

- 唯一解直接打印，多解按 `--max` 上限显示，无解提示无解
- 网格格式兼容生成器产出的 `.md` 文件，可直接求解
- 支持任意 n×n 尺寸，自动检测子矩阵划分

### 库调用

```python
from src.sudoku_solver import Sudoku

s = Sudoku(board)
ok, msg = s.validate()    # 校验合法性
s.solve()                 # 求解（原地修改 board）
print(s)                  # 打印
solutions = s.find_all()  # 所有解（上限 5）
```
