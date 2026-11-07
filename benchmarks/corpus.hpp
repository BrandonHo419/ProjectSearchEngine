#ifndef BENCHMARK_CORPUS_HPP
#define BENCHMARK_CORPUS_HPP

#include <cstdint>
#include <stdexcept>
#include "Index.h"

namespace benchmark {
// Version 1: fixed uint32 arithmetic, high bits for token selection, ASCII only.
inline std::vector<TextDocument> corpus(int count, int words, std::uint32_t seed) {
    if (count < 1 || count > 10000 || words < 16 || words > 4096 ||
        static_cast<std::int64_t>(count) * words > 2000000)
        throw std::invalid_argument("Corpus requires 1..10000 documents, 16..4096 words, at most 2000000 tokens.");
    std::vector<TextDocument> documents;
    documents.reserve(count);
    std::uint32_t state = seed;
    for (int i = 0; i < count; ++i) {
        std::string text = "common ";
        text += i % 20 == 0 ? "rare " : "ordinary ";
        text += i % 4 == 0 ? "amber silver comet" : "amber quiet comet";
        for (int j = 5; j < words; ++j) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            const auto number = std::to_string((state >> 16) % 256);
            text += " tok" + std::string(4 - number.size(), '0') + number;
        }
        documents.push_back({"synthetic:" + std::to_string(i), std::move(text)});
    }
    return documents;
}

inline std::uint64_t fingerprint(const std::vector<TextDocument>& documents) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (const auto& doc : documents) {
        for (const auto* field : {&doc.id, &doc.text}) {
            for (unsigned char byte : *field) {
                hash ^= byte;
                hash *= UINT64_C(1099511628211);
            }
            hash ^= 0; // NUL separator after each field, including the last.
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

struct Workload { const char* name; const char* query; };
inline const std::vector<Workload>& workloads() {
    static const std::vector<Workload> value = {
        {"common_term", "common"}, {"rare_term", "rare"},
        {"phrase", "\"amber silver comet\""}, {"prefix", "tok000*"},
        {"boolean_and", "rare AND silver"}, {"boolean_or", "rare OR silver"},
        {"exclusion", "common - silver"}, {"missing", "absenttoken"},
    };
    return value;
}
} // namespace benchmark
#endif
