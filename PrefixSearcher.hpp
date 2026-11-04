#ifndef PREFIX_SEARCHER_HPP
#define PREFIX_SEARCHER_HPP

#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Map edges support digits, apostrophes and hyphens without array indexing.
class PrefixSearcher {
private:
  struct TrieNode {
    std::map<unsigned char, std::unique_ptr<TrieNode>> children;
    std::vector<std::pair<std::string, int>> postings;
  };
  TrieNode root;

  const TrieNode* findPrefix(const std::string& prefix) const {
    const TrieNode* node = &root;
    for (unsigned char c : prefix) {
      auto it = node->children.find(static_cast<unsigned char>(std::tolower(c)));
      if (it == node->children.end()) return nullptr;
      node = it->second.get();
    }
    return node;
  }

  void collectWords(const TrieNode& node, const std::string& prefix,
                    std::vector<std::string>& words) const {
    if (!node.postings.empty()) words.push_back(prefix);
    for (const auto& [character, child] : node.children) {
      collectWords(*child, prefix + static_cast<char>(character), words);
    }
  }

  void collectPostings(const TrieNode& node, std::map<std::string, int>& totals) const {
    for (const auto& [document, frequency] : node.postings) totals[document] += frequency;
    for (const auto& [character, child] : node.children) collectPostings(*child, totals);
  }

public:
  void insert(const std::string& key, const std::pair<std::string, int>& posting) {
    if (key.empty()) return;
    TrieNode* node = &root;
    for (unsigned char c : key) {
      auto& child = node->children[static_cast<unsigned char>(std::tolower(c))];
      if (!child) child = std::make_unique<TrieNode>();
      node = child.get();
    }
    node->postings.push_back(posting);
  }

  std::vector<std::string> getSuggestions(std::string prefix) const {
    const TrieNode* node = findPrefix(prefix);
    if (!node) return {};
    for (char& c : prefix) c = std::tolower(static_cast<unsigned char>(c));
    std::vector<std::string> words;
    collectWords(*node, prefix, words);
    return words;
  }

  std::vector<std::pair<std::string, int>> searchPrefix(const std::string& prefix) const {
    const TrieNode* node = findPrefix(prefix);
    if (!node) return {};
    std::map<std::string, int> totals;
    collectPostings(*node, totals);
    return {totals.begin(), totals.end()};
  }
};
#endif
