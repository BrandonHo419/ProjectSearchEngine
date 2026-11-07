#include <memory>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "query_processor.hpp"

namespace {
using emscripten::val;

const char* errorName(QueryError error) {
    switch (error) {
        case QueryError::None: return "None";
        case QueryError::EmptyQuery: return "EmptyQuery";
        case QueryError::EmptyPhrase: return "EmptyPhrase";
        case QueryError::UnmatchedQuote: return "UnmatchedQuote";
        case QueryError::UnsupportedExpression: return "UnsupportedExpression";
        case QueryError::InvalidWildcard: return "InvalidWildcard";
        case QueryError::MissingOperand: return "MissingOperand";
        case QueryError::EmptyTerm: return "EmptyTerm";
    }
    return "Unknown";
}

template<typename T>
val arrayOf(const std::vector<T>& values) {
    val array = val::array();
    for (const auto& value : values) array.call<void>("push", value);
    return array;
}

struct Corpus {
    Indexer index;
    PrefixSearcher trie;

    explicit Corpus(const std::vector<TextDocument>& documents) {
        index.buildIndexFromText(documents);
        for (const auto& [word, postings] : index.getIndex()) {
            for (const auto& [id, data] : postings) trie.insert(word, {id, data.first});
        }
    }
};

class SearchEngine {
    std::unique_ptr<Corpus> corpus = std::make_unique<Corpus>(std::vector<TextDocument>{});

public:
    int loadDocuments(const std::vector<TextDocument>& documents) {
        // Install index and trie together, only after the replacement is built.
        auto replacement = std::make_unique<Corpus>(documents);
        corpus.swap(replacement);
        return corpus->index.getTotalDoc();
    }

    val search(const std::string& query, int topK) {
        const auto response = executeQuery(query, corpus->index, corpus->trie, topK);
        val output = val::object();
        output.set("ok", response.ok());
        output.set("error", std::string(errorName(response.error)));
        output.set("message", response.message);
        output.set("suggestions", arrayOf(response.suggestions));
        val results = val::array();
        for (const auto& result : response.results) {
            val item = val::object();
            item.set("document", result.document);
            item.set("score", result.score);
            item.set("wordCount", corpus->index.getDocLen().at(result.document));
            item.set("snippet", corpus->index.getSnippetFromSpans(result.document, result.spans));
            item.set("isPhrase", result.isPhrase);
            item.set("phraseOccurrences", result.phraseOccurrences);
            val matches = val::array();
            for (const auto& match : result.matches) {
                val term = val::object();
                term.set("term", match.term);
                term.set("frequency", match.frequency);
                term.set("positions", arrayOf(match.positions));
                matches.call<void>("push", term);
            }
            item.set("matches", matches);
            val spans = val::array();
            for (const auto& span : result.spans) {
                val range = val::object();
                range.set("start", span.start);
                range.set("end", span.end);
                spans.call<void>("push", range);
            }
            item.set("spans", spans);
            results.call<void>("push", item);
        }
        output.set("results", results);
        return output;
    }
};
} // namespace

EMSCRIPTEN_BINDINGS(project_search_engine) {
    emscripten::value_object<TextDocument>("TextDocument")
        .field("id", &TextDocument::id)
        .field("text", &TextDocument::text);
    emscripten::register_vector<TextDocument>("TextDocuments");
    emscripten::class_<SearchEngine>("SearchEngine")
        .constructor<>()
        .function("loadDocuments", &SearchEngine::loadDocuments)
        .function("search", &SearchEngine::search);
}
