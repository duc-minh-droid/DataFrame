# DataFrame - C++ Data Analysis Library

A lightweight, high-performance C++ library for data manipulation and analysis, inspired by pandas.

## Features

- **Fast and Efficient**: Built with performance in mind using modern C++
- **DataFrame Operations**: Select, filter, head, and more
- **Column-based Storage**: Efficient columnar data structure
- **Groupby Operations**: Group data and perform aggregations (count, sum, mean)
- **CSV Support**: Read data from CSV files or strings
- **Method Chaining**: Fluent API for data pipelines

## Quick Start

```cpp
#include "DataFrame.h"
#include "CsvReader.h"
#include <iostream>

int main() {
    // Read CSV data
    std::string csv =
        "price,volume,signal\n"
        "101.5,2000,1\n"
        "102.3,1800,0\n"
        "99.8,2500,-1\n";

    DataFrame df = readCSVString(csv);

    // Select columns
    auto view = df.select({ "price", "signal" });

    // Filter data
    auto filtered = view.filter("signal", [](double x) { return x != 0; });

    // Group by and aggregate
    auto result = df.groupby("signal").mean("price");

    // Print results
    std::cout << result << "\n";

    return 0;
}
```

## Core Components

### DataFrame
Main data structure that holds columnar data with named columns.

**Key Methods:**
- `select(columns)` - Select specific columns
- `head(n)` - Get first n rows
- `groupby(column)` - Group by column values
- `numRows()`, `numCols()` - Get dimensions
- `operator[]` - Access columns by name or index

### DataFrameView
Lightweight view into a DataFrame without copying data.

**Key Methods:**
- `select(columns)` - Select columns from view
- `filter(column, predicate)` - Filter rows based on condition
- `head(n)` - Get first n rows
- `at(row, col)` - Access specific cell

### GroupedDataFrame
Result of groupby operation, enables aggregations.

**Aggregation Methods:**
- `count()` - Count rows per group
- `sum(column)` - Sum values per group
- `mean(column)` - Calculate mean per group

### Column
Represents a single column of double values.

**Methods:**
- `sum()`, `mean()`, `min()`, `max()`, `median()` - Aggregation functions
- `operator[]` - Access values by index
- `size()` - Get number of elements

## Building the Project

### Requirements
- C++20 or later
- Visual Studio 2019/2022 (or any C++20 compatible compiler)

### Build with Visual Studio
1. Open `DataFrame.sln`
2. Select your desired configuration (Debug/Release)
3. Build the solution (F7)

### Build with CMake (if needed)
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Performance

Benchmark results on 1,000,000 rows:

| Operation | Time |
|-----------|------|
| select() | ~4.5 ms |
| select + head() | ~5.5 ms |
| select + filter() | ~53 ms |
| groupby + count() | ~179 ms |
| groupby + mean() | ~228 ms |
| Chained pipeline | ~207 ms |

## Project Structure

```
DataFrame/
??? Column.h              # Column implementation
??? DataFrame.h           # DataFrame class
??? DataFrame.cpp         # DataFrame implementation
??? DataFrameView.h       # View implementation
??? DataFrameView.cpp     # View implementation
??? GroupedDataFrame.h    # Groupby operations
??? GroupedDataFrame.cpp  # Groupby implementation
??? CsvReader.h           # CSV parsing
??? CsvReader.cpp         # CSV implementation
??? ColumnIO.h            # Print operators
??? Benchmark.h           # Benchmarking utilities
??? main.cpp              # Example usage
```

## Example: Data Pipeline

```cpp
// Read large dataset
DataFrame df = make_big_df(1'000'000);

// Chain operations
auto result = df
    .select({ "price", "signal" })
    .filter("signal", [](double x) { return x != 0; })
    .groupby("signal")
    .mean("price");

// Print formatted results
std::cout << result << "\n";
```

## CSV Reading

```cpp
// From string
std::string csv = "col1,col2\n1.0,2.0\n3.0,4.0\n";
DataFrame df = readCSVString(csv);

// From file
DataFrame df2 = readCSV("data.csv");
```

## Printing DataFrames

The library includes formatted printing for all data structures:

```cpp
std::cout << df << "\n";           // Print DataFrame
std::cout << view << "\n";         // Print DataFrameView
std::cout << df["price"] << "\n";  // Print Column
```

Output format:
```
       price        volume        signal
------------  ------------  ------------
       101.5          2000             1
       102.3          1800             0
        99.8          2500            -1
```

## License

This project is open source. Feel free to use and modify as needed.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Author

Created as a learning project to explore modern C++ and data structures.
