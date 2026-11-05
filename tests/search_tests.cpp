#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include "query_processor.hpp"


int main() {
  namespace fs = std::filesystem;
  fs::create_directories("tests/tmp");
  const std::string a = "tests/tmp/a.txt", b = "tests/tmp/b.txt";
  const std::string c = "tests/tmp/c.txt", empty = "tests/tmp/empty.txt";
  std::ofstream(a) << "quick brown fox fox foxes don't test-case 123";
  std::ofstream(b) << "quick brown dog brown fox";
  std::ofstream(c) << "dog dog";
  std::ofstream(empty) << "";
  Indexer index;
  index.buildIndex({a, b, c, empty, a});
  assert(index.searchWord("fox").at(a).first == 2);
  PrefixSearcher trie;
  for (const auto& [word, postings] : index.getIndex())
    for (const auto& [doc, data] : postings) trie.insert(word, {doc, data.first});
  auto search = [&](const std::string& query, int limit = 10) {
    return processQuery(query, index, trie, limit);
  };
  assert(search("").empty());
  assert(search("   ").empty());
  assert(search("\"\"").empty());
  assert(search("*").empty());
  assert(search("AND fox").empty());
  assert(search("fox OR").empty());
  assert(search("fox AND OR dog").empty());
  assert(search("fox", 0).empty());
  assert(search("fox", -1).empty());
  assert(search("fox", 1).size() == 1);
  assert(search("\"quick brown fox\"").size() == 1);
  assert(search("\"quick brown fox\"")[0].first == a);
  assert(search("fox OR dog").size() == 3);
  assert(search("fox - dog").size() == 1);
  assert(search("fox dog OR 123").size() == 2);
  assert(search("FOX").size() == 2);
  assert(search("FO*").size() == 2);
  assert(search("fo*")[0].second > 0.0);
  assert(trie.searchPrefix("fo").size() == 2);
  assert(search("don't").size() == 1);
  assert(search("test-*").size() == 1);
  assert(search("12*").size() == 1);
  const double expected = 2.0 / 8.0 * (std::log(5.0 / 3.0) + 1.0);
  assert(std::abs(search("fox")[0].second - expected) < 1e-10);
  assert(!index.getSnippet(a, "\"\"").empty());
  auto detailed = processQueryDetailed("FO*", index, trie, 10);
  assert(detailed.size() == 2);
  assert(detailed[0].document == a);
  assert(detailed[0].matches.size() == 2);
  assert(detailed[0].matches[0].term == "fox");
  assert(detailed[0].matches[0].frequency == 2);
  assert((detailed[0].matches[0].positions == std::vector<int>{2, 3}));
  assert(detailed[0].matches[1].term == "foxes");
  assert(detailed[0].matches[1].frequency == 1);
  assert(!detailed[0].isPhrase);
  assert(index.getSnippetFromSpans(a, detailed[0].spans) ==
         "quick brown fox fox foxes don't test-case ...");
  assert(trie.searchPrefix("fo")[0].second == 3);

  detailed = processQueryDetailed("fox - dog", index, trie, 10);
  assert(detailed.size() == 1);
  assert(detailed[0].matches.size() == 1);
  assert(detailed[0].matches[0].term == "fox");
  detailed = processQueryDetailed("123 OR dog", index, trie, 10);
  for (const auto& result : detailed) {
    assert(result.matches.size() == 1);
    assert(result.matches[0].frequency > 0);
    assert(result.matches[0].term == (result.document == a ? "123" : "dog"));
  }
  const auto once = processQueryDetailed("fox", index, trie, 10);
  const auto twice = processQueryDetailed("fox FOX", index, trie, 10);
  assert(twice[0].matches.size() == 1);
  assert(twice[0].matches[0].frequency == 2);
  assert(std::abs(twice[0].score - 2 * once[0].score) < 1e-10);

  detailed = processQueryDetailed("\"quick brown fox\"", index, trie, 10);
  assert(detailed.size() == 1);
  assert(detailed[0].isPhrase && detailed[0].phraseOccurrences == 1);
  assert(detailed[0].spans.size() == 1);
  assert(detailed[0].spans[0].start == 0 && detailed[0].spans[0].end == 3);
  assert(detailed[0].matches[1].term == "fox" && detailed[0].matches[1].frequency == 2);
  assert(index.getSnippetFromSpans(a, detailed[0].spans, 0) == "quick brown fox ...");
  assert(processQueryDetailed("fox OR", index, trie, 10).empty());
  assert(processQueryDetailed("fox", index, trie, 0).empty());
  assert(index.getSnippetFromSpans(a, {}).empty());
  assert(index.getSnippetFromSpans("missing", {{0, 1}}).empty());
  assert(index.getSnippetFromSpans(a, {{-1, 1}, {2, 2}, {0, 100}}).empty());

  struct InvalidQueryCase {
    std::string query;
    QueryError error;
    std::string explanation;
  };
  const std::vector<InvalidQueryCase> invalidQueries = {
      {"", QueryError::EmptyQuery, "Enter a search term"},
      {" \t\r\n\v\f", QueryError::EmptyQuery, "Enter a search term"},
      {"\"\"", QueryError::EmptyPhrase, "phrase is empty"},
      {"\"   \t\"", QueryError::EmptyPhrase, "phrase is empty"},
      {"\"fox", QueryError::UnmatchedQuote, "double quote"},
      {"fox\"", QueryError::UnmatchedQuote, "double quote"},
      {"\"fox\"\"", QueryError::UnmatchedQuote, "double quote"},
      {"\"fox\" dog", QueryError::UnsupportedExpression, "entire query"},
      {"fox \"dog\"", QueryError::UnsupportedExpression, "entire query"},
      {"\"quick\" \"brown fox\"", QueryError::UnsupportedExpression, "entire query"},
      {"\"fox\" OR dog", QueryError::UnsupportedExpression, "entire query"},
      {"\"fox\"*", QueryError::UnsupportedExpression, "entire query"},
      {"(fox)", QueryError::UnsupportedExpression, "Parentheses"},
      {"fox OR (dog AND 123)", QueryError::UnsupportedExpression, "left to right"},
      {"fox)", QueryError::UnsupportedExpression, "Parentheses"},
      {"*", QueryError::InvalidWildcard, "prefix"},
      {"*fox", QueryError::InvalidWildcard, "prefix"},
      {"f*ox", QueryError::InvalidWildcard, "prefix"},
      {"fo**", QueryError::InvalidWildcard, "prefix"},
      {"fo* dog", QueryError::InvalidWildcard, "cannot be combined"},
      {"fox OR do*", QueryError::InvalidWildcard, "cannot be combined"},
      {"fox\tdo*", QueryError::InvalidWildcard, "cannot be combined"},
      {"fox\vdo*", QueryError::InvalidWildcard, "cannot be combined"},
      {"\"fo*\"", QueryError::InvalidWildcard, "inside phrases"},
      {"AND fox", QueryError::MissingOperand, "before operator 'AND'"},
      {"OR fox", QueryError::MissingOperand, "before operator 'OR'"},
      {"- fox", QueryError::MissingOperand, "before operator '-'"},
      {"fox AND", QueryError::MissingOperand, "after operator 'AND'"},
      {"fox OR", QueryError::MissingOperand, "after operator 'OR'"},
      {"fox -", QueryError::MissingOperand, "after operator '-'"},
      {"fox AND OR dog", QueryError::MissingOperand, "before operator 'OR'"},
      {"fox - - dog", QueryError::MissingOperand, "before operator '-'"},
      {"!!!", QueryError::EmptyTerm, "searchable characters"},
      {"fox OR !!!", QueryError::EmptyTerm, "searchable characters"},
      {"\"quick !!! fox\"", QueryError::EmptyTerm, "phrase term"},
      {"!!!*", QueryError::EmptyTerm, "prefix"},
  };
  for (const auto& test : invalidQueries) {
    const auto response = executeQuery(test.query, index, trie, 10);
    assert(!response.ok());
    assert(response.error == test.error);
    assert(response.results.empty());
    assert(response.message.find(test.explanation) != std::string::npos);
    assert(processQueryDetailed(test.query, index, trie, 10).empty());
    assert(search(test.query).empty());
  }
  // A well-formed search with no matches is successful, with no diagnostic.
  for (const std::string query : {"unfindableterm", "unfindable*", "\"unfindable phrase\""}) {
    const auto response = executeQuery(query, index, trie, 10);
    assert(response.ok() && response.results.empty() && response.message.empty());
  }
  for (const std::string query : {"fox", "FO*", "don't", "test-*", "12*",
                                 "\"quick brown fox\"", "fox OR dog", "fox - dog"}) {
    const auto response = executeQuery(query, index, trie, 10);
    assert(response.ok() && !response.results.empty() && response.message.empty());
  }
  assert(executeQuery("\v FOX \f", index, trie, 10).results.size() == 2);
  assert(executeQuery("fox\tdog", index, trie, 10).results.size() == 1);
  assert(executeQuery("quick, brown", index, trie, 10).results.size() == 2);
  assert(executeQuery("\"quick (brown) fox\"", index, trie, 10).results.size() == 1);
  // Do not silently add operator precedence or treat lowercase operators as keywords.
  auto leftToRight = executeQuery("123 OR dog AND fox", index, trie, 10);
  assert(leftToRight.ok() && leftToRight.results.size() == 2);
  assert(executeQuery("fox and dog", index, trie, 10).ok());
  assert(executeQuery("fox and dog", index, trie, 10).results.empty());
  assert(executeQuery("fox -dog", index, trie, 10).ok());
  assert(executeQuery("fox -dog", index, trie, 10).results.empty());
  assert(executeQuery("help", index, trie, 10).ok());
  for (int limit : {0, -1}) {
    const auto response = executeQuery("fox", index, trie, limit);
    assert(response.ok() && response.results.empty());
    assert(executeQuery("fox AND", index, trie, limit).error == QueryError::MissingOperand);
  }
  assert(executeQuery("fox", index, trie, 1).results.size() == 1);
  Indexer noDocuments;
  PrefixSearcher noTerms;
  assert(executeQuery("fox", noDocuments, noTerms, 10).ok());
  assert(executeQuery("fox", noDocuments, noTerms, 10).results.empty());
  assert(executeQuery("fox OR", noDocuments, noTerms, 10).error == QueryError::MissingOperand);

  // A phrase at the end must win over isolated occurrences near the start.
  const std::string edges = "tests/tmp/edges.txt";
  std::ofstream(edges) << "quick filler filler filler filler brown filler filler filler "
                          "fox filler filler filler filler quick brown fox";
  Indexer edgeIndex;
  edgeIndex.buildIndex({edges});
  PrefixSearcher edgeTrie;
  for (const auto& [word, postings] : edgeIndex.getIndex())
    for (const auto& [doc, data] : postings) edgeTrie.insert(word, {doc, data.first});
  auto phraseResult = processQueryDetailed("\"quick brown fox\"", edgeIndex, edgeTrie, 10);
  assert(phraseResult[0].phraseOccurrences == 1);
  assert(edgeIndex.getSnippetFromSpans(edges, phraseResult[0].spans, 0) ==
         "... quick brown fox");
  auto prefixResult = processQueryDetailed("qu*", edgeIndex, edgeTrie, 10);
  assert(edgeIndex.getSnippetFromSpans(edges, prefixResult[0].spans, 0) ==
         "quick ... quick ...");

  std::ofstream(edges) << "a a a";
  edgeIndex.buildIndex({edges});
  phraseResult = processQueryDetailed("\"a a\"", edgeIndex, edgeTrie, 10);
  assert(phraseResult[0].phraseOccurrences == 2);
  assert(phraseResult[0].matches.size() == 1 && phraseResult[0].matches[0].frequency == 3);
  assert(edgeIndex.getSnippetFromSpans(edges, phraseResult[0].spans, 0) == "a a a");
  assert(edgeIndex.getSnippetFromSpans(edges, {{0, 3}}, -1) == "a a a");
  assert(edgeIndex.getSnippetFromSpans(edges, {{0, 3}}, 2, 0).empty());

  std::ofstream(edges) << "one two three four five six seven eight nine ten";
  edgeIndex.buildIndex({edges});
  const std::vector<MatchSpan> separated = {{9, 10}, {6, 7}, {3, 4}, {0, 1}};
  assert(edgeIndex.getSnippetFromSpans(edges, separated, 0) ==
         "one ... four ... seven ...");
  assert(edgeIndex.getSnippetFromSpans(edges, separated, 0, 4) ==
         "one ... four ... seven ... ten");
  assert(edgeIndex.getSnippetFromSpans(edges, {{1, 2}, {2, 3}, {1, 2}}, 0) ==
         "... two three ...");
  // A complete long phrase remains intact even with zero context.
  phraseResult = processQueryDetailed("\"one two three four five six seven eight nine ten\"",
                                     edgeIndex, edgeTrie, 10);
  assert(edgeIndex.getSnippetFromSpans(edges, phraseResult[0].spans, 0) ==
         "one two three four five six seven eight nine ten");
  assert(edgeIndex.getSnippetFromSpans(edges, {{0, 1}}, 2147483647) ==
         "one two three four five six seven eight nine ten");

  index.buildIndex({c});
  assert(index.getTotalDoc() == 1);
  assert(index.searchWord("fox").empty());
  // Exercise prompt recovery and diagnostic routing through the real CLI on both platforms.
  std::ofstream("tests/tmp/cli-doc.txt") << "fox fox foxes quick brown fox AND help";
  const auto runCli = [](const std::string& input) {
    std::ofstream("tests/tmp/cli-input.txt") << input;
#ifdef _WIN32
    const std::string executable = ".\\search.exe";
#else
    const std::string executable = "./search";
#endif
    const std::string command = executable +
        " tests/tmp/cli-doc.txt < tests/tmp/cli-input.txt > tests/tmp/cli-output.txt 2>&1";
    const int status = std::system(command.c_str());
    assert(status == 0);
    std::ifstream output("tests/tmp/cli-output.txt");
    assert(output);
    return std::string(std::istreambuf_iterator<char>(output), std::istreambuf_iterator<char>());
  };
  const auto invalidOutput = runCli("\n :help \n*\nfox AND\n\"fox\n");
  assert(invalidOutput.find("Query help:") != std::string::npos);
  assert(invalidOutput.find("left to right") != std::string::npos);
  assert(invalidOutput.find("Invalid query:") != std::string::npos);
  assert(invalidOutput.find("Missing search term after operator 'AND'") != std::string::npos);
  assert(invalidOutput.find("Unmatched double quote") != std::string::npos);
  assert(invalidOutput.find("No results found.") == std::string::npos);
  assert(invalidOutput.find("Suggestions:") == std::string::npos);
  assert(invalidOutput.find("Goodbye!") != std::string::npos);
  const auto validOutput = runCli("unfindableterm\nfo*\n\"quick brown fox\"\nx\nAND fox\n");
  assert(validOutput.find("No results found.") != std::string::npos);
  assert(validOutput.find("Suggestions: fox foxes") != std::string::npos);
  assert(validOutput.find("fox: 3 foxes: 1") != std::string::npos);
  assert(validOutput.find("Phrase occurrences: 1") != std::string::npos);
  assert(validOutput.find("Invalid query:") == std::string::npos);
  const auto literalOutput = runCli("help\n\"AND\"\n");
  assert(literalOutput.find("help: 1") != std::string::npos);
  assert(literalOutput.find("and: 1") != std::string::npos);
  assert(literalOutput.find("Query help:") == std::string::npos);
  assert(literalOutput.find("Invalid query:") == std::string::npos);

  fs::remove_all("tests/tmp");
  std::cout << "Search regression checks passed.\n";
}
