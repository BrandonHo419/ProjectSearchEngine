#include <cassert>
#include <iostream>
#include <sstream>
#include "benchmarks/corpus.hpp"
#include "query_processor.hpp"

int main() {
    const auto documents = benchmark::corpus(40, 16, 1234);
    assert(documents.size() == 40);
    // Independently calculated canonical corpus-v1 fingerprint.
    assert(benchmark::fingerprint(documents) == UINT64_C(0x63c92f643dabc9d9));
    const auto repeat = benchmark::corpus(40, 16, 1234);
    for (std::size_t i = 0; i < documents.size(); ++i) {
        assert(documents[i].id == "synthetic:" + std::to_string(i));
        assert(documents[i].text == repeat[i].text);
        std::istringstream words(documents[i].text);
        int count = 0;
        for (std::string word; words >> word;) ++count;
        assert(count == 16);
    }
    assert(benchmark::fingerprint(documents) != benchmark::fingerprint(benchmark::corpus(40, 16, 1235)));
    for (const auto& dimensions : std::vector<std::pair<int, int>>{{0, 16}, {1, 15}, {10001, 16}, {1000, 4096}}) {
        bool rejected = false;
        try { benchmark::corpus(dimensions.first, dimensions.second, 0); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    Indexer index;
    index.buildIndexFromText(documents);
    PrefixSearcher trie;
    for (const auto& [word, postings] : index.getIndex())
        for (const auto& [id, data] : postings) trie.insert(word, {id, data.first});
    const std::vector<int> expected = {40, 2, 10, -1, 2, 10, 30, 0};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto response = executeQuery(benchmark::workloads()[i].query, index, trie, 100);
        assert(response.ok());
        if (expected[i] >= 0) assert(response.results.size() == static_cast<std::size_t>(expected[i]));
        else {
            int matches = 0;
            for (const auto& doc : documents) if (doc.text.find("tok000") != std::string::npos) ++matches;
            assert(response.results.size() == static_cast<std::size_t>(matches));
            assert(!response.suggestions.empty() && response.suggestions.size() <= 10);
        }
        for (const auto& result : response.results) {
            assert(!index.getSnippetFromSpans(result.document, result.spans).empty());
            if (i == 2) assert(result.phraseOccurrences == 1);
        }
        const auto limited = executeQuery(benchmark::workloads()[i].query, index, trie, 10);
        assert(limited.results.size() == std::min<std::size_t>(10, response.results.size()));
    }
    std::cout << "Benchmark fixture checks passed. Fingerprint: " << std::hex
              << benchmark::fingerprint(documents) << '\n';
}
