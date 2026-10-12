#include "count_min_sketch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr std::size_t kView = 0;
constexpr std::size_t kAddToCart = 1;
constexpr std::size_t kTransaction = 2;
constexpr std::size_t kEventTypeCount = 3;

constexpr std::array<const char*, kEventTypeCount> kOutputFiles = {
    "most_viewed.csv",
    "most_added_to_cart.csv",
    "most_purchased.csv",
};

struct EventResult {
    std::string date;
    std::uint64_t item_id;
    std::uint64_t estimated_count;
    std::uint64_t exact_count;
    std::int64_t error;
    double threshold;
    std::uint64_t total_day_events;
};

struct ProductEvent {
    std::uint64_t item_id;
    std::size_t type;
};

struct DailyAnalysis {
    std::array<std::vector<EventResult>, kEventTypeCount> event_results;
};

std::uint64_t parse_item_id(const std::string& value) {
    return static_cast<std::uint64_t>(std::stoull(value));
}

std::size_t event_type_index(const std::string& event_type) {
    if (event_type == "view") {
        return kView;
    }
    if (event_type == "addtocart") {
        return kAddToCart;
    }
    if (event_type == "transaction") {
        return kTransaction;
    }
    return kEventTypeCount;
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
        const std::size_t type = event_type_index(event_type);
        if (date.empty() || item_value.empty() || type == kEventTypeCount) {
            continue;
        }

        events_by_date[date].push_back({parse_item_id(item_value), type});
    }

    return events_by_date;
}

std::uint64_t exact_count_for(
    const std::unordered_map<std::uint64_t, std::uint64_t>& counts,
    std::uint64_t item_id) {
    const auto found = counts.find(item_id);
    return found == counts.end() ? 0 : found->second;
}

DailyAnalysis analyze_day(
    const std::string& date,
    const std::vector<ProductEvent>& events,
    std::size_t k,
    double delta,
    double epsilon_divisor) {
    const double epsilon = 1.0 / (epsilon_divisor * static_cast<double>(k));
    const auto width = static_cast<std::size_t>(std::ceil(std::exp(1.0) / epsilon));
    const auto rows = static_cast<std::size_t>(std::ceil(std::log(1.0 / delta)));

    std::array<CountMinSketch, kEventTypeCount> sketches = {
        CountMinSketch(rows, width),
        CountMinSketch(rows, width),
        CountMinSketch(rows, width),
    };
    std::array<std::unordered_set<std::uint64_t>, kEventTypeCount> event_candidates;
    std::array<std::unordered_map<std::uint64_t, std::uint64_t>, kEventTypeCount>
        exact_counts;
    std::array<std::uint64_t, kEventTypeCount> total_events = {0, 0, 0};

    for (const ProductEvent& event : events) {
        sketches[event.type].update(event.item_id);
        ++exact_counts[event.type][event.item_id];
        ++total_events[event.type];

        const std::uint64_t event_estimate = sketches[event.type].estimate(event.item_id);
        const double event_threshold =
            static_cast<double>(total_events[event.type]) / static_cast<double>(k);
        if (static_cast<double>(event_estimate) >= event_threshold) {
            event_candidates[event.type].insert(event.item_id);
        }
    }

    DailyAnalysis analysis;

    for (std::size_t type = 0; type < kEventTypeCount; ++type) {
        const double threshold =
            static_cast<double>(total_events[type]) / static_cast<double>(k);
        auto& results = analysis.event_results[type];
        results.reserve(event_candidates[type].size());

        for (const std::uint64_t item_id : event_candidates[type]) {
            const std::uint64_t estimate = sketches[type].estimate(item_id);
            if (static_cast<double>(estimate) < threshold) {
                continue;
            }

            const std::uint64_t exact = exact_count_for(exact_counts[type], item_id);
            results.push_back({
                date,
                item_id,
                estimate,
                exact,
                static_cast<std::int64_t>(estimate) - static_cast<std::int64_t>(exact),
                threshold,
                total_events[type],
            });
        }

        std::sort(results.begin(), results.end(), [](const EventResult& left,
                                                     const EventResult& right) {
            if (left.estimated_count != right.estimated_count) {
                return left.estimated_count > right.estimated_count;
            }
            return left.item_id < right.item_id;
        });
    }

    return analysis;
}

void write_event_results(
    const std::filesystem::path& output_path,
    const std::vector<EventResult>& results) {
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("Could not open output file: " + output_path.string());
    }

    output << "date,itemid,estimated_count,exact_count,error,threshold,total_day_events\n";
    for (const EventResult& result : results) {
        output << result.date << ','
               << result.item_id << ','
               << result.estimated_count << ','
               << result.exact_count << ','
               << result.error << ','
               << result.threshold << ','
               << result.total_day_events << '\n';
    }
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string input_path =
        argc > 1 ? argv[1] : "data/processed/events_daily_clean.csv";
    const std::filesystem::path output_directory = argc > 2 ? argv[2] : "results";
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

        std::array<std::vector<EventResult>, kEventTypeCount> all_event_results;

        for (const std::string& date : dates) {
            DailyAnalysis daily = analyze_day(
                date, events_by_date.at(date), k, delta, epsilon_divisor);

            for (std::size_t type = 0; type < kEventTypeCount; ++type) {
                auto& destination = all_event_results[type];
                auto& source = daily.event_results[type];
                destination.insert(destination.end(), source.begin(), source.end());
            }
        }

        std::filesystem::create_directories(output_directory);
        for (std::size_t type = 0; type < kEventTypeCount; ++type) {
            write_event_results(output_directory / kOutputFiles[type], all_event_results[type]);
        }

        std::cout << "Processed " << dates.size() << " days\n";
        std::cout << "Used separate view, add-to-cart, and purchase sketches\n";
        std::cout << "Used epsilon = 1 / (" << epsilon_divisor << " * k)\n";
        std::cout << "Wrote " << all_event_results[kView].size()
                  << " most-viewed rows\n";
        std::cout << "Wrote " << all_event_results[kAddToCart].size()
                  << " most-added-to-cart rows\n";
        std::cout << "Wrote " << all_event_results[kTransaction].size()
                  << " most-purchased rows\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
