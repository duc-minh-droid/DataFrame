#include "DataFrame.h"
#include "DataFrameView.h"
#include "GroupedDataFrame.h"
#include "CsvReader.h"
#include <iostream>
#include "ColumnIO.h"
#include "Benchmark.h"
#include "Pipeline.h"
#include <cstdlib>
#include <fstream>
#include <string>

static void usage() {
    std::cout <<
        "usage:\n"
        "  DataFrame                       run the 1M-row benchmark\n"
        "  DataFrame --bench               same\n"
        "  DataFrame --run   \"<pipeline>\"  run a pipeline, print every step\n"
        "  DataFrame --trace \"<pipeline>\"  run a pipeline, print a JSON trace\n"
        "options:\n"
        "  --data-dir <dir>   where load:/join: files are read from (default: .)\n"
        "  -o <file>          write the trace to a file instead of stdout\n"
        "pipeline example:\n"
        "  \"load:sales.csv | filter:units>=5 | sort:price:desc | groupby:region:count,mean(price)\"\n"
        "steps: load:<csv>  filter:<col><op><value>  select:<a,b,..>  sort:<col>[:desc]\n"
        "       head:<n>  groupby:<key>:<count|fn(col),..>  join:<csv>:<key>  describe\n";
}

static int runBenchmark();

int main(int argc, char** argv) {
    std::string mode = "--bench", pipe, data_dir, out_file;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { usage(); std::exit(2); }
            return argv[++i];
        };
        if (a == "--trace" || a == "--run") { mode = a; pipe = next(); }
        else if (a == "--bench") mode = a;
        else if (a == "--data-dir") data_dir = next();
        else if (a == "-o") out_file = next();
        else { usage(); return a == "--help" || a == "-h" ? 0 : 2; }
    }
    if (mode == "--bench") return runBenchmark();

    pipeline::Runner runner(data_dir);
    pipeline::Result res = runner.run(pipe);

    if (mode == "--trace") {
        if (out_file.empty()) {
            pipeline::writeTraceJson(std::cout, res);
        }
        else {
            std::ofstream f(out_file, std::ios::binary);
            if (!f) { std::cerr << "cannot write " << out_file << "\n"; return 1; }
            pipeline::writeTraceJson(f, res);
        }
        if (!res.ok) std::cerr << "error: " << res.steps.back().error << "\n";
        return res.ok ? 0 : 1;
    }

    // --run: human readable summary of every step, then the final table.
    for (size_t i = 0; i < res.steps.size(); ++i) {
        const auto& s = res.steps[i];
        std::cout << "[" << i << "] " << s.text << "\n";
        if (!s.error.empty()) {
            std::cerr << "error: " << s.error << "\n";
            return 1;
        }
        std::cout << "    " << s.cpp << "   (" << s.rows_in << " -> " << s.rows_out << " rows)\n";
    }
    if (const DataFrameView* v = runner.current()) {
        std::cout << "\n" << *v;
    }
    return 0;
}

static int runBenchmark() {
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

    // ---------------- SORT ----------------
    {
        Timer t;
        auto s = df.select({ "price" }).sort("price");
        std::cout << "select + sort(): " << t.elapsed_ms() << " ms\n";
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

    // ---------------- DESCRIBE ----------------
    {
        Timer t;
        auto d = df.view().describe();
        std::cout << "describe(): " << t.elapsed_ms() << " ms\n";
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
