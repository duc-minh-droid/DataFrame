#pragma once
#include "DataFrame.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <utility>

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
		: df_(df), column_indices_(std::move(column_indices)), row_indices_(std::move(row_indices))
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
		auto it = name_to_view_idx_.find(name);
		if (it == name_to_view_idx_.end()) {
			throw std::out_of_range("No column named '" + name + "' in view");
		}
		return it->second;
	}
	bool hasColumn(const std::string& name) const {
		return name_to_view_idx_.count(name) > 0;
	}

	// The frame this view points into, and the index vectors that define it.
	const DataFrame& frame() const { return *df_; }
	const std::vector<size_t>& rowIndices() const { return row_indices_; }
	const std::vector<size_t>& columnIndices() const { return column_indices_; }
	bool isCategorical(size_t view_col_idx) const {
		return df_->isCategorical(column_indices_.at(view_col_idx));
	}
	std::string formatCell(size_t row, size_t view_col_idx) const {
		return df_->formatCell(column_indices_.at(view_col_idx), at(row, view_col_idx));
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

	// Stable sort of the row index vector by one column. No data is moved.
	DataFrameView sort(const std::string& col, bool ascending = true) const;

	// Copy the visible rows/columns into a new, owning DataFrame.
	DataFrame materialize() const;

	// count / mean / std / min / 25% / 50% / 75% / max for every numeric column.
	DataFrame describe() const;

	// Inner hash join on a key column present in both views. Output rows follow
	// left order; for each left row, matches appear in right order. If `pairs`
	// is given it receives the (left view row, right view row) of every output row.
	DataFrame join(const DataFrameView& right, const std::string& key,
		std::vector<std::pair<size_t, size_t>>* pairs = nullptr) const;
};

