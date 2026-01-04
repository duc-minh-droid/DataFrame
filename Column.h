#pragma once
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <ranges>

class Column
{
private:
	std::vector<double> column_;
public:
	Column() {}
	double operator[](size_t index) const {
		return column_[index];
	}
	void push(double value) {
		column_.push_back(value);
	}
	const std::vector<double>& data() const {
		return column_;
	}

	size_t size() const {
		return column_.size();
	}

	void reserve(size_t n) {
		column_.reserve(n);
	}

	double sum() const {
		return std::accumulate(column_.begin(), column_.end(), 0.0);
	}

	double mean() const {
		return column_.empty() ? 0.0 : sum() / size();
	}

	double min() const {
		return std::ranges::min(column_);
	}

	double max() const {
		return std::ranges::max(column_);
	}

	double median() const {
		//if (column_.empty()) {
		//	throw std::runtime_error("median() on empty column");
		//}

		std::vector<double> temp = column_;  // copy
		size_t n = temp.size();
		size_t mid = n / 2;

		std::nth_element(temp.begin(), temp.begin() + mid, temp.end());

		if (n % 2 == 1) {
			return temp[mid];
		}
		else {
			double high = temp[mid];
			std::nth_element(temp.begin(), temp.begin() + mid - 1, temp.end());
			double low = temp[mid - 1];
			return (low + high) / 2.0;
		}
	}

};

