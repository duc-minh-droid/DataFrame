#pragma once
#include <string>
#include <vector>
#include "Column.h"
#include <map>
#include <unordered_map>
#include <stdexcept>
#include <ranges>

// Forward declarations
class GroupedDataFrame;
class DataFrameView;

class DataFrame
{
private:
	std::vector<std::string> column_names_;
	std::vector<Column> columns_;
	std::unordered_map<std::string, size_t> name_to_index_;

	std::vector<size_t> all_column_indices() const {
		std::vector<size_t> idx;
		idx.reserve(numCols());
		for (size_t i = 0; i < numCols(); ++i) {
			idx.push_back(i);
		}
		return idx;
	}
	std::vector<size_t> all_row_indices() const {
		std::vector<size_t> rows(numRows());
		std::iota(rows.begin(), rows.end(), 0);
		return rows;
	}

public:
	DataFrame(const std::vector<std::string>& column_names)
		: column_names_(column_names),
		columns_(column_names.size()) {
		if (column_names_.empty()) {
			throw std::runtime_error("DataFrame must have at least one column");
		}
		for (size_t i = 0; i < column_names_.size(); i++) {
			name_to_index_[column_names_[i]] = i;
		}
	}

	size_t numCols() const {
		return columns_.size();
	}

	size_t numRows() const {
		return columns_.empty() ? 0 : columns_[0].size();
	}

	const std::string& getColumnName(size_t idx) const {
		if (idx >= column_names_.size()) {
			throw std::out_of_range("Column index out of range");
		}
		return column_names_[idx];
	}

	void reserveRows(size_t n) {
		for (auto& col : columns_) {
			col.reserve(n);
		}
	}

	DataFrameView head(size_t n = 5) const;

	DataFrameView select(std::vector<std::string> column_names) const;

	GroupedDataFrame groupby(const std::string colName) const;

	void addRow(const std::vector<double>& row) {
		if (row.size() != columns_.size()) {
			throw std::runtime_error("Row size mismatch");
		}
		for (size_t i = 0; i < row.size(); i++) {
			columns_[i].push(row[i]);
		}
	}

	Column& operator[](std::string colName) {
		return columns_.at(name_to_index_.at(colName));
	}
	Column& operator[](size_t idx) {
		return columns_.at(idx);
	}

	const Column& operator[](const std::string& colName) const {
		return columns_.at(name_to_index_.at(colName));
	}
	const Column& operator[](size_t idx) const {
		return columns_.at(idx);
	}
};

