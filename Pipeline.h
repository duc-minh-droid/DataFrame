#pragma once
// Text pipelines + JSON trace output.
//
// A pipeline is a '|' separated list of steps, for example
//
//   load:sales.csv | filter:units>=5 | select:region,units,price
//     | sort:price:desc | groupby:region:count,sum(units),mean(price)
//     | join:regions.csv:region | describe | head:10
//
// run() executes it with the normal DataFrame / DataFrameView API and
// records, for every step, the table before and after, the view's internal
// index vectors and an op-specific row/column mapping. writeTraceJson()
// turns that into the JSON the web visualizer animates.

#include "DataFrame.h"
#include "DataFrameView.h"
#include "GroupedDataFrame.h"
#include "CsvReader.h"
#include "Benchmark.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace pipeline {

// ------------------------------------------------------------------ JSON --

inline std::string jsonString(const std::string& s) {
	std::string out = "\"";
	for (unsigned char ch : s) {
		switch (ch) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (ch < 0x20) {
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
				out += buf;
			}
			else {
				out += static_cast<char>(ch);
			}
		}
	}
	return out + "\"";
}

// Shortest decimal that parses back to the same double (like JS / Python repr).
inline std::string jsonNumber(double v) {
	if (!std::isfinite(v)) return "null";
	char buf[40];
	for (int prec = 1; prec <= 17; ++prec) {
		std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
		if (std::strtod(buf, nullptr) == v) break;
	}
	return buf;
}

template <typename T, typename F>
std::string jsonArray(const std::vector<T>& items, F each) {
	std::string out = "[";
	for (size_t i = 0; i < items.size(); ++i) {
		if (i) out += ",";
		out += each(items[i]);
	}
	return out + "]";
}

inline std::string jsonSizes(const std::vector<size_t>& v) {
	return jsonArray(v, [](size_t x) { return std::to_string(x); });
}

inline std::string jsonNames(const std::vector<std::string>& v) {
	return jsonArray(v, [](const std::string& x) { return jsonString(x); });
}

// ------------------------------------------------------------- parsing --

inline std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

inline std::vector<std::string> split(const std::string& s, char sep) {
	std::vector<std::string> out;
	std::string cur;
	for (char ch : s) {
		if (ch == sep) {
			out.push_back(trim(cur));
			cur.clear();
		}
		else {
			cur += ch;
		}
	}
	out.push_back(trim(cur));
	return out;
}

struct StepSpec {
	std::string op;
	std::vector<std::string> args; // ':' separated parts after the op name
	std::string text;              // original text of the step
};

inline std::vector<StepSpec> parse(const std::string& text) {
	std::vector<StepSpec> steps;
	for (const auto& part : split(text, '|')) {
		if (part.empty()) continue;
		auto pieces = split(part, ':');
		StepSpec s;
		s.op = pieces[0];
		s.args.assign(pieces.begin() + 1, pieces.end());
		s.text = part;
		steps.push_back(s);
	}
	return steps;
}

// "units>=5" -> {"units", ">=", "5"}
struct Condition {
	std::string column, op, value;
};

inline Condition parseCondition(const std::string& s) {
	static const char* ops[] = { ">=", "<=", "!=", "==", ">", "<" };
	for (const char* op : ops) {
		size_t pos = s.find(op);
		if (pos != std::string::npos && pos > 0) {
			return { trim(s.substr(0, pos)), op, trim(s.substr(pos + std::string(op).size())) };
		}
	}
	throw std::runtime_error("filter: expected <column><op><value>, got '" + s + "'");
}

inline std::vector<Aggregation> parseAggs(const std::string& s) {
	std::vector<Aggregation> aggs;
	for (const auto& a : split(s, ',')) {
		if (a == "count") {
			aggs.push_back({ "count", "" });
			continue;
		}
		size_t open = a.find('('), close = a.find(')');
		if (open == std::string::npos || close == std::string::npos || close < open) {
			throw std::runtime_error("groupby: expected fn(column), got '" + a + "'");
		}
		aggs.push_back({ trim(a.substr(0, open)), trim(a.substr(open + 1, close - open - 1)) });
	}
	return aggs;
}

// ------------------------------------------------------------- tracing --

