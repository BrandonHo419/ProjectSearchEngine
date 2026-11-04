#ifndef INDEX_H
#define INDEX_H

#include <string>
#include <unordered_map>
#include <vector>

class Indexer {
private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<int, std::vector<int>>>> invertedIndex;
    std::unordered_map<std::string, int> docLen;
    std::string normalizeWord(const std::string& word) const;

     std::unordered_map<std::string, std::string> docContent;

public:
    void addDocument(const std::string& filename);
    void buildIndex(const std::vector<std::string>& files);
    std::unordered_map<std::string, std::pair<int, std::vector<int>>> searchWord(const std::string& word) const;
    
    
    int getTotalDoc() { return docLen.size(); }
    int getDocFreq(const std::string& word) const;

    // Set the window of the snippet to be 2 words long
    std::string getSnippet(const std::string& filename, const std::string& query, int window = 2) const;


    
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<int, std::vector<int>>>>& getIndex() { return invertedIndex; }
    std::unordered_map<std::string, int>& getDocLen() { return docLen; }

    const std::unordered_map<std::string, std::string>& getDocContent() const { return docContent; }
};

#endif