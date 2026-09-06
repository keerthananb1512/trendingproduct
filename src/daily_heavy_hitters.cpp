#include "count_min_sketch.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

struct DailyResult {
    std::string date;
    std::uint64_t item_id;
    std::uint64_t estimated_score;
    std::uint64_t exact_score;
    std::int64_t error;
    double threshold;
    std::uint64_t total_day_score;
};

struct ProductEvent {
    std::uint64_t item_id;
    std::uint64_t weight;
};

std::uint64_t parse_item_id(const std::string& value) {
    return static_cast<std::uint64_t>(std::stoull(value));
}

std::uint64_t event_weight(const std::string& event_type) {
    if (event_type == "view") {
        return 1;
    }
    if (event_type == "addtocart") {
        return 3;
    }
    if (event_type == "transaction") {
        return 5;
    }
    return 1;
}

std::unordered_map<std::string, std::vector<ProductEvent>> read_daily_events(
    const std::string& input_path) {
    std::ifstream input(input_path);
    if (!input) {
        throw std::runtime_error("Could not open input file: " + input_path);
    }

    std::unordered_map<std::string, std::vector<ProductEvent>> events_by_date;
    std::string line;

    std::getline(input, line); // header: date,event,itemid

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        const std::size_t first_comma = line.find(',');
        const std::size_t second_comma = line.find(',', first_comma + 1);
        if (first_comma == std::string::npos || second_comma == std::string::npos) {
            continue;
        }

        const std::string date = line.substr(0, first_comma);
        const std::string event_type =
            line.substr(first_comma + 1, second_comma - first_comma - 1);
        const std::string item_value = line.substr(second_comma + 1);
        if (date.empty() || event_type.empty() || item_value.empty()) {
            continue;
        }

        events_by_date[date].push_back({parse_item_id(item_value), event_weight(event_type)});
    }

    return events_by_date;
}

std::vector<DailyResult> find_daily_heavy_hitters(
    const std::string& date,
    const std::vector<ProductEvent>& events,
    std::size_t k,
    double delta,
    double epsilon_divisor) {
    const double epsilon = 1.0 / (epsilon_divisor * static_cast<double>(k));
    const auto width = static_cast<std::size_t>(std::ceil(std::exp(1.0) / epsilon));
    const auto rows = static_cast<std::size_t>(std::ceil(std::log(1.0 / delta)));

    CountMinSketch sketch(rows, width);
    std::unordered_set<std::uint64_t> candidates;
    std::unordered_map<std::uint64_t, std::uint64_t> exact_scores;

    std::uint64_t processed_score = 0;
    for (const ProductEvent& event : events) {
        processed_score += event.weight;
        sketch.update(event.item_id, event.weight);
        exact_scores[event.item_id] += event.weight;

        const std::uint64_t estimated_score = sketch.estimate(event.item_id);
        const double current_threshold =
            static_cast<double>(processed_score) / static_cast<double>(k);

        if (static_cast<double>(estimated_score) >= current_threshold) {
            candidates.insert(event.item_id);
        }
    }

    const std::uint64_t total_day_score = processed_score;
    const double final_threshold =
        static_cast<double>(total_day_score) / static_cast<double>(k);

    std::vector<DailyResult> results;
    results.reserve(candidates.size());

    for (const std::uint64_t item_id : candidates) {
        const std::uint64_t estimated_score = sketch.estimate(item_id);
        if (static_cast<double>(estimated_score) >= final_threshold) {
            const std::uint64_t exact_score = exact_scores.at(item_id);
            const auto error = static_cast<std::int64_t>(estimated_score) -
                               static_cast<std::int64_t>(exact_score);
            results.push_back({
                date,
                item_id,
                estimated_score,
                exact_score,
                error,
                final_threshold,
                total_day_score,
            });
        }
    }

    std::sort(results.begin(), results.end(), [](const DailyResult& left, const DailyResult& right) {
        if (left.date != right.date) {
            return left.date < right.date;
        }
        if (left.estimated_score != right.estimated_score) {
            return left.estimated_score > right.estimated_score;
        }
        return left.item_id < right.item_id;
    });

    return results;
}

void write_results(const std::string& output_path, const std::vector<DailyResult>& results) {
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("Could not open output file: " + output_path);
    }

    output << "date,itemid,estimated_score,exact_score,error,threshold,total_day_score\n";
    for (const DailyResult& result : results) {
        output << result.date << ','
               << result.item_id << ','
               << result.estimated_score << ','
               << result.exact_score << ','
               << result.error << ','
               << result.threshold << ','
               << result.total_day_score << '\n';
    }
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string input_path =
        argc > 1 ? argv[1] : "data/processed/events_daily_clean.csv";
    const std::string output_path =
        argc > 2 ? argv[2] : "results/daily_heavy_hitters_validated.csv";
    const std::size_t k = argc > 3 ? static_cast<std::size_t>(std::stoull(argv[3])) : 100;
    const double delta = argc > 4 ? std::stod(argv[4]) : 0.01;
    const double epsilon_divisor = argc > 5 ? std::stod(argv[5]) : 5.0;

    if (k == 0) {
        std::cerr << "k must be positive\n";
        return 1;
    }
    if (delta <= 0.0 || delta >= 1.0) {
        std::cerr << "delta must be between 0 and 1\n";
        return 1;
    }
    if (epsilon_divisor <= 0.0) {
        std::cerr << "epsilon divisor must be positive\n";
        return 1;
    }

    try {
        const auto events_by_date = read_daily_events(input_path);
        std::vector<std::string> dates;
        dates.reserve(events_by_date.size());
        for (const auto& entry : events_by_date) {
            dates.push_back(entry.first);
        }
        std::sort(dates.begin(), dates.end());

        std::vector<DailyResult> all_results;
        for (const std::string& date : dates) {
            const auto& item_ids = events_by_date.at(date);
            auto daily_results = find_daily_heavy_hitters(
                date, item_ids, k, delta, epsilon_divisor);
            all_results.insert(all_results.end(), daily_results.begin(), daily_results.end());
        }

        write_results(output_path, all_results);

        std::cout << "Processed " << dates.size() << " days\n";
        std::cout << "Used epsilon = 1 / (" << epsilon_divisor << " * k)\n";
        std::cout << "Wrote " << all_results.size() << " heavy-hitter rows to "
                  << output_path << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