// Serialise a view as a table: column names/kinds, stable row ids
// ("<frame>:<row in that frame>") and the visible cell values.
inline std::string tableJson(const DataFrameView& v, size_t frame_id) {
	std::ostringstream os;
	os << "{\"frame\":" << frame_id << ",\"columns\":[";
	for (size_t c = 0; c < v.numCols(); ++c) {
		if (c) os << ",";
		os << "{\"name\":" << jsonString(v.getColumnName(c))
			<< ",\"kind\":\"" << (v.isCategorical(c) ? "cat" : "num") << "\"}";
	}
	os << "],\"ids\":[";
	const auto& rows = v.rowIndices();
	for (size_t r = 0; r < rows.size(); ++r) {
		if (r) os << ",";
		os << "\"" << frame_id << ":" << rows[r] << "\"";
	}
	os << "],\"rows\":[";
	for (size_t r = 0; r < v.numRows(); ++r) {
		if (r) os << ",";
		os << "[";
		for (size_t c = 0; c < v.numCols(); ++c) {
			if (c) os << ",";
			if (v.isCategorical(c)) os << jsonString(v.formatCell(r, c));
			else os << jsonNumber(v.at(r, c));
		}
		os << "]";
	}
	os << "]}";
	return os.str();
}

// The DataFrameView internals: which frame, which rows, which columns.
inline std::string viewJson(const DataFrameView& v, size_t frame_id) {
	return "{\"frame\":" + std::to_string(frame_id) +
		",\"rowIndices\":" + jsonSizes(v.rowIndices()) +
		",\"columnIndices\":" + jsonSizes(v.columnIndices()) + "}";
}

// Physical storage of a frame: one contiguous double vector per column plus
// the dictionary for categorical columns.
inline std::string storageJson(const DataFrame& df, size_t frame_id) {
	std::ostringstream os;
	os << "{\"frame\":" << frame_id << ",\"columns\":[";
	for (size_t c = 0; c < df.numCols(); ++c) {
		if (c) os << ",";
		os << "{\"name\":" << jsonString(df.getColumnName(c))
			<< ",\"kind\":\"" << (df.isCategorical(c) ? "cat" : "num") << "\""
			<< ",\"labels\":" << jsonNames(df.labels(c))
			<< ",\"data\":" << jsonArray(df[c].data(), [](double x) { return jsonNumber(x); })
			<< "}";
	}
	os << "]}";
	return os.str();
}

inline std::string cppLiteral(const std::string& s) {
	return "\"" + s + "\"";
}

struct Step {
	std::string op, text, cpp;
	std::string before;  // table JSON or "null"
	std::string after;   // table JSON
	std::string view;    // view internals JSON
	std::string map;     // op-specific JSON object
	std::string storage; // storage JSON or "null" (only when a new frame appears)
	std::string error;
	double micros = 0;
	size_t rows_in = 0, rows_out = 0, cols_in = 0, cols_out = 0;
};

struct Result {
	std::string pipeline;
	std::vector<Step> steps;
	bool ok = true;
};

class Runner {
	std::string data_dir_;
	std::deque<DataFrame> frames_; // deque: stable addresses for views
	std::optional<DataFrameView> cur_;
	size_t cur_frame_ = 0;

	std::string path(const std::string& file) const {
		if (data_dir_.empty()) return file;
		char last = data_dir_.back();
		return data_dir_ + ((last == '/' || last == '\\') ? "" : "/") + file;
	}

	size_t push(DataFrame df) {
		frames_.push_back(std::move(df));
		return frames_.size() - 1;
	}

	// Position of every row of `v` inside `before` (views never repeat rows).
	static std::vector<size_t> positionsIn(const DataFrameView& before, const DataFrameView& v) {
		std::unordered_map<size_t, size_t> pos;
		for (size_t i = 0; i < before.numRows(); ++i) pos[before.rowIndices()[i]] = i;
		std::vector<size_t> out;
		for (size_t r : v.rowIndices()) out.push_back(pos.at(r));
		return out;
	}

