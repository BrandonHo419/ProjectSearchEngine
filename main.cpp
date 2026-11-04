#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "Index.h"
#include "PrefixSearcher.hpp"
#include "query_processor.hpp"

// Utility function to normalize a word (duplicate from Index.cpp for local use)
std::string normalizeWord(const std::string& word) {
  std::string clean = "";
  for (size_t i = 0; i < word.size(); i++) {
    unsigned char c = static_cast<unsigned char>(word[i]);
    if (!std::ispunct(c) || c == '-' || c == '\'') {
      clean += std::tolower(c);
    }
  }
  return clean;
}

void displaySuggestions(const std::string& prefix,
                        const PrefixSearcher& prefixSearcher) {
  // Note: getSuggestions expects a prefix without the '*'
  std::string clean_prefix =
      prefix.back() == '*' ? prefix.substr(0, prefix.size() - 1) : prefix;
  auto suggestions = prefixSearcher.getSuggestions(clean_prefix);

  if (!suggestions.empty()) {
    std::cout << "Suggestions: ";
    for (const auto& s : suggestions) {
      std::cout << s << " ";
    }
    std::cout << "\n";
  }
}

int main(int argc, char* argv[]) {
  std::cout << "=== Mini Search Engine CLI ===\n";

  // IMPORTANT: These files must exist in the same directory for indexing to
  // work.
  std::vector<std::string> files = {"testfile1.txt", "testfile2.txt",
                                    "testfile3.txt", "extreme secret.txt"};

  if (argc > 1) files.assign(argv + 1, argv + argc);

  Indexer indexer;
  PrefixSearcher prefixSearcher;
  indexer.buildIndex(files);

  if (indexer.getTotalDoc() == 0) {
    std::cerr << "No documents could be loaded. Pass readable text file paths.\n";
    return 1;
  }

  // Build PrefixSearcher (Trie) from the Indexer's terms
  for (const auto& [word, postings] : indexer.getIndex()) {
    for (const auto& [doc, data] : postings) {
      // 'word' is already normalized
      prefixSearcher.insert(word, {doc, data.first});
    }
  }
  std::cout << "Index built successfully!\n";

  while (true) {
    std::cout << "\nEnter search query (press 'x' to quit): ";
    std::string query;
    if (!std::getline(std::cin, query)) break;
    const auto first = query.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) continue;
    query = query.substr(first, query.find_last_not_of(" \t\r\n") - first + 1);

    if (query == "x") break;
    if (query.empty()) continue;

    // Display suggestions if it's a prefix query
    if (query.back() == '*' && !(query.front() == '"' && query.back() == '"')) {
      displaySuggestions(query, prefixSearcher);
    }

    // Process the query using the combined logic
    auto results = processQuery(query, indexer, prefixSearcher, 10);

    if (results.empty()) {
      std::cout << "No results found.\n";
    } else {
      std::cout << "\nSearch results (ranked):\n";
      for (size_t i = 0; i < results.size(); ++i) {
        const std::string& doc = results[i].first;
        double score = results[i].second;

        // Use .count() and .at() for safer map access
        int totalWords =
            indexer.getDocLen().count(doc) ? indexer.getDocLen().at(doc) : 0;

        std::cout << i + 1 << ". " << doc << " | Score: " << score
                  << " | Total words: " << totalWords << "\n";

        // Print snippet
        std::cout << "   Snippet: " << indexer.getSnippet(doc, query) << "\n";

        // Print word frequencies (matches)
        std::cout << "   Matches: ";
        std::istringstream iss(query);
        std::string word_token;

        std::unordered_set<std::string> seen_terms;

        while (iss >> word_token) {
          if (word_token == "AND" || word_token == "OR" || word_token == "-")
            continue;

          std::string clean_word = word_token;
          // Pre-clean for normalization logic
          if (clean_word.front() == '"' && clean_word.back() == '"')
            clean_word = clean_word.substr(1, clean_word.size() - 2);
          if (!clean_word.empty() && clean_word.back() == '*') clean_word.pop_back();

          // Tokenize the (potentially cleaned) word/phrase to handle multi-word
          // inputs
          std::istringstream twiss(clean_word);
          std::string token;
          while (twiss >> token) {
            std::string normalized_token = normalizeWord(token);

            if (!normalized_token.empty() &&
                seen_terms.find(normalized_token) == seen_terms.end()) {
              auto postings = indexer.searchWord(normalized_token);
              int freq = postings.count(doc) ? postings.at(doc).first : 0;
              std::cout << normalized_token << "(" << freq << ") ";
              seen_terms.insert(normalized_token);
            }
          }
        }
        std::cout << "\n";
      }
    }
  }

  std::cout << "Goodbye!\n";
  return 0;
}