#ifndef QUERY_PROCESSOR_HPP
#define QUERY_PROCESSOR_HPP

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_map>
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

enum class QueryError {
  None,
  EmptyQuery,
  EmptyPhrase,
  UnmatchedQuote,
  UnsupportedExpression,
  InvalidWildcard,
  MissingOperand,
  EmptyTerm
};

struct QueryResponse {
  std::vector<SearchResult> results;
  QueryError error = QueryError::None;
  std::string message;
  // Valid prefix expansions are reusable by any presentation layer.
  std::vector<std::string> suggestions = {};

  bool ok() const { return error == QueryError::None; }
};

// Report malformed input separately from valid searches with no matching documents.
// Boolean operations are evaluated from left to right; adjacent terms imply AND.
inline QueryResponse executeQuery(
    const std::string& input, Indexer& index,
    const PrefixSearcher& prefixSearcher, int topK) {
  const auto fail = [](QueryError error, const std::string& message) {
    return QueryResponse{{}, error, message};
  };
  const std::string whitespace = " \t\r\n\v\f";
  const auto first = input.find_first_not_of(whitespace);
  if (first == std::string::npos) {
    return fail(QueryError::EmptyQuery, "Enter a search term, a quoted phrase or a prefix such as fo*.");
  }
  const std::string query = input.substr(first, input.find_last_not_of(whitespace) - first + 1);
  const auto quotes = std::count(query.begin(), query.end(), '"');
  if (quotes % 2 != 0) {
    return fail(QueryError::UnmatchedQuote, "Unmatched double quote. Close the phrase with a double quote.");
  }
  const bool phrase = quotes == 2 && query.front() == '"' && query.back() == '"';
  if (quotes != 0 && !phrase) {
    return fail(QueryError::UnsupportedExpression,
                "Use one quoted phrase as the entire query; mixed phrase expressions are unsupported.");
  }
  if (!phrase && query.find_first_of("()") != std::string::npos) {
    return fail(QueryError::UnsupportedExpression,
                "Parentheses are unsupported. Boolean operations are evaluated from left to right.");
  }
  const auto stars = std::count(query.begin(), query.end(), '*');
  const bool prefix = !phrase && stars != 0;
  if (phrase && stars != 0) {
    return fail(QueryError::InvalidWildcard, "Wildcards are unsupported inside phrases. Use a standalone prefix such as fo*.");
  }
  if (prefix && (stars != 1 || query.back() != '*' || query.size() == 1 ||
                 query.find_first_of(whitespace) != std::string::npos)) {
    return fail(QueryError::InvalidWildcard,
                "Use one non-empty prefix followed by *, such as fo*. Prefixes cannot be combined with other terms.");
  }
  ExactSearcher exact;
  std::vector<std::pair<std::string, int>> candidates;
  std::vector<std::string> scoringTerms;
  std::unordered_map<std::string, std::vector<int>> phraseEnds;

  if (phrase) {
    const std::string text = query.substr(1, query.size() - 2);
    std::istringstream words(text);
    std::string word;
    while (words >> word) {
      if (index.normalizeWord(word).empty()) {
        return fail(QueryError::EmptyTerm, "Each phrase term must contain searchable characters; remove punctuation-only terms.");
      }
      scoringTerms.push_back(word);
    }
    if (scoringTerms.empty()) {
      return fail(QueryError::EmptyPhrase, "The phrase is empty. Put one or more words between the double quotes.");
    }
    phraseEnds = exact.searchPhrasePositions(text, index);
    for (const auto& [document, ends] : phraseEnds) {
      candidates.emplace_back(document, ends.size());
    }
  } else if (prefix) {
    const std::string text = query.substr(0, query.size() - 1);
    if (index.normalizeWord(text).empty()) {
      return fail(QueryError::EmptyTerm, "The prefix must contain searchable characters before *.");
    }
    candidates = prefixSearcher.searchPrefix(text);
    scoringTerms = prefixSearcher.getSuggestions(text);
  } else {
    std::istringstream words(query);
    std::string word, operation = "AND";
    bool haveTerm = false, pendingOperator = false;
    while (words >> word) {
      if (word == "AND" || word == "OR" || word == "-") {
        if (!haveTerm || pendingOperator) {
          return fail(QueryError::MissingOperand, "Missing search term before operator '" + word + "'.");
        }
        operation = word;
        pendingOperator = true;
        continue;
      }
      if (index.normalizeWord(word).empty()) {
        return fail(QueryError::EmptyTerm, "Each search term must contain searchable characters; remove punctuation-only terms.");
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
    if (pendingOperator) {
      return fail(QueryError::MissingOperand, "Missing search term after operator '" + operation + "'.");
    }
  }

  // A result limit is an output option, not query syntax. Still validate when it is zero.
  QueryResponse response;
  if (prefix) response.suggestions = scoringTerms;
  if (topK <= 0) return response;

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
  response.results = std::move(ranked);
  return response;
}

// Compatibility callers retain vector results; use executeQuery() for diagnostics.
inline std::vector<SearchResult> processQueryDetailed(
    const std::string& input, Indexer& index,
    const PrefixSearcher& prefixSearcher, int topK) {
  return executeQuery(input, index, prefixSearcher, topK).results;
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
