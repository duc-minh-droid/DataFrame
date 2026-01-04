#pragma once

#include <iostream>
#include <iomanip>
#include "Column.h"
#include "DataFrame.h"
#include "DataFrameView.h"

inline std::ostream& operator<<(std::ostream& os, const Column& col) {
    os << "[";
    const auto& data = col.data();
    for (size_t i = 0; i < data.size(); i++) {
        os << data[i];
        if (i < data.size() - 1) {
            os << ", ";
        }
    }
    os << "]";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const DataFrame& df) {
	// Print column names
	for (size_t i = 0; i < df.numCols(); ++i) {
		if (i > 0) os << "\t";
		os << std::setw(12) << df.getColumnName(i);
	}
	os << "\n";

	// Print separator
	for (size_t i = 0; i < df.numCols(); ++i) {
		if (i > 0) os << "\t";
		os << std::string(12, '-');
	}
	os << "\n";

	// Print rows
	for (size_t row = 0; row < df.numRows(); ++row) {
		for (size_t col = 0; col < df.numCols(); ++col) {
			if (col > 0) os << "\t";
			os << std::setw(12) << df[col][row];
		}
		os << "\n";
	}

	return os;
}

inline std::ostream& operator<<(std::ostream& os, const DataFrameView& view) {
	// Print column names
	for (size_t i = 0; i < view.numCols(); ++i) {
		if (i > 0) os << "\t";
		os << std::setw(12) << view.getColumnName(i);
	}
	os << "\n";

	// Print separator
	for (size_t i = 0; i < view.numCols(); ++i) {
		if (i > 0) os << "\t";
		os << std::string(12, '-');
	}
	os << "\n";

	// Print rows
	for (size_t row = 0; row < view.numRows(); ++row) {
		for (size_t col = 0; col < view.numCols(); ++col) {
			if (col > 0) os << "\t";
			os << std::setw(12) << view.at(row, col);
		}
		os << "\n";
	}

	return os;
}
