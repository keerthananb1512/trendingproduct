#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class CountMinSketch {
public:
    CountMinSketch(std::size_t rows, std::size_t width);

    void update(std::uint64_t item_id, std::uint64_t count = 1);
    std::uint64_t estimate(std::uint64_t item_id) const;

    std::size_t rows() const;
    std::size_t width() const;

private:
    std::size_t rows_;
    std::size_t width_;
    std::vector<std::vector<std::uint64_t>> table_;
    std::vector<std::uint64_t> seeds_;

    std::size_t bucket_for(std::uint64_t item_id, std::size_t row) const;
    static std::uint64_t splitmix64(std::uint64_t value);
};
