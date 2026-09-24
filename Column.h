#pragma once
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <ranges>
#include <cmath>
#include <stdexcept>

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
		if (column_.empty()) {
			throw std::runtime_error("min() on empty column");
		}
		return std::ranges::min(column_);
	}

	double max() const {
		if (column_.empty()) {
			throw std::runtime_error("max() on empty column");
		}
		return std::ranges::max(column_);
	}

	// Sample standard deviation (n - 1), same as pandas' default.
	double stddev() const {
		size_t n = column_.size();
		if (n < 2) return 0.0;
		double m = mean();
		double acc = 0.0;
		for (double v : column_) {
			acc += (v - m) * (v - m);
		}
		return std::sqrt(acc / (n - 1));
	}

	// Quantiles with linear interpolation between closest ranks
	// (pandas / numpy "linear" method). Each q in [0, 1]. Sorts one copy.
	std::vector<double> quantiles(const std::vector<double>& qs) const {
		if (column_.empty()) {
			throw std::runtime_error("quantile() on empty column");
		}
		std::vector<double> temp = column_;
		std::sort(temp.begin(), temp.end());
		std::vector<double> out;
		for (double q : qs) {
			double pos = q * (temp.size() - 1);
			size_t lo = static_cast<size_t>(std::floor(pos));
			size_t hi = std::min(lo + 1, temp.size() - 1);
			double frac = pos - lo;
			out.push_back(temp[lo] + (temp[hi] - temp[lo]) * frac);
		}
		return out;
	}

	double quantile(double q) const {
		return quantiles({ q })[0];
	}

	double median() const {
		if (column_.empty()) {
			throw std::runtime_error("median() on empty column");
		}

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

