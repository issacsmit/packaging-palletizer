# Packaging Palletizer

一个使用 C++17 实现的卷烟包装码垛算法示例项目。程序根据物料尺寸和来料顺序，在 `440mm x 140mm` 的二维码垛空间内按顺序生成垛型，输出码垛明细、统计结果，并提供 Win32 GDI 图形窗口查看指定码垛号。

## Features

- C++17 面向对象实现，不依赖第三方 C++ 库。
- 严格按订单顺序和来料顺序处理。
- 支持机械手一次抓取 1 条或 2 条卷烟。
- 自动校验边界、抓取间距、订单隔离和悬空约束。
- 输出 CSV 明细、码垛汇总和运行统计。
- 支持输入码垛号查看垛型图。

## Repository Layout

```text
.
├── src/
│   └── main.cpp            # C++ source code
├── data/
│   ├── materials.csv       # Material size data, UTF-8 BOM CSV
│   └── orders.csv          # Incoming order data, UTF-8 BOM CSV
├── build.bat               # Windows build script
├── README.md
└── .gitignore
```

本仓库不跟踪课程要求文档、原始附件、视频、示例图片、可执行文件和运行输出；这些内容按需保留在本地。

## Clone

```bash
git clone https://github.com/issacsmit/packaging-palletizer.git
cd packaging-palletizer
```

如果仓库是私有仓库，请先完成 GitHub 认证，例如使用 GitHub CLI：

```bash
gh auth login
```

## Build

### Requirements

- Windows
- `g++`，例如 TDM-GCC / MinGW-w64

确认 `g++` 可用：

```bat
g++ --version
```

构建：

```bat
build.bat
```

生成文件：

```text
bin\palletizer.exe
```

## Run Without Building

如果只想在 Windows 上直接运行，可以使用预构建发布包：

```text
dist\windows\palletizer.exe
```

发布包已经包含运行所需资源：

```text
dist\windows\data\materials.csv
dist\windows\data\orders.csv
```

进入 `dist\windows` 后运行 `palletizer.exe`，输入码垛号即可查看垛型图。生成结果会写入 `dist\windows\output`。

## Run

在项目根目录运行：

```bat
bin\palletizer.exe
```

程序会读取：

```text
data\materials.csv
data\orders.csv
```

计算完成后生成：

```text
output\stacking_result.csv
output\package_summary.csv
output\run_stats.txt
```

随后在控制台输入码垛号即可打开垛型图窗口；关闭窗口后可继续输入其他码垛号。输入 `0` 退出。

## Output

`output/stacking_result.csv` 每条烟一行，包含：

- 码垛号
- 码垛机械手抓取顺序号
- 订单顺序号
- 来料顺序号
- 物料编号
- 卷烟名称
- 抓取数量
- 组内序号
- 坐标和宽高

`output/package_summary.csv` 每个码垛一行，包含订单号、条烟数量、抓取次数和面积利用率。

`output/run_stats.txt` 包含运行时间、码垛数量、机械手抓取次数和校验结果。

## Algorithm Notes

程序内部使用 `0.1mm` 整数单位计算，避免浮点误差。核心策略是按来料顺序将一次抓取作为放置块进行启发式摆放：优先尝试合法的双条抓取，再退化为单条抓取；当前码垛无法继续放置时创建新的码垛。放置完成后由校验器复查硬约束。

主要约束包括：

- 码垛空间宽 `440mm`、高 `140mm`。
- 宽度方向左右边界至少保留 `5mm`。
- 不同次抓取在宽度方向至少间隔 `5mm`。
- 相邻两条卷烟高度差不超过 `1mm` 时才允许一次抓取 2 条。
- 条烟下方连续悬空宽度不超过 `20mm`。
- 不同订单不混装。

## Data Format

`data/materials.csv` 字段：

```text
物料编号,物料名称,长(0.1mm),宽(0.1mm),高(0.1mm)
```

`data/orders.csv` 字段：

```text
订单顺序号,来料顺序号,物料编号,物料名称,来料数量
```
