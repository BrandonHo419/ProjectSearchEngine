#include "Index.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// Only strip basic punctuation, keep letters to make it easier
std::string Indexer::normalizeWord(const std::string& word) const {
    std::string clean = "";
    for (size_t i = 0; i < word.size(); i++) {
        unsigned char c = static_cast<unsigned char>(word[i]);
        
    if (!std::ispunct(c) || c == '-' || c == '\'') { 
            clean += std::tolower(c);
    }
}
    return clean;
}

// Adds a document into the inverted index, 
void Indexer::addDocument(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cout << "Error with file: " << filename << "\n";
        return;
    }
    // Replacing a document must not duplicate frequencies or positions.
    for (auto& entry : invertedIndex) entry.second.erase(filename);

    std::string word, content;
    
    int total = 0, position = 0;

        // count word freq, store positions and save full document text
        while (file >> word) {
            std::string clean = normalizeWord(word);

            if (clean.empty()) continue;

            // increases the count for the current word for the current file
            invertedIndex[clean][filename].first++;

            // marks the position of the current word for the current file
            invertedIndex[clean][filename].second.push_back(position++);
            total++;
            content += word + " ";
            
        }
        docLen[filename] = total;

        // for the snippet
        docContent[filename] = content;




        file.close();
    }

// Builds the index for all the required files
void Indexer::buildIndex(const std::vector<std::string>& files) {
    invertedIndex.clear();
    docLen.clear();
    docContent.clear();
    for (const auto& file : files) {
        addDocument(file);
    }
}

// Lookups for words, used from query_processor
std::unordered_map<std::string, std::pair<int, std::vector<int>>> Indexer::searchWord(const std::string& word) const {
    std::string clean = normalizeWord(word);
    
    auto it = invertedIndex.find(clean);
    
    if (it != invertedIndex.end()) return it->second;
    
    
    
    return {};
}

int Indexer::getDocFreq(const std::string& word) const {
    std::string clean = normalizeWord(word);
    auto it = invertedIndex.find(clean);
    if (it != invertedIndex.end()) return it->second.size();
    return 0;
}

//Returns the snippet of text around the queried term(s) within document
//Shows the context of the queried term(s)
std::string Indexer::getSnippet(const std::string& filename, const std::string& query, int window) const {
    window = std::max(0, window);
    //look up full text of file
    auto it = docContent.find(filename);

    if (it == docContent.end()) return "";

    //split document into individual words
    std::vector<std::string> words;
    std::istringstream iss(it->second);
    std::string word;
    while (iss >> word) words.push_back(word);

    //Normalise the queried words
    std::vector<std::string> queryTerms;
    std::istringstream qiss(query);
    while (qiss >> word) {
        std::string cleanWord = word;

        //remove quotes or trailing *
        if (cleanWord.front() == '"' && cleanWord.back() == '"') {
            cleanWord = cleanWord.substr(1, cleanWord.size() - 2);
        }

        if (!cleanWord.empty() && cleanWord.back() == '*') {
            cleanWord.pop_back();
        }
        std::istringstream twiss(cleanWord);
        std::string token;
        while (twiss >> token) {
            std::string normalized = normalizeWord(token);
            if (!normalized.empty()) queryTerms.push_back(normalized);
        }
    }
    std::sort(queryTerms.begin(), queryTerms.end());
    queryTerms.erase(std::unique(queryTerms.begin(), queryTerms.end()), queryTerms.end());

    // Collect all match windows
    struct Window { int start, end; };
    std::vector<Window> windows;
    for (size_t i = 0; i < words.size(); ++i) {
        std::string normalizedWord = normalizeWord(words[i]);

        if (std::find(queryTerms.begin(), queryTerms.end(), normalizedWord) != queryTerms.end()) {
            int start = std::max(0, static_cast<int>(i) - window);


            int end = std::min(static_cast<int>(words.size()), static_cast<int>(i) + window + 1);
            
            windows.push_back({start, end});
        }
    }

    if (windows.empty()) {
        // fallback snippet
        return it->second.substr(0, std::min((size_t)50, it->second.size())) + "...";
    }

    // Merge overlapping windows
    std::sort(windows.begin(), windows.end(), [](const Window &a, const Window &b){ return a.start < b.start; });
    std::vector<Window> merged;
    merged.push_back(windows[0]);
    for (size_t i = 1; i < windows.size(); ++i) {
        if (windows[i].start <= merged.back().end) {
            merged.back().end = std::max(merged.back().end, windows[i].end);
        } else {
            merged.push_back(windows[i]);
        }
    }

    // Build the actual snippet string
    std::string snippet;
    for (auto &w : merged) {
        for (int i = w.start; i < w.end; ++i) {
            snippet += words[i] + " ";
        }
        snippet += "... ";
    }

    return snippet;
}




std::string Indexer::getSnippetFromSpans(const std::string& filename,
    const std::vector<MatchSpan>& spans, int window, int maxExcerpts) const {
    const auto content = docContent.find(filename);
    if (content == docContent.end() || spans.empty() || maxExcerpts <= 0) return "";
    window = std::max(0, window);
    std::vector<std::string> words;
    std::istringstream input(content->second);
    std::string word;
    while (input >> word) words.push_back(word);
    const int length = static_cast<int>(words.size());
    std::vector<MatchSpan> windows;
    for (const auto& span : spans) {
        if (span.start < 0 || span.end <= span.start || span.end > length) continue;
        // Expand complete phrase spans, so context limits cannot split a phrase.
        windows.push_back({span.start - std::min(span.start, window),
                           span.end + std::min(length - span.end, window)});
    }
    std::sort(windows.begin(), windows.end(), [](const auto& a, const auto& b) {
        return a.start < b.start;
    });
    std::vector<MatchSpan> merged;
    for (const auto& candidate : windows) {
        if (!merged.empty() && candidate.start <= merged.back().end) {
            merged.back().end = std::max(merged.back().end, candidate.end);
        } else {
            merged.push_back(candidate);
        }
    }
    std::string snippet;
    const auto count = std::min(merged.size(), static_cast<std::size_t>(maxExcerpts));
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) snippet += " ... ";
        else if (merged[i].start > 0) snippet += "... ";
        for (int position = merged[i].start; position < merged[i].end; ++position) {
            if (position > merged[i].start) snippet += " ";
            snippet += words[position];
        }
    }
    if (count > 0 && merged[count - 1].end < length) snippet += " ...";
    return snippet;
}
