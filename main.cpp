#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "Index.h"
#include "PrefixSearcher.hpp"
#include "query_processor.hpp"

void displaySuggestions(const std::vector<std::string>& suggestions) {
  if (!suggestions.empty()) {
    std::cout << "Suggestions: ";
    for (const auto& s : suggestions) {
      std::cout << s << " ";
    }
    std::cout << "\n";
  }
}

void displayQueryHelp() {
  std::cout << R"(Query help:
  fox                  Single term (case-insensitive)
  quick brown          Both terms (implicit AND)
  fox AND dog          Both terms
  fox OR secret        Either term
  fox - dog            Exclude dog; separate '-' with spaces
  "quick brown fox"    One complete phrase
  fo*                  One prefix followed by '*'

Boolean operators must be uppercase and are evaluated left to right.
Lowercase 'and' and 'or' are ordinary search terms.
Parentheses and mixed phrase/prefix expressions are unsupported.
Use a quoted phrase to search literal operator words, e.g. "AND".
Enter :help for this help, x to quit, or a query to continue.
)";
}

std::string trimQuery(const std::string& query) {
  const auto first = query.find_first_not_of(" \t\r\n\v\f");
  if (first == std::string::npos) return "";
  return query.substr(first, query.find_last_not_of(" \t\r\n\v\f") - first + 1);
}

int runQuery(const std::string& input, Indexer& indexer,
             const PrefixSearcher& prefixSearcher) {
  auto response = executeQuery(input, indexer, prefixSearcher, 10);
  if (!response.ok()) {
    std::cout << "Invalid query: " << response.message << " Use --help or :help for examples.\n";
    return 2;
  }

  displaySuggestions(response.suggestions);

  const auto& results = response.results;

  if (results.empty()) {
    std::cout << "No results found.\n";
  } else {
    std::cout << "\nSearch results (ranked):\n";
    for (size_t i = 0; i < results.size(); ++i) {
      const std::string& doc = results[i].document;
      double score = results[i].score;

      // Use .count() and .at() for safer map access
      int totalWords =
          indexer.getDocLen().count(doc) ? indexer.getDocLen().at(doc) : 0;

      std::cout << i + 1 << ". " << doc << " | Score: " << score
                << " | Total words: " << totalWords << "\n";

      // Print snippet
      std::cout << "   Snippet: " << indexer.getSnippetFromSpans(doc, results[i].spans) << "\n";

      std::cout << "   Matched terms (occurrences in document): ";
      for (const auto& match : results[i].matches) {
        std::cout << match.term << ": " << match.frequency << " ";
      }
      if (results[i].isPhrase) {
        std::cout << "| Phrase occurrences: " << results[i].phraseOccurrences;
      }
      std::cout << "\n";
    }
  }
  return 0;
}

void displayUsage() {
  std::cout << R"(Usage: search [files...] [--file PATH] [--directory PATH] [--query QUERY]
  --help            Show command-line and query help without indexing
  --file PATH       Add one text file (any extension); repeatable
  --directory PATH  Add immediate regular .txt files (case-insensitive extension)
  --query QUERY     Run one query and exit without reading stdin; specify once
  --                Treat all remaining arguments as literal file paths

With no input paths, use bundled samples relative to the working directory.
Quote paths and queries containing spaces. Each value option consumes the next
argument literally. Directory scans skip subdirectories and symbolic links.
Canonical path duplicates are indexed once; the first path spelling is shown.
Empty files are indexed but cannot match. Unavailable inputs are reported and
skipped; no sample fallback occurs when explicit inputs cannot be loaded.
Exit codes: 0 = success (including no matches), 1 = no documents loaded,
2 = invalid arguments or invalid single query. Interactive query errors recover.

)";
  displayQueryHelp();
}

struct InputPath {
  std::string path;
  bool directory;
};

struct Options {
  std::vector<InputPath> inputs;
  std::string query;
  bool singleQuery = false;
  bool help = false;
};

