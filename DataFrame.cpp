#include "DataFrame.h"
#include "DataFrameView.h"
#include "GroupedDataFrame.h"
#include <sstream>

std::string DataFrame::formatCell(size_t idx, double v) const {
	const auto& dict = labels_.at(idx);
	if (!dict.empty()) {
		size_t code = static_cast<size_t>(v);
		return code < dict.size() ? dict[code] : std::string("?");
	}
	std::ostringstream os;
	os << v;
	return os.str();
}

DataFrameView DataFrame::view() const {
	return DataFrameView(this, all_column_indices(), all_row_indices());
}

DataFrameView DataFrame::head(size_t n) const {
	auto row_indices = all_row_indices();
	auto col_indices = all_column_indices();
	size_t k = std::min(n, row_indices.size());
	return DataFrameView(
		this, col_indices,
		std::vector<size_t>(row_indices.begin(), row_indices.begin() + k)
	);
}

DataFrameView DataFrame::select(std::vector<std::string> column_names) const {
	std::vector<size_t> indices;
	for (auto name : column_names) {
		indices.push_back(columnIndex(name));
	}
	return DataFrameView(this, indices, all_row_indices());
}

GroupedDataFrame DataFrame::groupby(const std::string colName) const {
	DataFrameView v = view();
	size_t key_view_idx = v.getColumnIdx(colName);
	return GroupedDataFrame(v, key_view_idx);
}