	void apply(const StepSpec& s, Step& st) {
		const auto& a = s.args;
		auto need = [&](size_t n) {
			if (a.size() < n) throw std::runtime_error(s.op + ": missing arguments in '" + s.text + "'");
		};

		if (s.op == "load") {
			need(1);
			Timer t;
			DataFrame df = readCSVFile(path(a[0]));
			st.micros = t.elapsed_ms() * 1000.0;
			cur_frame_ = push(std::move(df));
			cur_ = frames_[cur_frame_].view();
			st.cpp = "DataFrame df = readCSVFile(" + cppLiteral(a[0]) + ");";
			st.map = "{\"file\":" + jsonString(a[0]) + "}";
			st.storage = storageJson(frames_[cur_frame_], cur_frame_);
			return;
		}
		if (!cur_) throw std::runtime_error("pipeline must start with load:<file>");
		const DataFrameView before = *cur_;

		if (s.op == "filter") {
			need(1);
			Condition cond = parseCondition(a[0]);
			size_t col = before.getColumnIdx(cond.column);
			double rhs;
			if (before.isCategorical(col)) {
				if (cond.op != "==" && cond.op != "!=") {
					throw std::runtime_error("filter: only == and != work on categorical column '" + cond.column + "'");
				}
				rhs = before.frame().encode(before.columnIndices()[col], cond.value);
			}
			else {
				try { rhs = std::stod(cond.value); }
				catch (const std::exception&) {
					throw std::runtime_error("filter: '" + cond.value + "' is not a number");
				}
			}
			std::function<bool(double)> pred;
			const std::string& op = cond.op;
			if (op == ">=") pred = [rhs](double x) { return x >= rhs; };
			else if (op == "<=") pred = [rhs](double x) { return x <= rhs; };
			else if (op == ">") pred = [rhs](double x) { return x > rhs; };
			else if (op == "<") pred = [rhs](double x) { return x < rhs; };
			else if (op == "==") pred = [rhs](double x) { return x == rhs; };
			else pred = [rhs](double x) { return x != rhs; };

			Timer t;
			DataFrameView out = before.filter(cond.column, pred);
			st.micros = t.elapsed_ms() * 1000.0;

			auto kept = positionsIn(before, out);
			std::vector<size_t> dropped;
			for (size_t i = 0, k = 0; i < before.numRows(); ++i) {
				if (k < kept.size() && kept[k] == i) ++k;
				else dropped.push_back(i);
			}
			std::string lit = before.isCategorical(col) ? jsonString(cond.value) : jsonNumber(rhs);
			st.map = "{\"column\":" + jsonString(cond.column) + ",\"cmp\":" + jsonString(op) +
				",\"value\":" + lit + ",\"kept\":" + jsonSizes(kept) + ",\"dropped\":" + jsonSizes(dropped) + "}";
			std::string rhs_cpp = before.isCategorical(col)
				? "df.encode(" + cppLiteral(cond.column) + ", " + cppLiteral(cond.value) + ")"
				: cond.value;
			st.cpp = ".filter(" + cppLiteral(cond.column) + ", [](double x) { return x " + op + " " + rhs_cpp + "; })";
			cur_ = out;
			return;
		}
		if (s.op == "select") {
			need(1);
			auto names = split(a[0], ',');
			Timer t;
			DataFrameView out = before.select(names);
			st.micros = t.elapsed_ms() * 1000.0;
			std::vector<std::string> dropped;
			for (size_t c = 0; c < before.numCols(); ++c) {
				const auto& n = before.getColumnName(c);
				if (std::find(names.begin(), names.end(), n) == names.end()) dropped.push_back(n);
			}
			st.map = "{\"kept\":" + jsonNames(names) + ",\"dropped\":" + jsonNames(dropped) + "}";
			std::string list;
			for (size_t i = 0; i < names.size(); ++i) list += (i ? ", " : "") + cppLiteral(names[i]);
			st.cpp = ".select({ " + list + " })";
			cur_ = out;
			return;
		}
		if (s.op == "sort") {
			need(1);
			bool asc = !(a.size() > 1 && a[1] == "desc");
			Timer t;
			DataFrameView out = before.sort(a[0], asc);
			st.micros = t.elapsed_ms() * 1000.0;
			st.map = "{\"column\":" + jsonString(a[0]) + ",\"ascending\":" + (asc ? "true" : "false") +
				",\"order\":" + jsonSizes(positionsIn(before, out)) + "}";
			st.cpp = ".sort(" + cppLiteral(a[0]) + (asc ? "" : ", false") + ")";
			cur_ = out;
			return;
		}
		if (s.op == "head") {
			size_t n = a.empty() ? 5 : std::stoul(a[0]);
			Timer t;
			DataFrameView out = before.head(n);
			st.micros = t.elapsed_ms() * 1000.0;
			st.map = "{\"n\":" + std::to_string(n) + ",\"kept\":" + std::to_string(out.numRows()) + "}";
			st.cpp = ".head(" + std::to_string(n) + ")";
			cur_ = out;
			return;
		}
		if (s.op == "groupby") {
			need(2);
			auto aggs = parseAggs(a[1]);
			Timer t;
			GroupedDataFrame g = before.groupby(a[0]);
			DataFrame res = g.agg(aggs);
			st.micros = t.elapsed_ms() * 1000.0;

			size_t key_col = before.getColumnIdx(a[0]);
			std::ostringstream os;
			os << "{\"key\":" << jsonString(a[0]) << ",\"groups\":[";
			for (size_t i = 0; i < g.numGroups(); ++i) {
				if (i) os << ",";
				const auto& rows = g.groups()[i];
				os << "{\"key\":"
					<< (before.isCategorical(key_col) ? jsonString(before.formatCell(rows[0], key_col))
						: jsonNumber(g.keys()[i]))
					<< ",\"rows\":" << jsonSizes(rows) << "}";
			}
			os << "],\"aggs\":[";
			for (size_t i = 0; i < aggs.size(); ++i) {
				if (i) os << ",";
				os << "{\"fn\":" << jsonString(aggs[i].fn) << ",\"column\":" << jsonString(aggs[i].column)
					<< ",\"output\":" << jsonString(res.getColumnName(i + 1)) << "}";
			}
			os << "]}";
			st.map = os.str();

			std::string list;
			for (size_t i = 0; i < aggs.size(); ++i) {
				list += (i ? ", " : "") + std::string("{ ") + cppLiteral(aggs[i].fn) + ", " + cppLiteral(aggs[i].column) + " }";
			}
			st.cpp = ".groupby(" + cppLiteral(a[0]) + ").agg({ " + list + " })";
			cur_frame_ = push(std::move(res));
			cur_ = frames_[cur_frame_].view();
			st.storage = storageJson(frames_[cur_frame_], cur_frame_);
			return;
		}
		if (s.op == "join") {
			need(2);
			size_t right_frame = push(readCSVFile(path(a[0])));
			DataFrameView right = frames_[right_frame].view();
			std::vector<std::pair<size_t, size_t>> pairs;
			Timer t;
			DataFrame res = before.join(right, a[1], &pairs);
			st.micros = t.elapsed_ms() * 1000.0;

			std::vector<bool> lhit(before.numRows()), rhit(right.numRows());
			for (auto& p : pairs) { lhit[p.first] = true; rhit[p.second] = true; }
			std::vector<size_t> ul, ur;
			for (size_t i = 0; i < lhit.size(); ++i) if (!lhit[i]) ul.push_back(i);
			for (size_t i = 0; i < rhit.size(); ++i) if (!rhit[i]) ur.push_back(i);

			st.map = "{\"file\":" + jsonString(a[0]) + ",\"key\":" + jsonString(a[1]) +
				",\"right\":" + tableJson(right, right_frame) +
				",\"pairs\":" + jsonArray(pairs, [](const std::pair<size_t, size_t>& p) {
					return "[" + std::to_string(p.first) + "," + std::to_string(p.second) + "]";
				}) +
				",\"unmatchedLeft\":" + jsonSizes(ul) + ",\"unmatchedRight\":" + jsonSizes(ur) + "}";
			st.cpp = ".join(readCSVFile(" + cppLiteral(a[0]) + ").view(), " + cppLiteral(a[1]) + ")";
			cur_frame_ = push(std::move(res));
			cur_ = frames_[cur_frame_].view();
			st.storage = storageJson(frames_[cur_frame_], cur_frame_);
			return;
		}
		if (s.op == "describe") {
			Timer t;
			DataFrame res = before.describe();
			st.micros = t.elapsed_ms() * 1000.0;

			// Histogram of every numeric input column, for the sparklines.
			constexpr size_t BINS = 8;
			std::ostringstream os;
			os << "{\"bins\":" << BINS << ",\"hist\":{";
			bool first = true;
			for (size_t c = 0; c < before.numCols(); ++c) {
				if (before.isCategorical(c)) continue;
				double lo = INFINITY, hi = -INFINITY;
				for (size_t r = 0; r < before.numRows(); ++r) {
					lo = std::min(lo, before.at(r, c));
					hi = std::max(hi, before.at(r, c));
				}
				std::vector<size_t> counts(BINS, 0);
				for (size_t r = 0; r < before.numRows(); ++r) {
					size_t b = hi > lo ? static_cast<size_t>((before.at(r, c) - lo) / (hi - lo) * BINS) : 0;
					counts[std::min(b, BINS - 1)]++;
				}
				if (!first) os << ",";
				first = false;
				os << jsonString(before.getColumnName(c)) << ":{\"min\":" << jsonNumber(lo)
					<< ",\"max\":" << jsonNumber(hi) << ",\"counts\":" << jsonSizes(counts) << "}";
			}
			os << "}}";
			st.map = os.str();
			st.cpp = ".describe()";
			cur_frame_ = push(std::move(res));
			cur_ = frames_[cur_frame_].view();
			st.storage = storageJson(frames_[cur_frame_], cur_frame_);
			return;
		}
		throw std::runtime_error("unknown step '" + s.op + "'");
	}

public:
	explicit Runner(std::string data_dir) : data_dir_(std::move(data_dir)) {}

