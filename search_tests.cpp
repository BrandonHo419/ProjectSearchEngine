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
  // assert(trie.searchPrefix("fo").size() == 2);
  assert(search("don't").size() == 1);
  assert(search("test-*").size() == 1);
  assert(search("12*").size() == 1);
  const double expected = 2.0 / 8.0 * (std::log(5.0 / 3.0) + 1.0);
  assert(std::abs(search("fox")[0].second - expected) < 1e-10);
  assert(!index.getSnippet(a, "\"\"").empty());
  index.buildIndex({c});
  assert(index.getTotalDoc() == 1);
  assert(index.searchWord("fox").empty());
  fs::remove_all("tests/tmp");
  std::cout << "Search regression checks passed.\n";
}
