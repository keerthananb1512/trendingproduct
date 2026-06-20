#include "count_min_sketch.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

CountMinSketch::CountMinSketch(std::size_t rows, std::size_t width)
    : rows_(rows),
      width_(width),
      table_(rows, std::vector<std::uint64_t>(width, 0)),
      seeds_(rows) {
    if (rows_ == 0 || width_ == 0) {
        throw std::invalid_argument("CountMinSketch rows and width must be positive");
    }

    constexpr std::uint64_t base_seed = 0x9e3779b97f4a7c15ULL;
    for (std::size_t row = 0; row < rows_; ++row) {
        seeds_[row] = splitmix64(base_seed + row);
    }
}

void CountMinSketch::update(std::uint64_t item_id, std::uint64_t count) {
    for (std::size_t row = 0; row < rows_; ++row) {
        table_[row][bucket_for(item_id, row)] += count;
    }
}

std::uint64_t CountMinSketch::estimate(std::uint64_t item_id) const {
    std::uint64_t minimum_count = std::numeric_limits<std::uint64_t>::max();

    for (std::size_t row = 0; row < rows_; ++row) {
        minimum_count = std::min(minimum_count, table_[row][bucket_for(item_id, row)]);
    }

    return minimum_count;
}

std::size_t CountMinSketch::rows() const {
    return rows_;
}

std::size_t CountMinSketch::width() const {
    return width_;
}

std::size_t CountMinSketch::bucket_for(std::uint64_t item_id, std::size_t row) const {
    return splitmix64(item_id + seeds_[row]) % width_;
}

std::uint64_t CountMinSketch::splitmix64(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
