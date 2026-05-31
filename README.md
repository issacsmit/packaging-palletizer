# 包装码垛算法演示程序

这个项目是一个用 C++ 写的包装码垛小程序。它会读取卷烟物料尺寸和来料顺序，自动计算每个订单应该怎么码垛，并生成结果文件。程序还可以输入码垛号，打开窗口查看对应的垛型图。

如果你只是想看效果，不需要安装开发环境，直接运行仓库里的 Windows 版本即可。

## 不会用 GitHub 和终端也可以运行

如果你只是想把项目下载到电脑上运行，按下面步骤做即可。

1. 打开项目网页：

```text
https://github.com/issacsmit/packaging-palletizer
```

2. 在网页右上方找到绿色的 `Code` 按钮，点击它。

3. 在弹出的菜单里选择 `Download ZIP`。

4. 下载完成后，找到这个压缩包：

```text
packaging-palletizer-main.zip
```

5. 右键这个压缩包，选择“全部解压缩”或“解压到当前文件夹”。

6. 打开解压后的文件夹，一般名字类似：

```text
packaging-palletizer-main
```

7. 继续打开：

```text
dist
windows
```

8. 双击运行：

```text
palletizer.exe
```

9. 程序运行后会出现一个黑色窗口。计算完成后，它会让你输入码垛号。

可以输入：

```text
1
```

然后按回车，就会打开第 1 个码垛的图形窗口。关闭图形窗口后，还可以继续输入其他码垛号。输入 `0` 后按回车退出程序。

如果 Windows 提示“无法确认发布者”或“Windows 已保护你的电脑”，这是因为这个程序不是商店软件。可以点击“更多信息”，再点击“仍要运行”。

## 会用命令行的话

也可以用 Git 克隆项目：

```bash
git clone https://github.com/issacsmit/packaging-palletizer.git
cd packaging-palletizer
```

然后打开这个目录：

```text
dist\windows
```

运行：

```text
palletizer.exe
```

程序计算完成后，会提示输入码垛号。输入 `1`、`2` 等数字可以查看对应垛型图，输入 `0` 退出。

## 运行后会得到什么

直接运行 `dist\windows\palletizer.exe` 后，结果会生成在：

```text
dist\windows\output
```

主要有 3 个文件：

```text
stacking_result.csv    每条烟的码垛明细
package_summary.csv    每个码垛的汇总信息
run_stats.txt          程序运行统计和规则校验结果
```

其中 `stacking_result.csv` 包含题目要求的核心字段，例如：

- 码垛号
- 机械手抓取顺序号
- 订单顺序号
- 来料顺序号
- 卷烟名称
- 抓取数量
- 放置坐标和尺寸

## 从源码编译

如果你想修改代码或自己重新编译，需要 Windows 和 `g++`。

先确认电脑上能找到 `g++`：

```bat
g++ --version
```

然后在项目根目录运行：

```bat
build.bat
```

编译成功后会生成：

```text
bin\palletizer.exe
```

运行方式：

```bat
bin\palletizer.exe
```

源码编译版会读取根目录下的：

```text
data\materials.csv
data\orders.csv
```

输出结果会生成到：

```text
output
```

## 项目目录说明

```text
.
├── src\main.cpp                 C++ 源码
├── data\materials.csv           物料尺寸数据
├── data\orders.csv              来料顺序数据
├── build.bat                    编译脚本
├── dist\windows\palletizer.exe  可直接运行的 Windows 程序
└── dist\windows\data            可执行程序配套数据
```

## 程序做了什么

程序按照来料顺序逐条处理卷烟，不能打乱订单顺序，也不能把不同订单混到同一个包装里。

计算时会遵守这些规则：

- 码垛空间宽 `440mm`、高 `140mm`。
- 左右边界至少保留 `5mm`。
- 机械手每次可以抓 `1` 条或 `2` 条。
- 只有相邻两条烟高度差不超过 `1mm` 时，才允许一次抓 `2` 条。
- 不同次抓取之间至少留 `5mm` 间距。
- 条烟下方悬空部分不能超过 `20mm`。
- 一个订单太多时，可以拆成多个包装。

## 当前样例运行结果

使用仓库内置数据运行后，当前结果为：

```text
物料数量: 36
来料行数: 341
展开条烟: 393
码垛数量: 46
机械手抓取次数: 336
校验结果: 通过
```

## 数据格式

`data/materials.csv` 的字段是：

```text
物料编号,物料名称,长(0.1mm),宽(0.1mm),高(0.1mm)
```

`data/orders.csv` 的字段是：

```text
订单顺序号,来料顺序号,物料编号,物料名称,来料数量
```

尺寸单位是 `0.1mm`。例如 `1020` 表示 `102.0mm`。

## 备注

课程要求文档、原始 Excel 附件、视频、示例图片和本地提交用压缩包没有放进 GitHub 仓库。仓库里保留的是其他人运行和二次开发需要的源码、CSV 数据、构建脚本和 Windows 可执行文件。
