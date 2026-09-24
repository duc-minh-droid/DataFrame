#include "CsvReader.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>

namespace {
	void stripCR(std::string& line) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
	}

	std::vector<std::string> splitLine(const std::string& line) {
		std::vector<std::string> cells;
		std::stringstream ss(line);
		std::string cell;
		while (std::getline(ss, cell, ',')) cells.push_back(cell);
		if (!line.empty() && line.back() == ',') cells.push_back("");
		return cells;
	}

	// Whole cell must be a number ("12", "-3.5", "1e3"); "12abc" is text.
	bool parseNumber(const std::string& s, double& out) {
		if (s.empty()) return false;
		try {
			size_t used = 0;
			out = std::stod(s, &used);
			return used == s.size();
		}
		catch (const std::exception&) {
			return false;
		}
	}

	// Shared by readCSVString / readCSVFile: header line, then rows. A column
	// where any cell is not a number becomes categorical; its dictionary is
	// sorted so code order matches alphabetical order (sort/groupby by code
	// then behave like sorting by label).
	DataFrame parseCSV(std::istream& in) {
		std::string line;
		if (!std::getline(in, line)) {
			throw std::runtime_error("Empty CSV");
		}
		stripCR(line);
		std::vector<std::string> column_names = splitLine(line);

		std::vector<std::vector<std::string>> cells;
		while (std::getline(in, line)) {
			stripCR(line);
			if (line.empty()) continue;
			auto row = splitLine(line);
			if (row.size() != column_names.size()) {
				throw std::runtime_error("Row size mismatch on data line " +
					std::to_string(cells.size() + 1));
			}
			cells.push_back(std::move(row));
		}

		DataFrame df(column_names);
		df.reserveRows(cells.size());
		size_t ncols = column_names.size();
		std::vector<bool> numeric(ncols, true);
		double tmp;
		for (const auto& row : cells) {
			for (size_t c = 0; c < ncols; ++c) {
				if (numeric[c] && !parseNumber(row[c], tmp)) numeric[c] = false;
			}
		}
		for (size_t c = 0; c < ncols; ++c) {
			if (numeric[c]) continue;
			std::vector<std::string> dict;
			for (const auto& row : cells) dict.push_back(row[c]);
			std::sort(dict.begin(), dict.end());
			dict.erase(std::unique(dict.begin(), dict.end()), dict.end());
			df.setLabels(c, std::move(dict));
		}

		std::vector<double> values(ncols);
		for (const auto& row : cells) {
			for (size_t c = 0; c < ncols; ++c) {
				if (numeric[c]) {
					parseNumber(row[c], values[c]);
				}
				else {
					const auto& dict = df.labels(c);
					values[c] = static_cast<double>(
						std::lower_bound(dict.begin(), dict.end(), row[c]) - dict.begin());
				}
			}
			df.addRow(values);
		}
		return df;
	}
}

DataFrame readCSV(const std::string& path,
	const std::vector<std::string>& column_names) {
	std::ifstream file(path);
	if (!file.is_open()) throw std::runtime_error("Failed to open CSV");

	DataFrame df(column_names);
	df.reserveRows(10000);
	std::string line;
	while (std::getline(file, line)) {
		stripCR(line);
		if (line.empty()) continue;
		std::stringstream ss(line);
		std::string cell;
		std::vector<double> row;

		while (std::getline(ss, cell, ',')) {
			row.push_back(std::stod(cell));
		}
		df.addRow(row);
	}

	return df;
}

DataFrame readCSVString(const std::string& csv) {
	std::istringstream ss(csv);
	return parseCSV(ss);
}

DataFrame readCSVFile(const std::string& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) throw std::runtime_error("Failed to open CSV: " + path);
	return parseCSV(file);
}
