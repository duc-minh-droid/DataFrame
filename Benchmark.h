#pragma once

#include "DataFrame.h"
#include "DataFrameView.h"
#include "GroupedDataFrame.h"
#include "CsvReader.h"

#include <iostream>
#include <chrono>
#include <random>

// Simple timer helper
struct Timer {
    std::chrono::high_resolution_clock::time_point start;
    Timer() : start(std::chrono::high_resolution_clock::now()) {}
    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

inline DataFrame make_big_df(size_t rows) {
    DataFrame df({ "price", "volume", "signal" });
    df.reserveRows(rows);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> price(90.0, 110.0);
    std::uniform_int_distribution<int> volume(1000, 5000);
    std::uniform_int_distribution<int> signal(-1, 1);

    for (size_t i = 0; i < rows; ++i) {
        df.addRow({
            price(rng),
            static_cast<double>(volume(rng)),
            static_cast<double>(signal(rng))
            });
    }
    return df;
}