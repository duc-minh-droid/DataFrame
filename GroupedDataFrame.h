#pragma once
#include "DataFrame.h"
#include "DataFrameView.h"
#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

// One aggregation in GroupedDataFrame::agg(): fn is count/sum/mean/min/max.
struct Aggregation {
	std::string fn;
	std::string column; // ignored for count
};

class GroupedDataFrame
{
private:
	const DataFrameView view_;
	size_t key_idx_;
	// Groups are kept sorted by key (like pandas' sort=True) so results are
	// deterministic. groups_[g] holds view row positions in original order.
	std::vector<double> keys_;
	std::vector<std::vector<size_t>> groups_;

	DataFrame makeResult(const std::vector<std::string>& value_names) const {
		std::vector<std::string> names = { view_.getColumnName(key_idx_) };
		names.insert(names.end(), value_names.begin(), value_names.end());
		DataFrame res(names);
		res.setLabels(0, view_.frame().labels(view_.columnIndices()[key_idx_]));
		res.reserveRows(keys_.size());
		return res;
	}

	double reduce(const std::string& fn, const std::vector<size_t>& rows, size_t col_idx) const {
		if (fn == "count") return static_cast<double>(rows.size());
		double acc = 0;
		if (fn == "sum" || fn == "mean") {
			for (auto row_idx : rows) acc += view_.at(row_idx, col_idx);
			return fn == "sum" ? acc : acc / rows.size();
		}
		if (fn == "min") {
			acc = std::numeric_limits<double>::infinity();
			for (auto row_idx : rows) acc = std::min(acc, view_.at(row_idx, col_idx));
			return acc;
		}
		if (fn == "max") {
			acc = -std::numeric_limits<double>::infinity();
			for (auto row_idx : rows) acc = std::max(acc, view_.at(row_idx, col_idx));
			return acc;
		}
		throw std::runtime_error("Unknown aggregation '" + fn + "'");
	}

public:
	GroupedDataFrame(DataFrameView view, size_t key_idx)
		: view_(view), key_idx_(key_idx) {
		std::unordered_map<double, std::vector<size_t>> val_to_indices;
		for (size_t i = 0; i < view_.numRows(); i++) {
			auto val = view_.at(i, key_idx_);
			val_to_indices[val].push_back(i);
		}
		keys_.reserve(val_to_indices.size());
		for (const auto& kv : val_to_indices) keys_.push_back(kv.first);
		std::sort(keys_.begin(), keys_.end());
		groups_.reserve(keys_.size());
		for (double k : keys_) groups_.push_back(std::move(val_to_indices[k]));
	}

	size_t numGroups() const { return keys_.size(); }
	const std::vector<double>& keys() const { return keys_; }
	const std::vector<std::vector<size_t>>& groups() const { return groups_; }
	const DataFrameView& view() const { return view_; }

	DataFrame count() const {
		DataFrame res = makeResult({ "Count" });
		for (size_t g = 0; g < keys_.size(); ++g) {
			res.addRow({ keys_[g], static_cast<double>(groups_[g].size()) });
		}
		return res;
	}

	DataFrame sum(const std::string& colName) const {
		return agg({ { "sum", colName } });
	}

	DataFrame mean(const std::string& colName) const {
		return agg({ { "mean", colName } });
	}

	DataFrame min(const std::string& colName) const {
		return agg({ { "min", colName } });
	}

	DataFrame max(const std::string& colName) const {
		return agg({ { "max", colName } });
	}

	// Several aggregations at once, one output column each:
	// count -> "count", everything else -> "<column>_<fn>".
	DataFrame agg(const std::vector<Aggregation>& aggs) const {
		std::vector<std::string> names;
		std::vector<size_t> cols;
		for (const auto& a : aggs) {
			if (a.fn == "count") {
				names.push_back("count");
				cols.push_back(0);
			}
			else {
				size_t c = view_.getColumnIdx(a.column);
				if (view_.isCategorical(c)) {
					throw std::runtime_error(a.fn + "() on categorical column '" + a.column + "'");
				}
				names.push_back(a.column + "_" + a.fn);
				cols.push_back(c);
			}
		}
		DataFrame res = makeResult(names);
		std::vector<double> row(aggs.size() + 1);
		for (size_t g = 0; g < keys_.size(); ++g) {
			row[0] = keys_[g];
			for (size_t k = 0; k < aggs.size(); ++k) {
				row[k + 1] = reduce(aggs[k].fn, groups_[g], cols[k]);
			}
			res.addRow(row);
		}
		return res;
	}
};
