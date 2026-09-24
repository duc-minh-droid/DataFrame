#include "DataFrameView.h"
#include "GroupedDataFrame.h"
#include <algorithm>
#include <cstdio>
#include <unordered_map>

GroupedDataFrame DataFrameView::groupby(const std::string colName) const {
	return GroupedDataFrame(*this, getColumnIdx(colName));
}

DataFrameView DataFrameView::select(std::vector<std::string> column_names) const {
	std::vector<size_t> new_column_indices;
	new_column_indices.reserve(column_names.size());
	for (auto name : column_names) {
		new_column_indices.push_back(column_indices_[getColumnIdx(name)]);
	}
	return DataFrameView(df_, std::move(new_column_indices), row_indices_);
}

DataFrameView DataFrameView::sort(const std::string& col, bool ascending) const {
	const Column& c = (*df_)[column_indices_[getColumnIdx(col)]];
	std::vector<size_t> rows = row_indices_;
	if (ascending) {
		std::stable_sort(rows.begin(), rows.end(),
			[&](size_t a, size_t b) { return c[a] < c[b]; });
	}
	else {
		std::stable_sort(rows.begin(), rows.end(),
			[&](size_t a, size_t b) { return c[a] > c[b]; });
	}
	return DataFrameView(df_, column_indices_, std::move(rows));
}

DataFrame DataFrameView::materialize() const {
	std::vector<std::string> names;
	for (size_t i = 0; i < numCols(); ++i) names.push_back(getColumnName(i));
	DataFrame out(names);
	for (size_t i = 0; i < numCols(); ++i) {
		out.setLabels(i, df_->labels(column_indices_[i]));
	}
	out.reserveRows(numRows());
	std::vector<double> row(numCols());
	for (size_t r = 0; r < numRows(); ++r) {
		for (size_t c = 0; c < numCols(); ++c) row[c] = at(r, c);
		out.addRow(row);
	}
	return out;
}

DataFrame DataFrameView::describe() const {
	static const std::vector<std::string> stats =
		{ "count", "mean", "std", "min", "25%", "50%", "75%", "max" };

	std::vector<std::string> names = { "stat" };
	std::vector<size_t> numeric;
	for (size_t i = 0; i < numCols(); ++i) {
		if (!isCategorical(i)) {
			numeric.push_back(i);
			names.push_back(getColumnName(i));
		}
	}
	if (numeric.empty()) {
		throw std::runtime_error("describe() needs at least one numeric column");
	}
	if (numRows() == 0) {
		throw std::runtime_error("describe() on an empty view");
	}

	// Gather each numeric column of the view into a contiguous Column.
	std::vector<Column> cols(numeric.size());
	for (size_t k = 0; k < numeric.size(); ++k) {
		cols[k].reserve(numRows());
		for (size_t r = 0; r < numRows(); ++r) cols[k].push(at(r, numeric[k]));
	}

	// One sort per column for the three quartiles.
	std::vector<std::vector<double>> quart;
	for (const Column& c : cols) quart.push_back(c.quantiles({ 0.25, 0.50, 0.75 }));

	DataFrame out(names);
	out.setLabels(0, stats);
	for (size_t s = 0; s < stats.size(); ++s) {
		std::vector<double> row = { static_cast<double>(s) };
		for (size_t k = 0; k < cols.size(); ++k) {
			const Column& c = cols[k];
			switch (s) {
			case 0: row.push_back(static_cast<double>(c.size())); break;
			case 1: row.push_back(c.mean()); break;
			case 2: row.push_back(c.stddev()); break;
			case 3: row.push_back(c.min()); break;
			case 4: row.push_back(quart[k][0]); break;
			case 5: row.push_back(quart[k][1]); break;
			case 6: row.push_back(quart[k][2]); break;
			default: row.push_back(c.max()); break;
			}
		}
		out.addRow(row);
	}
	return out;
}

namespace {
	// Join keys are compared by their printed value so a categorical "North"
	// in one frame matches "North" in another even if the codes differ.
	std::string joinKey(const DataFrameView& v, size_t row, size_t col) {
		if (v.isCategorical(col)) return v.formatCell(row, col);
		char buf[40];
		std::snprintf(buf, sizeof(buf), "%.17g", v.at(row, col));
		return buf;
	}
}

DataFrame DataFrameView::join(const DataFrameView& right, const std::string& key,
	std::vector<std::pair<size_t, size_t>>* pairs) const {
	size_t lkey = getColumnIdx(key);
	size_t rkey = right.getColumnIdx(key);
	if (isCategorical(lkey) != right.isCategorical(rkey)) {
		throw std::runtime_error("join(): key '" + key + "' is categorical on one side only");
	}

	// Output schema: every left column, then right columns except the key.
	std::vector<std::string> names;
	for (size_t i = 0; i < numCols(); ++i) names.push_back(getColumnName(i));
	std::vector<size_t> right_cols;
	for (size_t i = 0; i < right.numCols(); ++i) {
		if (i == rkey) continue;
		right_cols.push_back(i);
		std::string name = right.getColumnName(i);
		if (std::find(names.begin(), names.end(), name) != names.end()) name += "_right";
		names.push_back(name);
	}

	DataFrame out(names);
	for (size_t i = 0; i < numCols(); ++i) {
		out.setLabels(i, df_->labels(column_indices_[i]));
	}
	for (size_t k = 0; k < right_cols.size(); ++k) {
		out.setLabels(numCols() + k,
			right.frame().labels(right.columnIndices()[right_cols[k]]));
	}

	// Build phase: hash the right side.
	std::unordered_map<std::string, std::vector<size_t>> index;
	for (size_t r = 0; r < right.numRows(); ++r) {
		index[joinKey(right, r, rkey)].push_back(r);
	}

	// Probe phase: walk the left side in order.
	std::vector<double> row(names.size());
	for (size_t l = 0; l < numRows(); ++l) {
		auto it = index.find(joinKey(*this, l, lkey));
		if (it == index.end()) continue;
		for (size_t r : it->second) {
			for (size_t c = 0; c < numCols(); ++c) row[c] = at(l, c);
			for (size_t k = 0; k < right_cols.size(); ++k) {
				row[numCols() + k] = right.at(r, right_cols[k]);
			}
			out.addRow(row);
			if (pairs) pairs->emplace_back(l, r);
		}
	}
	return out;
}
