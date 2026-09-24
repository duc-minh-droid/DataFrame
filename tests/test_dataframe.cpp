// Small self-contained test runner (no framework needed).
// usage: dataframe_tests [data-dir]    (data-dir defaults to web/data)

#include "DataFrame.h"
#include "DataFrameView.h"
#include "GroupedDataFrame.h"
#include "CsvReader.h"
#include "../Pipeline.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int g_failed = 0, g_checks = 0;
static std::string g_data = "web/data";

#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
	std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)
#define CHECK_NEAR(a, b) CHECK(std::fabs((a) - (b)) < 1e-9)
#define CHECK_THROWS(expr) do { bool threw = false; try { (void)(expr); } \
	catch (const std::exception&) { threw = true; } CHECK(threw); } while (0)

static const char* kCsv =
	"price,volume,signal\n"
	"101.5,2000,1\n"
	"102.3,1800,0\n"
	"99.8,2500,-1\n"
	"100.0,2200,1\n";

static void test_csv_numeric() {
	DataFrame df = readCSVString(kCsv);
	CHECK(df.numRows() == 4);
	CHECK(df.numCols() == 3);
	CHECK(df.getColumnName(1) == "volume");
	CHECK_NEAR(df["price"][2], 99.8);
	CHECK(!df.isCategorical(0));
	CHECK_THROWS(readCSVString("a,b\n1,2\n3\n"));
}

static void test_csv_categorical() {
	DataFrame df = readCSVString("city,temp\r\nOslo,3\r\nCairo,31\r\nOslo,5\r\n");
	CHECK(df.getColumnName(1) == "temp"); // \r stripped from header
	CHECK(df.isCategorical(0));
	// Dictionary is sorted: Cairo=0, Oslo=1.
	CHECK(df.labels(0).size() == 2);
	CHECK(df.labels(0)[0] == "Cairo");
	CHECK_NEAR(df["city"][0], 1.0);
	CHECK(df.formatCell(0, 0.0) == "Cairo");
	CHECK_NEAR(df.encode(0, "Oslo"), 1.0);
	CHECK_NEAR(df.encode(0, "Paris"), -1.0);
}

static void test_column_stats() {
	DataFrame df = readCSVString(kCsv);
	const Column& p = df["price"];
	CHECK_NEAR(p.sum(), 403.6);
	CHECK_NEAR(p.mean(), 100.9);
	CHECK_NEAR(p.min(), 99.8);
	CHECK_NEAR(p.max(), 102.3);
	CHECK_NEAR(p.median(), (100.0 + 101.5) / 2);
	CHECK_NEAR(p.quantile(0.5), p.median());
	CHECK_NEAR(p.quantile(0.0), 99.8);
	CHECK_NEAR(p.quantile(1.0), 102.3);
	// sample std of {101.5, 102.3, 99.8, 100.0}
	double m = 100.9, ss = 0.36 + 1.96 + 1.21 + 0.81;
	CHECK_NEAR(p.stddev(), std::sqrt(ss / 3));
	(void)m;
	Column empty;
	CHECK_THROWS(empty.min());
	CHECK_THROWS(empty.median());
}

static void test_view_select_filter_head() {
	DataFrame df = readCSVString(kCsv);
	auto v = df.select({ "signal", "price" });
	CHECK(v.numCols() == 2);
	CHECK(v.getColumnName(0) == "signal");
	auto f = v.filter("signal", [](double x) { return x == 1; });
	CHECK(f.numRows() == 2);
	CHECK((f.rowIndices() == std::vector<size_t>{ 0, 3 }));
	CHECK_NEAR(f.at(1, 1), 100.0);
	CHECK(df.head(2).numRows() == 2);
	CHECK(df.head(99).numRows() == 4);
	CHECK_THROWS(df.select({ "nope" }));
	CHECK_THROWS(v.select({ "volume" })); // not in the view
}

static void test_sort() {
	DataFrame df = readCSVString(kCsv);
	auto s = df.view().sort("price");
	CHECK((s.rowIndices() == std::vector<size_t>{ 2, 3, 0, 1 }));
	auto d = df.view().sort("price", false);
	CHECK((d.rowIndices() == std::vector<size_t>{ 1, 0, 3, 2 }));
	// Stable: equal signals keep their original order.
	auto st = df.view().sort("signal", false);
	CHECK((st.rowIndices() == std::vector<size_t>{ 0, 3, 1, 2 }));
	// Sorting does not touch storage.
	CHECK_NEAR(df["price"][0], 101.5);
}

static void test_groupby() {
	DataFrame df = readCSVString(kCsv);
	auto g = df.groupby("signal");
	CHECK(g.numGroups() == 3);
	CHECK((g.keys() == std::vector<double>{ -1, 0, 1 })); // sorted
	DataFrame c = g.count();
	CHECK(c.getColumnName(1) == "Count");
	CHECK_NEAR(c["Count"][2], 2.0);
	DataFrame m = g.mean("price");
	CHECK(m.getColumnName(1) == "price_mean");
	CHECK_NEAR(m["price_mean"][2], (101.5 + 100.0) / 2);
	DataFrame a = g.agg({ { "count", "" }, { "sum", "volume" }, { "max", "price" } });
	CHECK(a.numCols() == 4);
	CHECK_NEAR(a["volume_sum"][2], 4200.0);
	CHECK_NEAR(a["price_max"][0], 99.8);

	// Group-by on a filtered view uses view positions, not frame rows.
	auto gv = df.view().filter("price", [](double x) { return x > 100; }).groupby("signal");
	CHECK(gv.numGroups() == 2);
	CHECK_NEAR(gv.count()["Count"][1], 1.0);
}

