#ifndef INDEX_H
#define INDEX_H

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

// Half-open range in indexed token positions.
struct MatchSpan {
    int start;
    int end;
};

struct TextDocument {
    std::string id;
    std::string text;
};

class Indexer {
private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<int, std::vector<int>>>> invertedIndex;
    std::unordered_map<std::string, int> docLen;

     std::unordered_map<std::string, std::string> docContent;

    void clearIndex();
    void indexDocument(const std::string& documentId, std::istream& input);

public:
    std::string normalizeWord(const std::string& word) const;
    void addDocument(const std::string& filename);
    void buildIndex(const std::vector<std::string>& files);
    // Text IDs are opaque labels, not paths. Empty IDs throw std::invalid_argument.
    void addDocumentText(const std::string& documentId, const std::string& text);
    void buildIndexFromText(const std::vector<TextDocument>& documents);
    std::unordered_map<std::string, std::pair<int, std::vector<int>>> searchWord(const std::string& word) const;
    
    
    int getTotalDoc() { return docLen.size(); }
    int getDocFreq(const std::string& word) const;

    std::string getSnippetFromSpans(const std::string& filename,
        const std::vector<MatchSpan>& spans, int window = 2, int maxExcerpts = 3) const;

    // Set the window of the snippet to be 2 words long
    std::string getSnippet(const std::string& filename, const std::string& query, int window = 2) const;


    
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<int, std::vector<int>>>>& getIndex() { return invertedIndex; }
    std::unordered_map<std::string, int>& getDocLen() { return docLen; }

    const std::unordered_map<std::string, std::string>& getDocContent() const { return docContent; }
};

#endif
