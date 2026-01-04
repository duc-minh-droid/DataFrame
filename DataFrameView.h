#pragma once
#include "DataFrame.h"
#include <vector>
#include <unordered_map>
#include <functional>

// Forward declaration to avoid circular dependency
class GroupedDataFrame;

class DataFrameView
{
private:
	const DataFrame* df_;
	std::vector<size_t> column_indices_;
	std::vector<size_t> row_indices_;
	std::unordered_map<std::string, size_t> name_to_view_idx_;

public:
	DataFrameView(const DataFrame* df, std::vector<size_t> column_indices, std::vector<size_t> row_indices)
		: df_(df), column_indices_(std::move(column_indices)), row_indices_(row_indices)
	{
		for (size_t view_idx = 0; view_idx < column_indices_.size(); ++view_idx) {
			size_t df_idx = column_indices_[view_idx];
			name_to_view_idx_[df_->getColumnName(df_idx)] = view_idx;
		}
	}
	size_t numRows() const {
		return row_indices_.size();
	}
	size_t numCols() const {
		return column_indices_.size();
	}
	const Column& operator[](const std::string& colName) const {
		return (*this)[name_to_view_idx_.at(colName)];
	}
	const Column& operator[](size_t idx) const {
		return (*df_)[column_indices_.at(idx)];
	}
	double at(size_t row, size_t view_col_idx) const {
		if (row >= numRows()) {
			throw std::out_of_range("Row out of view bounds");
		}
		return (*df_)[column_indices_[view_col_idx]][row_indices_[row]];
	}
	const std::string& getColumnName(size_t idx) const {
		return df_->getColumnName(column_indices_.at(idx));
	}
	size_t getColumnIdx(const std::string name) const {
		return name_to_view_idx_.at(name);
	}
	GroupedDataFrame groupby(const std::string colName) const;
	DataFrameView select(std::vector<std::string> column_names) const;

	DataFrameView head(size_t n = 5) const {
		size_t k = std::min(n, row_indices_.size());
		return DataFrameView(
			df_, column_indices_, 
			std::vector<size_t>(row_indices_.begin(), row_indices_.begin() + k)
		);
	}

	DataFrameView filter(
		const std::string& col, 
		std::function<bool(double)> pred
	) const {
		size_t col_idx = getColumnIdx(col);
		const Column& c = (*df_)[column_indices_[col_idx]];

		std::vector<size_t> new_rows;

		for (auto row_idx : row_indices_) {
			if (pred(c[row_idx])) {
				new_rows.push_back(row_idx);
			}
		}

		return DataFrameView(df_, column_indices_, new_rows);
	}
};

