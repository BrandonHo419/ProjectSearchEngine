#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "Index.h"
#include "PrefixSearcher.hpp"
#include "query_processor.hpp"

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

void displayQueryHelp() {
  std::cout << R"(Query help:
  fox                  Single term (case-insensitive)
  quick brown          Both terms (implicit AND)
  fox AND dog          Both terms
  fox OR secret        Either term
  fox - dog            Exclude dog; separate '-' with spaces
  "quick brown fox"    One complete phrase
  fo*                  One prefix followed by '*'

Boolean operators must be uppercase and are evaluated left to right.
Lowercase 'and' and 'or' are ordinary search terms.
Parentheses and mixed phrase/prefix expressions are unsupported.
Use a quoted phrase to search literal operator words, e.g. "AND".
Enter :help for this help, x to quit, or a query to continue.
)";
}

int main(int argc, char* argv[]) {
  std::cout << "=== Mini Search Engine CLI ===\n";

  // IMPORTANT: These files must exist in the same directory for indexing to
  // work.
  std::vector<std::string> files = {"data/sample/testfile1.txt", "data/sample/testfile2.txt",
                                    "data/sample/testfile3.txt", "data/sample/extreme secret.txt"};

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
    std::cout << "\nEnter search query (:help for help, x to quit): ";
    std::string query;
    if (!std::getline(std::cin, query)) break;
    const auto first = query.find_first_not_of(" \t\r\n\v\f");
    if (first == std::string::npos) continue;
    query = query.substr(first, query.find_last_not_of(" \t\r\n\v\f") - first + 1);

    if (query == "x") break;
    if (query == ":help") {
      displayQueryHelp();
      continue;
    }

    auto response = executeQuery(query, indexer, prefixSearcher, 10);
    if (!response.ok()) {
      std::cout << "Invalid query: " << response.message << " Type :help for examples.\n";
      continue;
    }

    // Only valid prefix queries reach suggestion expansion.
    if (query.back() == '*' && !(query.front() == '"' && query.back() == '"')) {
      displaySuggestions(query, prefixSearcher);
    }

    const auto& results = response.results;

    if (results.empty()) {
      std::cout << "No results found.\n";
    } else {
      std::cout << "\nSearch results (ranked):\n";
      for (size_t i = 0; i < results.size(); ++i) {
        const std::string& doc = results[i].document;
        double score = results[i].score;

        // Use .count() and .at() for safer map access
        int totalWords =
            indexer.getDocLen().count(doc) ? indexer.getDocLen().at(doc) : 0;

        std::cout << i + 1 << ". " << doc << " | Score: " << score
                  << " | Total words: " << totalWords << "\n";

        // Print snippet
        std::cout << "   Snippet: " << indexer.getSnippetFromSpans(doc, results[i].spans) << "\n";

        std::cout << "   Matched terms (occurrences in document): ";
        for (const auto& match : results[i].matches) {
          std::cout << match.term << ": " << match.frequency << " ";
        }
        if (results[i].isPhrase) {
          std::cout << "| Phrase occurrences: " << results[i].phraseOccurrences;
        }
        std::cout << "\n";
      }
    }
  }

  std::cout << "Goodbye!\n";
  return 0;
}
