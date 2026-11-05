#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
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
  fs::remove_all("tests/tmp");
  std::cout << "Search regression checks passed.\n";
}
