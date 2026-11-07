// Native oracle for the optional Node/WebAssembly parity checks.
#include <iomanip>
#include <iostream>
#include <string>
#include "query_processor.hpp"

void stringValue(const std::string& value) {
    std::cout << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') std::cout << '\\' << c;
        else if (c < 32) {
            const char* hex = "0123456789abcdef";
            std::cout << "\\u00" << hex[c >> 4] << hex[c & 15];
        } else std::cout << c;
    }
    std::cout << '"';
}

int main(int argc, char* argv[]) {
    if (argc < 4) return 2;
    Indexer index;
    index.buildIndex(std::vector<std::string>(argv + 3, argv + argc));
    if (index.getTotalDoc() != argc - 3) return 1;
    PrefixSearcher trie;
    for (const auto& [word, postings] : index.getIndex())
        for (const auto& [id, data] : postings) trie.insert(word, {id, data.first});
    const auto response = executeQuery(argv[1], index, trie, std::stoi(argv[2]));
    const char* names[] = {"None", "EmptyQuery", "EmptyPhrase", "UnmatchedQuote",
                          "UnsupportedExpression", "InvalidWildcard", "MissingOperand", "EmptyTerm"};
    std::cout << std::setprecision(17) << std::boolalpha;
    std::cout << "{\"ok\":" << response.ok() << ",\"error\":";
    stringValue(names[static_cast<int>(response.error)]);
    std::cout << ",\"message\":";
    stringValue(response.message);
    std::cout << ",\"suggestions\":[";
    for (std::size_t i = 0; i < response.suggestions.size(); ++i) {
        if (i) std::cout << ',';
        stringValue(response.suggestions[i]);
    }
    std::cout << "],\"results\":[";
    for (std::size_t i = 0; i < response.results.size(); ++i) {
        if (i) std::cout << ',';
        const auto& result = response.results[i];
        std::cout << "{\"document\":";
        stringValue(result.document);
        std::cout << ",\"score\":" << result.score
                  << ",\"wordCount\":" << index.getDocLen().at(result.document)
                  << ",\"snippet\":";
        stringValue(index.getSnippetFromSpans(result.document, result.spans));
        std::cout << ",\"isPhrase\":" << result.isPhrase
                  << ",\"phraseOccurrences\":" << result.phraseOccurrences << ",\"matches\":[";
        for (std::size_t j = 0; j < result.matches.size(); ++j) {
            if (j) std::cout << ',';
            const auto& match = result.matches[j];
            std::cout << "{\"term\":";
            stringValue(match.term);
            std::cout << ",\"frequency\":" << match.frequency << ",\"positions\":[";
            for (std::size_t k = 0; k < match.positions.size(); ++k) {
                if (k) std::cout << ',';
                std::cout << match.positions[k];
            }
            std::cout << "]}";
        }
        std::cout << "],\"spans\":[";
        for (std::size_t j = 0; j < result.spans.size(); ++j) {
            if (j) std::cout << ',';
            std::cout << "{\"start\":" << result.spans[j].start
                      << ",\"end\":" << result.spans[j].end << '}';
        }
        std::cout << "]}";
    }
    std::cout << "]}\n";
}
