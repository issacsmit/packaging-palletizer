# Windows Release Package

This folder contains a prebuilt Windows executable and the data files required to run it.

## Run

Double-click:

```text
palletizer.exe
```

Or run from a terminal in this folder:

```bat
palletizer.exe
```

The program reads:

```text
data\materials.csv
data\orders.csv
```

After calculation, generated files are written to:

```text
output\stacking_result.csv
output\package_summary.csv
output\run_stats.txt
```

When prompted, enter a package id such as `1` to open the pallet layout window. Enter `0` to exit.
