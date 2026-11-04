#ifndef PREFIX_SEARCHER_HPP 
#define PREFIX_SEARCHER_HPP

#include <algorithm>
#include <string>
#include <vector>

using namespace std;
constexpr int ALPHABET_SIZE = 26;

int charToIndex(char c) { return c - 'a'; }

struct TrieNode {
  TrieNode* children[ALPHABET_SIZE];
  bool isEndOfWord;
  vector<pair<string, int>> postings;

  TrieNode() : isEndOfWord(false) {
    fill(children, children + ALPHABET_SIZE, nullptr);
  }

  ~TrieNode() {
    for (int i = 0; i < ALPHABET_SIZE; ++i) {
      if (children[i]) delete children[i];
    }
  }
};

class PrefixSearcher {
private:
  TrieNode* root;

  void suggestionsRec(TrieNode* node, string currPrefix,
                      vector<string>& suggestions) const { // MARKED CONST
    if (node->isEndOfWord) suggestions.push_back(currPrefix);
    for (int i = 0; i < ALPHABET_SIZE; i++) {
      if (node->children[i])
        suggestionsRec(node->children[i], currPrefix + char('a' + i),
                       suggestions);
    }
  }

  void collectPostingsRec(TrieNode* node, vector<pair<string, int>>& postings) const { // MARKED CONST
    if (node->isEndOfWord)
      postings.insert(postings.end(), node->postings.begin(),
                      node->postings.end());
    for (int i = 0; i < ALPHABET_SIZE; i++) {
      if (node->children[i]) collectPostingsRec(node->children[i], postings);
    }
  }

  bool isLastNode(TrieNode* node) const { // MARKED CONST
    for (int i = 0; i < ALPHABET_SIZE; i++) {
      if (node->children[i]) return false;
    }
    return true;
  }

public:
  PrefixSearcher() { root = new TrieNode; }
  ~PrefixSearcher() { delete root; }

  void insert(const string& key, const pair<string, int>& posting) {
    TrieNode* pCrawl = root;
    for (char c : key) {
      int index = charToIndex(c);
      if (!pCrawl->children[index]) pCrawl->children[index] = new TrieNode();
      pCrawl = pCrawl->children[index];
    }
    pCrawl->isEndOfWord = true;
    pCrawl->postings.push_back(posting);
  }

  vector<string> getSuggestions(const string& prefix) const { // MARKED CONST
    TrieNode* pCrawl = root;
    for (char c : prefix) {
      int index = charToIndex(c);
      if (!pCrawl->children[index]) return {};
      pCrawl = pCrawl->children[index];
    }
    vector<string> suggestions;
    if (isLastNode(pCrawl)) {
      if (pCrawl->isEndOfWord) suggestions.push_back(prefix);
      return suggestions;
    }
    suggestionsRec(pCrawl, prefix, suggestions);
    return suggestions;
  }

  vector<pair<string, int>> searchPrefix(string& prefix) const { // MARKED CONST
    TrieNode* pCrawl = root;
    for (char c : prefix) {
      int index = charToIndex(c);
      if (!pCrawl->children[index]) return {};
      pCrawl = pCrawl->children[index];
    }
    vector<pair<string, int>> postings;
    collectPostingsRec(pCrawl, postings);
    sort(postings.begin(), postings.end());
    return postings;
  }
};

#endif