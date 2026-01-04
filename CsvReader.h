#pragma once
#include "DataFrame.h"
#include <string>
#include <vector>

DataFrame readCSV(const std::string& path,
	const std::vector<std::string>& column_names);
DataFrame readCSVString(const std::string& csv);