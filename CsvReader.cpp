#include "CsvReader.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>

DataFrame readCSV(const std::string& path,
	const std::vector<std::string>& column_names) {
	std::ifstream file(path);
	if (!file.is_open()) throw std::runtime_error("Failed to open CSV");

	DataFrame df(column_names);
	df.reserveRows(10000);
	std::string line;
	while (std::getline(file, line)) {
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
    std::string line;

    // Read the header line first
    if (!std::getline(ss, line)) {
        throw std::runtime_error("Empty CSV string");
    }

    std::vector<std::string> column_names;
    {
        std::stringstream header(line);
        std::string col;
        while (std::getline(header, col, ',')) {
            column_names.push_back(col);
        }
    }

    DataFrame df(column_names);

    // ---- read rows ----
    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        std::stringstream row_stream(line);
        std::vector<double> row;
        std::string cell;

        while (std::getline(row_stream, cell, ',')) {
            row.push_back(std::stod(cell));
        }

        if (row.size() != column_names.size()) {
            throw std::runtime_error("Row size mismatch");
        }

        df.addRow(row);
    }

    return df;
}