static void test_groupby_categorical() {
	DataFrame df = readCSVFile(g_data + "/sales.csv");
	DataFrame r = df.groupby("region").agg({ { "count", "" } });
	CHECK(r.isCategorical(0));
	CHECK(r.formatCell(0, r[0][0]) == "East"); // alphabetical
	double total = 0;
	for (size_t i = 0; i < r.numRows(); ++i) total += r["count"][i];
	CHECK_NEAR(total, static_cast<double>(df.numRows()));
	CHECK_THROWS(df.groupby("region").mean("product"));
}

static void test_join() {
	DataFrame left = readCSVString("k,a\n1,10\n2,20\n3,30\n2,21\n");
	DataFrame right = readCSVString("k,b\n2,200\n4,400\n1,100\n2,201\n");
	std::vector<std::pair<size_t, size_t>> pairs;
	DataFrame j = left.view().join(right.view(), "k", &pairs);
	CHECK(j.numCols() == 3);
	CHECK(j.getColumnName(2) == "b");
	// left order, then right order within a key
	CHECK(j.numRows() == 5);
	CHECK((pairs == std::vector<std::pair<size_t, size_t>>{ {0, 2}, {1, 0}, {1, 3}, {3, 0}, {3, 3} }));
	CHECK_NEAR(j["b"][2], 201.0);

	// Categorical keys match by label even though dictionaries differ.
	DataFrame s = readCSVFile(g_data + "/sales.csv");
	DataFrame reg = readCSVFile(g_data + "/regions.csv");
	DataFrame sj = s.view().join(reg.view(), "region");
	for (size_t i = 0; i < sj.numRows(); ++i) {
		std::string r = sj.formatCell(sj.columnIndex("region"), sj["region"][i]);
		CHECK(r != "West"); // no West in regions.csv: inner join drops it
	}
	CHECK(sj.hasColumn("manager"));
	CHECK(sj.formatCell(sj.columnIndex("manager"), sj["manager"][0]).size() > 0);

	DataFrame clash = readCSVString("k,a\n1,5\n");
	DataFrame jc = left.view().join(clash.view(), "k");
	CHECK(jc.getColumnName(2) == "a_right");
}

static void test_describe() {
	DataFrame df = readCSVString("x,name\n1,a\n2,b\n3,c\n4,d\n");
	DataFrame d = df.view().describe();
	CHECK(d.numCols() == 2); // stat + x (name is categorical)
	CHECK(d.numRows() == 8);
	CHECK(d.formatCell(0, d[0][4]) == "25%");
	CHECK_NEAR(d["x"][0], 4.0);
	CHECK_NEAR(d["x"][1], 2.5);
	CHECK_NEAR(d["x"][4], 1.75);
	CHECK_NEAR(d["x"][7], 4.0);
}

static void test_pipeline_trace() {
	pipeline::Runner r(g_data);
	auto res = r.run("load:sales.csv | filter:units>=5 | select:region,units,price | "
		"sort:price:desc | groupby:region:count,sum(units),mean(price) | join:regions.csv:region | describe");
	CHECK(res.ok);
	CHECK(res.steps.size() == 7);
	CHECK(res.steps[1].rows_out < res.steps[1].rows_in);
	CHECK(res.steps[2].cols_out == 3);
	CHECK(res.steps[6].rows_out == 8);
	std::ostringstream os;
	pipeline::writeTraceJson(os, res);
	std::string js = os.str();
	CHECK(js.find("\"kept\":") != std::string::npos);
	CHECK(js.find("\"pairs\":") != std::string::npos);
	CHECK(js.find("\"hist\":") != std::string::npos);

	pipeline::Runner bad(g_data);
	auto err = bad.run("load:sales.csv | filter:nosuch>1");
	CHECK(!err.ok);
	CHECK(!err.steps.back().error.empty());
	pipeline::Runner bad2(g_data);
	CHECK(!bad2.run("filter:a>1").ok);
	pipeline::Runner bad3(g_data);
	CHECK(!bad3.run("load:sales.csv | filter:region>North").ok);
}

int main(int argc, char** argv) {
	if (argc > 1) g_data = argv[1];
	struct T { const char* name; void (*fn)(); };
	T tests[] = {
		{ "csv numeric", test_csv_numeric },
		{ "csv categorical", test_csv_categorical },
		{ "column stats", test_column_stats },
		{ "view select/filter/head", test_view_select_filter_head },
		{ "sort", test_sort },
		{ "groupby", test_groupby },
		{ "groupby categorical", test_groupby_categorical },
		{ "join", test_join },
		{ "describe", test_describe },
		{ "pipeline trace", test_pipeline_trace },
	};
	for (auto& t : tests) {
		int before = g_failed;
		try {
			t.fn();
		}
		catch (const std::exception& e) {
			++g_failed;
			std::cerr << "  EXCEPTION " << e.what() << "\n";
		}
		std::cout << (g_failed == before ? "ok   " : "FAIL ") << t.name << std::endl;
	}
	std::cout << g_checks << " checks, " << g_failed << " failed\n";
	return g_failed ? 1 : 0;
}
