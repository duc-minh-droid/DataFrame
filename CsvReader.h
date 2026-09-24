#pragma once
#include "DataFrame.h"
#include <string>
#include <vector>

// Headerless numeric file with caller-supplied column names.
DataFrame readCSV(const std::string& path,
	const std::vector<std::string>& column_names);

// First line is the header. Numeric columns are parsed as doubles; any column
// holding text is dictionary-encoded as a categorical column.
DataFrame readCSVString(const std::string& csv);
DataFrame readCSVFile(const std::string& path);
