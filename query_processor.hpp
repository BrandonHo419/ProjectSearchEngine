#ifndef QUERY_PROCESSOR_HPP
#define QUERY_PROCESSOR_HPP

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include "ExactSearcher.hpp"
#include "PrefixSearcher.hpp"
#include "Index.h"

struct TermMatch {
  std::string term;
  int frequency;
  std::vector<int> positions;
};

struct SearchResult {
  std::string document;
  double score;
  std::vector<TermMatch> matches;
  std::vector<MatchSpan> spans;
  bool isPhrase = false;
  int phraseOccurrences = 0;
};

// Boolean operations are evaluated from left to right; adjacent terms imply AND.
inline std::vector<SearchResult> processQueryDetailed(
    const std::string& input, Indexer& index,
    const PrefixSearcher& prefixSearcher, int topK) {
  const auto first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos || topK <= 0) return {};
  const std::string query = input.substr(first, input.find_last_not_of(" \t\r\n") - first + 1);
  const bool phrase = query.size() >= 2 && query.front() == '"' && query.back() == '"';
  const bool prefix = !phrase && query.back() == '*';
  ExactSearcher exact;
  std::vector<std::pair<std::string, int>> candidates;
  std::vector<std::string> scoringTerms;
  std::unordered_map<std::string, std::vector<int>> phraseEnds;

  if (phrase) {
    const std::string text = query.substr(1, query.size() - 2);
    phraseEnds = exact.searchPhrasePositions(text, index);
    for (const auto& [document, ends] : phraseEnds) {
      candidates.emplace_back(document, ends.size());
    }
    std::istringstream words(text);
    std::string word;
    while (words >> word) scoringTerms.push_back(word);
  } else if (prefix) {
    std::string text = query.substr(0, query.size() - 1);
    if (text.empty() || text.find_first_of(" \t\r\n\"*") != std::string::npos) return {};
    candidates = prefixSearcher.searchPrefix(text);
    scoringTerms = prefixSearcher.getSuggestions(text);
  } else {
    if (query.find_first_of("\"*") != std::string::npos) return {};
    std::istringstream words(query);
    std::string word, operation = "AND";
    bool haveTerm = false, pendingOperator = false;
    while (words >> word) {
      if (word == "AND" || word == "OR" || word == "-") {
        if (!haveTerm || pendingOperator) return {};
        operation = word;
        pendingOperator = true;
        continue;
      }
      auto next = exact.search(word, index, false);
      if (!haveTerm) candidates = next;
      else if (operation == "AND") candidates = exact.intersectPostings(candidates, next);
      else if (operation == "OR") candidates = exact.unionPostings(candidates, next);
      else {
        std::unordered_set<std::string> excluded;
        for (const auto& posting : next) excluded.insert(posting.first);
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [&](const auto& posting) { return excluded.count(posting.first) != 0; }), candidates.end());
      }
      if (!haveTerm || operation != "-") scoringTerms.push_back(word);
      haveTerm = true;
      pendingOperator = false;
      operation = "AND";
    }
    if (pendingOperator) return {};
  }

  // Smoothed per-term TF-IDF avoids integer division and zero-only rankings.
  std::vector<SearchResult> ranked;
  for (const auto& [document, frequency] : candidates) {
    const int length = index.getDocLen().at(document);
    if (length == 0) continue;
    SearchResult result{document, 0.0, {}, {}, phrase, 0};
    std::unordered_set<std::string> explainedTerms;
    for (const auto& term : scoringTerms) {
      const auto postings = index.searchWord(term);
      const auto match = postings.find(document);
      if (match == postings.end()) continue;
      const double idf = std::log((index.getTotalDoc() + 1.0) / (postings.size() + 1.0)) + 1.0;
      result.score += (match->second.first / static_cast<double>(length)) * idf;
      const std::string normalized = index.normalizeWord(term);
      // Repeated terms still affect ranking, but each frequency is displayed once.
      if (explainedTerms.insert(normalized).second) {
        result.matches.push_back({normalized, match->second.first, match->second.second});
        if (!phrase) {
          for (int position : match->second.second) result.spans.push_back({position, position + 1});
        }
      }
    }
    if (phrase) {
      const auto& ends = phraseEnds.at(document);
      result.phraseOccurrences = static_cast<int>(ends.size());
      for (int end : ends) {
        result.spans.push_back({end + 1 - static_cast<int>(scoringTerms.size()), end + 1});
      }
    }
    std::sort(result.matches.begin(), result.matches.end(), [](const auto& a, const auto& b) {
      return a.term < b.term;
    });
    std::sort(result.spans.begin(), result.spans.end(), [](const auto& a, const auto& b) {
      return a.start < b.start;
    });
    ranked.push_back(std::move(result));
  }
  std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
    if (a.score != b.score) return a.score > b.score;
    return a.document < b.document;
  });
  if (ranked.size() > static_cast<std::size_t>(topK)) ranked.resize(topK);
  return ranked;
}
// Keep the original ranking-only interface available to existing callers.
inline std::vector<std::pair<std::string, double>> processQuery(
    const std::string& input, Indexer& index,
    const PrefixSearcher& prefixSearcher, int topK) {
  std::vector<std::pair<std::string, double>> results;
  for (const auto& result : processQueryDetailed(input, index, prefixSearcher, topK)) {
    results.emplace_back(result.document, result.score);
  }
  return results;
}
#endif
