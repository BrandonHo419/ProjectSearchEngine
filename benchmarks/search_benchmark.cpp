#include <chrono>
#include <iomanip>
#include <iostream>
#include <locale>
#include <memory>
#include <stdexcept>
#include "benchmarks/corpus.hpp"
#include "query_processor.hpp"

using Clock = std::chrono::steady_clock;

double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void buildTrie(Indexer& index, PrefixSearcher& trie) {
    for (const auto& [word, postings] : index.getIndex())
        for (const auto& [id, data] : postings) trie.insert(word, {id, data.first});
}

// Include response destruction and snippet work, but not console/file output.
std::uint64_t query(const std::string& text, Indexer& index, PrefixSearcher& trie) {
    const auto response = executeQuery(text, index, trie, 10);
    if (!response.ok()) throw std::runtime_error("Benchmark query is invalid.");
    std::uint64_t checksum = response.results.size() + response.suggestions.size();
    for (const auto& result : response.results) {
        checksum += result.document.size() + result.matches.size() + result.spans.size();
        checksum += index.getSnippetFromSpans(result.document, result.spans).size();
    }
    return checksum;
}

int number(const char* text, int minimum, int maximum) {
    const std::string value(text);
    std::size_t used = 0;
    const auto result = std::stoll(value, &used);
    if (used != value.size() || result < minimum || result > maximum)
        throw std::invalid_argument("Invalid benchmark argument.");
    return static_cast<int>(result);
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 6) throw std::invalid_argument("Usage: benchmark documents words seed samples iterations");
        const int count = number(argv[1], 1, 10000), words = number(argv[2], 16, 4096);
        const auto seed = static_cast<std::uint32_t>(number(argv[3], 0, 2147483647));
        const int samples = number(argv[4], 3, 100), iterations = number(argv[5], 1, 1000);
        const auto documents = benchmark::corpus(count, words, seed);
        std::size_t bytes = 0;
        for (const auto& doc : documents) bytes += doc.text.size();
        std::cout.imbue(std::locale::classic());
        std::cout << std::setprecision(12);
        std::cout << "{\"documents\":" << count << ",\"words_per_document\":" << words
                  << ",\"text_bytes\":" << bytes << ",\"seed\":" << seed
                  << ",\"fingerprint_fnv1a64\":\"" << std::hex << benchmark::fingerprint(documents)
                  << std::dec << "\"}\n";
        std::cout << "workload,sample,iterations,total_ms,checksum\n";
        std::unique_ptr<Indexer> index;
        std::unique_ptr<PrefixSearcher> trie;
        // One discarded warm-up build, then fresh objects for every sample.
        for (int sample = -1; sample < samples; ++sample) {
            trie.reset(); index.reset(); // Destruction is outside build timings.
            index = std::make_unique<Indexer>();
            auto start = Clock::now();
            index->buildIndexFromText(documents);
            const double indexMs = milliseconds(start);
            trie = std::make_unique<PrefixSearcher>();
            start = Clock::now();
            buildTrie(*index, *trie);
            const double trieMs = milliseconds(start);
            if (index->getTotalDoc() != count) throw std::runtime_error("Incomplete corpus.");
            if (sample >= 0) {
                std::cout << "index," << sample << ",1," << indexMs << ',' << index->getTotalDoc() << '\n';
                std::cout << "trie," << sample << ",1," << trieMs << ',' << index->getIndex().size() << '\n';
            }
        }
        const auto& workloads = benchmark::workloads();
        for (const auto& work : workloads) query(work.query, *index, *trie);
        for (int sample = 0; sample < samples; ++sample) {
            // Rotate order to reduce systematic first/last workload bias.
            for (std::size_t offset = 0; offset < workloads.size(); ++offset) {
                const auto& work = workloads[(offset + sample) % workloads.size()];
                std::uint64_t checksum = 0;
                const auto start = Clock::now();
                for (int i = 0; i < iterations; ++i) checksum += query(work.query, *index, *trie);
                const double elapsed = milliseconds(start);
                std::cout << work.name << ',' << sample << ',' << iterations << ',' << elapsed << ',' << checksum << '\n';
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