bool parseOptions(int argc, char* argv[], Options& options) {
  bool positionalOnly = false;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (!positionalOnly && argument == "--") {
      positionalOnly = true;
    } else if (!positionalOnly && argument == "--help") {
      options.help = true;
    } else if (!positionalOnly &&
               (argument == "--file" || argument == "--directory" || argument == "--query")) {
      if (i + 1 == argc) {
        std::cerr << "Missing value for " << argument << ".\n";
        return false;
      }
      const std::string value = argv[++i];
      if (argument == "--query") {
        if (options.singleQuery) {
          std::cerr << "Specify --query only once.\n";
          return false;
        }
        options.singleQuery = true;
        options.query = value;
      } else {
        options.inputs.push_back({value, argument == "--directory"});
      }
    } else if (!positionalOnly && argument.rfind("--", 0) == 0) {
      std::cerr << "Unknown option: " << argument << ". Use -- before literal paths.\n";
      return false;
    } else {
      options.inputs.push_back({argument, false});
    }
  }
  return true;
}

std::vector<std::string> collectFiles(const std::vector<InputPath>& inputs) {
  namespace fs = std::filesystem;
  std::vector<std::string> files;
  std::set<fs::path> seen;
  const auto addFile = [&](const fs::path& path) {
    std::error_code error;
    if (!fs::is_regular_file(path, error)) {
      std::cerr << "Cannot use file: " << path.string() << " ("
                << (error ? error.message() : "not a regular file") << ").\n";
      return;
    }
    const auto canonical = fs::canonical(path, error);
    if (error) {
      std::cerr << "Cannot resolve file: " << path.string() << " (" << error.message() << ").\n";
      return;
    }
    if (seen.insert(canonical).second) files.push_back(path.string());
  };
  for (const auto& input : inputs) {
    if (!input.directory) {
      addFile(input.path);
      continue;
    }
    std::error_code error;
    if (!fs::is_directory(input.path, error)) {
      std::cerr << "Cannot scan directory: " << input.path << " ("
                << (error ? error.message() : "not a directory") << ").\n";
      continue;
    }
    std::vector<fs::path> candidates;
    fs::directory_iterator entry(input.path, error), end;
    while (!error && entry != end) {
      std::error_code entryError;
      const auto entryStatus = entry->symlink_status(entryError);
      if (entryError) {
        std::cerr << "Cannot inspect entry: " << entry->path().string()
                  << " (" << entryError.message() << ").\n";
      } else if (fs::is_regular_file(entryStatus)) {
        std::string extension = entry->path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (extension == ".txt") candidates.push_back(entry->path());
      }
      entry.increment(error);
    }
    if (error) {
      std::cerr << "Cannot scan directory: " << input.path << " (" << error.message() << ").\n";
    }
    // Filesystem enumeration order must not choose the retained duplicate spelling.
    std::sort(candidates.begin(), candidates.end());
    if (candidates.empty() && !error) {
      std::cerr << "No .txt files found in directory: " << input.path << ".\n";
    }
    for (const auto& path : candidates) addFile(path);
  }
  return files;
}

int main(int argc, char* argv[]) {
  Options options;
  if (!parseOptions(argc, argv, options)) {
    std::cerr << "Use --help for usage.\n";
    return 2;
  }
  if (options.help) {
    displayUsage();
    return 0;
  }
  if (options.inputs.empty()) {
    options.inputs = {{"data/sample/testfile1.txt", false},
                      {"data/sample/testfile2.txt", false},
                      {"data/sample/testfile3.txt", false},
                      {"data/sample/extreme secret.txt", false}};
  }
  const auto files = collectFiles(options.inputs);
  std::cout << "=== Mini Search Engine CLI ===\n";

  Indexer indexer;
  PrefixSearcher prefixSearcher;
  indexer.buildIndex(files);

  if (indexer.getTotalDoc() == 0) {
    std::cerr << "No documents could be loaded. Pass readable text file paths.\n";
    return 1;
  }

  // Build PrefixSearcher (Trie) from the Indexer's terms
  for (const auto& [word, postings] : indexer.getIndex()) {
    for (const auto& [doc, data] : postings) {
      // 'word' is already normalized
      prefixSearcher.insert(word, {doc, data.first});
    }
  }
  std::cout << "Index built successfully!\n";

  if (options.singleQuery) return runQuery(options.query, indexer, prefixSearcher);

  while (true) {
    std::cout << "\nEnter search query (:help for help, x to quit): ";
    std::string query;
    if (!std::getline(std::cin, query)) break;
    query = trimQuery(query);
    if (query.empty()) continue;
    if (query == "x") break;
    if (query == ":help") {
      displayQueryHelp();
      continue;
    }
    runQuery(query, indexer, prefixSearcher);
  }

  std::cout << "Goodbye!\n";
  return 0;
}
