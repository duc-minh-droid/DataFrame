#pragma once
#include "DataFrame.h"
#include "DataFrameView.h"
#include <string>
#include <unordered_map>
#include <vector>

class GroupedDataFrame
{
private:
	const DataFrameView view_;
	size_t key_idx_;
	std::unordered_map<double, std::vector<size_t>> val_to_indices_;
public:
	GroupedDataFrame(DataFrameView view, size_t key_idx) 
		: view_(view), key_idx_(key_idx) {
		for (size_t i = 0; i < view_.numRows(); i++) {
			auto val = view_.at(i, key_idx_);
			val_to_indices_[val].push_back(i);
		}
	}

	DataFrame count() const {
		DataFrame res({view_.getColumnName(key_idx_), "Count"});
		res.reserveRows(val_to_indices_.size());

		for (auto r : val_to_indices_) {
			res.addRow({ r.first, static_cast<double>(r.second.size()) });
		}

		return res;
	}

	DataFrame sum(const std::string& colName) const {
		DataFrame res({ view_.getColumnName(key_idx_), colName + "_sum" });
		res.reserveRows(val_to_indices_.size());
		size_t col_idx = view_.getColumnIdx(colName);

		for (auto r : val_to_indices_) {
			double sum = 0;
			for (auto row_idx : r.second) {
				sum += view_.at(row_idx, col_idx);
			}
			res.addRow({ r.first, sum });
		}

		return res;
	}

	DataFrame mean(const std::string& colName) const {
		DataFrame res({ view_.getColumnName(key_idx_), colName + "_mean" });
		res.reserveRows(val_to_indices_.size());
		size_t col_idx = view_.getColumnIdx(colName);

		for (auto r : val_to_indices_) {
			double sum = 0;
			for (auto row_idx : r.second) {
				sum += view_.at(row_idx, col_idx);
			}
			res.addRow({ r.first, sum/r.second.size()});
		}

		return res;
	}
};

