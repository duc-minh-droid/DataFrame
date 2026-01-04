#include "DataFrame.h"
#include "DataFrameView.h"
#include "GroupedDataFrame.h"
#include "CsvReader.h"
#include <iostream>
#include "ColumnIO.h"
#include "Benchmark.h"

int main() {
    constexpr size_t N = 1'000'000;
    std::cout << "Building DataFrame with " << N << " rows...\n";

    DataFrame df = make_big_df(N);

    // ---------------- SELECT ----------------
    {
        Timer t;
        auto v = df.select({ "price", "signal" });
        std::cout << "select(): " << t.elapsed_ms() << " ms\n";
    }

    // ---------------- HEAD ----------------
    {
        Timer t;
        auto h = df.select({ "price", "volume" }).head(1000);
        std::cout << "select + head(): " << t.elapsed_ms() << " ms\n";
    }

    // ---------------- FILTER ----------------
    {
        Timer t;
        auto f = df
            .select({ "price", "signal" })
            .filter("signal", [](double x) { return x == 1; });
        std::cout << "select + filter(): " << t.elapsed_ms() << " ms\n";
    }

    // ---------------- GROUPBY COUNT ----------------
    {
        Timer t;
        auto g = df.groupby("signal").count();
        std::cout << "groupby + count(): " << t.elapsed_ms() << " ms\n";
    }

    // ---------------- GROUPBY MEAN ----------------
    {
        Timer t;
        auto m = df.groupby("signal").mean("price");
        std::cout << "groupby + mean(): " << t.elapsed_ms() << " ms\n";
    }

    // ---------------- CHAINED PIPELINE ----------------
    {
        Timer t;
        auto res = df
            .select({ "price", "signal" })
            .filter("signal", [](double x) { return x != 0; })
            .groupby("signal")
            .mean("price");

        std::cout << "select + filter + groupby + mean(): "
            << t.elapsed_ms() << " ms\n";
    }

    return 0;
}