	Result run(const std::string& text) {
		Result res;
		res.pipeline = text;
		for (const auto& spec : parse(text)) {
			Step st;
			st.op = spec.op;
			st.text = spec.text;
			st.before = "null";
			st.storage = "null";
			st.map = "{}";
			size_t before_frame = cur_frame_;
			bool had = cur_.has_value();
			if (had) {
				st.before = tableJson(*cur_, before_frame);
				st.rows_in = cur_->numRows();
				st.cols_in = cur_->numCols();
			}
			try {
				apply(spec, st);
			}
			catch (const std::exception& e) {
				st.error = e.what();
				res.steps.push_back(st);
				res.ok = false;
				break;
			}
			st.after = tableJson(*cur_, cur_frame_);
			st.view = viewJson(*cur_, cur_frame_);
			st.rows_out = cur_->numRows();
			st.cols_out = cur_->numCols();
			res.steps.push_back(st);
		}
		return res;
	}

	const DataFrameView* current() const { return cur_ ? &*cur_ : nullptr; }
};

inline void writeTraceJson(std::ostream& os, const Result& r) {
	os << "{\"engine\":\"cpp\",\"pipeline\":" << jsonString(r.pipeline)
		<< ",\"ok\":" << (r.ok ? "true" : "false") << ",\"steps\":[";
	for (size_t i = 0; i < r.steps.size(); ++i) {
		const Step& s = r.steps[i];
		if (i) os << ",\n";
		os << "{\"index\":" << i << ",\"op\":" << jsonString(s.op) << ",\"text\":" << jsonString(s.text);
		if (!s.error.empty()) {
			os << ",\"error\":" << jsonString(s.error) << "}";
			continue;
		}
		char micros[32];
		std::snprintf(micros, sizeof(micros), "%.1f", s.micros);
		os << ",\"cpp\":" << jsonString(s.cpp)
			<< ",\"stats\":{\"rowsIn\":" << s.rows_in << ",\"rowsOut\":" << s.rows_out
			<< ",\"colsIn\":" << s.cols_in << ",\"colsOut\":" << s.cols_out
			<< ",\"micros\":" << micros << "}"
			<< ",\"before\":" << s.before
			<< ",\"after\":" << s.after
			<< ",\"view\":" << s.view
			<< ",\"map\":" << s.map
			<< ",\"storage\":" << s.storage << "}";
	}
	os << "]}\n";
}

} // namespace pipeline
