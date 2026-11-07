#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#ifndef _WIN32
#include <sys/wait.h>
#endif
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
    assert(response.suggestions.empty());
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
  // A caller can show prefix expansions without interpreting the query itself.
  for (int limit : {10, 1, 0, -1}) {
    const auto response = executeQuery(" \tFO* \n", index, trie, limit);
    assert(response.ok());
    assert((response.suggestions == std::vector<std::string>{"fox", "foxes"}));
    assert(response.results.size() == (limit > 1 ? 2u : limit == 1 ? 1u : 0u));
    assert(executeQuery("fo**", index, trie, limit).suggestions.empty());
  }
  assert((executeQuery("12*", index, trie, 10).suggestions ==
          std::vector<std::string>{"123"}));
  assert((executeQuery("test-*", index, trie, 10).suggestions ==
          std::vector<std::string>{"test-case"}));
  for (const std::string query : {"fox", "fox OR dog", "fox - dog",
                                 "\"quick brown fox\"", "unfindable*"}) {
    assert(executeQuery(query, index, trie, 10).suggestions.empty());
  }
  // Existing three-field aggregate construction remains valid.
  const QueryResponse legacyResponse{{}, QueryError::None, ""};
  assert(legacyResponse.ok() && legacyResponse.suggestions.empty());
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
  assert(executeQuery("fo*", noDocuments, noTerms, 10).suggestions.empty());
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

  // Identical identifiers and text must yield the same complete response,
  // whether the content came from disk or from a caller-owned string.
  const std::vector<TextDocument> documents = {
      {"tests/tmp/memory-a.txt", "Quick, brown FOX fox foxes don't test-case 123\r\n a a a"},
      {"tests/tmp/memory-b.txt", "quick\tbrown dog brown fox !!!"},
      {"tests/tmp/memory-empty.txt", ""},
      {"tests/tmp/memory-punctuation.txt", "!!! ... \t\r\n"}};
  std::vector<std::string> documentPaths;
  for (const auto& document : documents) {
    std::ofstream(document.id, std::ios::binary) << document.text;
    documentPaths.push_back(document.id);
  }
  Indexer fileIndex, textIndex;
  fileIndex.buildIndex(documentPaths);
  textIndex.buildIndexFromText(documents);
  const auto makeTrie = [](Indexer& source) {
    PrefixSearcher result;
    for (const auto& [word, postings] : source.getIndex())
      for (const auto& [doc, data] : postings) result.insert(word, {doc, data.first});
    return result;
  };
  auto fileTrie = makeTrie(fileIndex);
  auto textTrie = makeTrie(textIndex);
  assert(fileIndex.getIndex() == textIndex.getIndex());
  assert(fileIndex.getDocLen() == textIndex.getDocLen());
  assert(fileIndex.getDocContent() == textIndex.getDocContent());
  assert(textIndex.getTotalDoc() == 4);
  for (const std::string query : {"fox", "FOX", "fo*", "12*", "don't", "test-*",
                                 "quick brown", "fox OR dog", "fox - dog", "fox FOX",
                                 "\"quick brown fox\"", "\"a a\"", "unfindable*",
                                 "unfindableterm", "fox AND", "*", ""}) {
    for (int limit : {10, 1, 0, -1}) {
      const auto disk = executeQuery(query, fileIndex, fileTrie, limit);
      const auto memory = executeQuery(query, textIndex, textTrie, limit);
      assert(disk.error == memory.error && disk.message == memory.message);
      assert(disk.suggestions == memory.suggestions);
      assert(disk.results.size() == memory.results.size());
      for (std::size_t i = 0; i < disk.results.size(); ++i) {
        const auto& left = disk.results[i];
        const auto& right = memory.results[i];
        assert(left.document == right.document);
        assert(std::abs(left.score - right.score) < 1e-12);
        assert(left.isPhrase == right.isPhrase);
        assert(left.phraseOccurrences == right.phraseOccurrences);
        assert(left.matches.size() == right.matches.size());
        for (std::size_t j = 0; j < left.matches.size(); ++j) {
          assert(left.matches[j].term == right.matches[j].term);
          assert(left.matches[j].frequency == right.matches[j].frequency);
          assert(left.matches[j].positions == right.matches[j].positions);
        }
        assert(left.spans.size() == right.spans.size());
        for (std::size_t j = 0; j < left.spans.size(); ++j) {
          assert(left.spans[j].start == right.spans[j].start);
          assert(left.spans[j].end == right.spans[j].end);
        }
        assert(fileIndex.getSnippetFromSpans(left.document, left.spans) ==
               textIndex.getSnippetFromSpans(right.document, right.spans));
      }
    }
  }

  Indexer virtualIndex;
  virtualIndex.buildIndexFromText({{"virtual:one", "obsolete fox fox"},
                                  {"virtual:two", "fox"},
                                  {"virtual:one", "replacement dog dog"}});
  assert(virtualIndex.getTotalDoc() == 2);
  assert(virtualIndex.searchWord("obsolete").empty());
  assert(virtualIndex.searchWord("fox").count("virtual:one") == 0);
  assert((virtualIndex.searchWord("dog").at("virtual:one").second == std::vector<int>{1, 2}));
  assert(virtualIndex.getDocLen().at("virtual:one") == 3);
  auto rebuiltTrie = makeTrie(virtualIndex);
  assert(executeQuery("obs*", virtualIndex, rebuiltTrie, 10).suggestions.empty());
  assert(executeQuery("do*", virtualIndex, rebuiltTrie, 10).results.size() == 1);
  virtualIndex.addDocumentText("virtual:one", "");
  assert(virtualIndex.getTotalDoc() == 2);
  assert(virtualIndex.getDocLen().at("virtual:one") == 0);
  assert(virtualIndex.getDocContent().at("virtual:one").empty());
  assert(virtualIndex.searchWord("dog").empty());
  virtualIndex.addDocumentText("Virtual:two", "case distinct");
  assert(virtualIndex.getTotalDoc() == 3);
  // Invalid IDs must not partially clear or replace a valid corpus.
  const auto beforeInvalid = virtualIndex.getIndex();
  const auto lengthsBeforeInvalid = virtualIndex.getDocLen();
  const auto contentBeforeInvalid = virtualIndex.getDocContent();
  bool rejected = false;
  try { virtualIndex.addDocumentText("", "invalid"); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  rejected = false;
  try { virtualIndex.buildIndexFromText({{"valid", "new text"}, {"", "invalid"}}); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  assert(virtualIndex.getIndex() == beforeInvalid);
  assert(virtualIndex.getDocLen() == lengthsBeforeInvalid);
  assert(virtualIndex.getDocContent() == contentBeforeInvalid);
  virtualIndex.buildIndexFromText({});
  assert(virtualIndex.getTotalDoc() == 0 && virtualIndex.getIndex().empty());
  assert(virtualIndex.getDocContent().empty());
  // Existing file loading can still replace a text-loaded document and vice versa.
  virtualIndex.addDocumentText(a, "temporary");
  virtualIndex.addDocument(a);
  assert(virtualIndex.searchWord("temporary").empty());
  assert(virtualIndex.searchWord("fox").at(a).first == 2);
  virtualIndex.addDocumentText(a, "memory replacement");
  assert(virtualIndex.searchWord("fox").empty());
  assert(virtualIndex.getTotalDoc() == 1);
  virtualIndex.buildIndex({c});
  assert(virtualIndex.searchWord("memory").empty());
  assert(virtualIndex.getTotalDoc() == 1);
  assert(virtualIndex.searchWord("dog").at(c).first == 2);

  index.buildIndex({c});
  assert(index.getTotalDoc() == 1);
  assert(index.searchWord("fox").empty());
  // Exercise prompt recovery and diagnostic routing through the real CLI on both platforms.
  std::ofstream("tests/tmp/cli-doc.txt") << "fox fox foxes quick brown fox AND help";
  const auto runCommand = [](const std::string& arguments, const std::string& input,
                             int expectedStatus = 0) {
    std::ofstream("tests/tmp/cli-input.txt") << input;
#ifdef _WIN32
    const std::string executable = ".\\search.exe";
#else
    const std::string executable = "./search";
#endif
    const std::string command = executable +
        " " + arguments + " < tests/tmp/cli-input.txt > tests/tmp/cli-output.txt 2>&1";
    const int status = std::system(command.c_str());
#ifdef _WIN32
    assert(status == expectedStatus);
#else
    assert(status != -1 && WIFEXITED(status));
    assert(WEXITSTATUS(status) == expectedStatus);
#endif
    std::ifstream output("tests/tmp/cli-output.txt");
    assert(output);
    return std::string(std::istreambuf_iterator<char>(output), std::istreambuf_iterator<char>());
  };
  const auto runCli = [&](const std::string& input) {
    return runCommand("tests/tmp/cli-doc.txt", input);
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

  // Options are exercised through the executable so argument parsing, discovery,
  // status codes and the shared result formatter are covered together.
  const auto helpOutput = runCommand("--help --directory tests/tmp/missing", "fox\n");
  assert(helpOutput.find("Usage: search") != std::string::npos);
  assert(helpOutput.find("Query help:") != std::string::npos);
  assert(helpOutput.find("Index built") == std::string::npos);
  assert(helpOutput.find("Cannot scan") == std::string::npos);
  for (const std::string args : {"--file", "--directory", "--query", "--unknown",
                               "--query fox --query dog"}) {
    const auto output = runCommand(args, "", 2);
    assert(output.find("Use --help") != std::string::npos);
    assert(output.find("Index built") == std::string::npos);
  }
  const std::string explicitFile = "--file tests/tmp/cli-doc.txt";
  const auto singleOutput = runCommand(explicitFile + " --query fox", "AND fox\n");
  assert(singleOutput.find("fox: 3") != std::string::npos);
  assert(singleOutput.find("Enter search query") == std::string::npos);
  assert(singleOutput.find("Goodbye!") == std::string::npos);
  assert(singleOutput.find("Invalid query:") == std::string::npos);
  assert(singleOutput == runCommand("tests/tmp/cli-doc.txt --query fox", ""));
  assert(runCommand(explicitFile + " --query unfindableterm", "").find("No results found.") !=
         std::string::npos);
  for (const std::string query : {"fox AND", "   ", ""}) {
    assert(runCommand(explicitFile + " --query \"" + query + "\"", "", 2)
               .find("Invalid query:") != std::string::npos);
  }
  assert(runCommand(explicitFile + " --query \" fo* \"", "")
             .find("Suggestions: fox foxes") != std::string::npos);
  assert(runCommand(explicitFile + " --query \"fox - dog\"", "")
             .find("fox: 3") != std::string::npos);
  assert(runCommand(explicitFile + " --query x", "").find("No results found.") !=
         std::string::npos);
  assert(runCommand(explicitFile + " --query :help", "").find("help: 1") !=
         std::string::npos);
  // Preserve the exact same result block as interactive mode.
  const auto interactiveFox = runCli("fox\nx\n");
  const std::string resultHeader = "Search results (ranked):";
  assert(interactiveFox.find(singleOutput.substr(singleOutput.find(resultHeader))) !=
         std::string::npos);
  assert(runCommand("--query fox", "").find("testfile1.txt") != std::string::npos);
  assert(runCommand("", "fox\nx\n").find("testfile1.txt") != std::string::npos);

  fs::create_directories("tests/tmp/folder with spaces/nested");
  fs::create_directories("tests/tmp/empty directory");
  std::ofstream("tests/tmp/folder with spaces/first file.txt") << "foldertoken";
  std::ofstream("tests/tmp/folder with spaces/second.TXT") << "foldertoken";
  std::ofstream("tests/tmp/folder with spaces/ignored.md") << "excludedtoken";
  std::ofstream("tests/tmp/folder with spaces/nested/hidden.txt") << "excludedtoken";
  std::ofstream("tests/tmp/folder with spaces/empty.txt");
  const std::string directory = "--directory \"tests/tmp/folder with spaces\"";
  const auto folderOutput = runCommand(directory + " --query foldertoken", "");
  assert(folderOutput.find("first file.txt") < folderOutput.find("second.TXT"));
  assert(folderOutput.find("second.TXT") != std::string::npos);
  assert(folderOutput.find("empty.txt") == std::string::npos);
  assert(runCommand(directory + " --query excludedtoken", "").find("No results found.") !=
         std::string::npos);
  assert(runCommand("--file \"tests/tmp/folder with spaces/ignored.md\" --query excludedtoken", "")
             .find("excludedtoken: 1") != std::string::npos);
  // Aliases must neither duplicate results nor change IDF by inflating document count.
  assert(singleOutput == runCommand(explicitFile +
      " tests/tmp/./cli-doc.txt --file tests/tmp/cli-doc.txt --query fox", ""));
  assert(folderOutput == runCommand(directory + " " + directory +
      " --file \"tests/tmp/folder with spaces/./first file.txt\" --query foldertoken", ""));
  const auto mixed = runCommand(explicitFile + " " + directory + " --query foldertoken", "");
  assert(mixed.find("first file.txt") != std::string::npos);
  assert(mixed.find("second.TXT") != std::string::npos);
  assert(runCommand("--file tests/tmp/empty.txt --query fox", "")
             .find("No results found.") != std::string::npos);
  for (const std::string args : {"--file tests/tmp/missing", "--file tests/tmp",
                               "--directory tests/tmp/missing",
                               "--directory tests/tmp/a.txt",
                               "--directory \"tests/tmp/empty directory\"", "--file \"\""}) {
    const auto output = runCommand(args + " --query fox", "", 1);
    assert(output.find("No documents could be loaded") != std::string::npos);
    assert(output.find("testfile1.txt") == std::string::npos);
  }
  const auto partial = runCommand("--file tests/tmp/missing " + explicitFile + " --query fox", "");
  assert(partial.find("Cannot use file") != std::string::npos);
  assert(partial.find("fox: 3") != std::string::npos);
  assert(runCommand("--query fox -- tests/tmp/cli-doc.txt", "") == singleOutput);
  // An option-looking basename remains searchable after the option terminator.
  const auto oldDirectory = fs::current_path();
  const auto cliPath = fs::absolute(
#ifdef _WIN32
      "search.exe"
#else
      "search"
#endif
  );
  fs::create_directories("tests/tmp/option-path/tests/tmp");
  std::ofstream("tests/tmp/option-path/--help") << "literalpath";
  fs::copy_file(cliPath, "tests/tmp/option-path/" + cliPath.filename().string(),
                fs::copy_options::overwrite_existing);
  fs::current_path("tests/tmp/option-path");
  const auto literalPath = runCommand("--query literalpath -- --help", "");
  assert(literalPath.find("literalpath: 1") != std::string::npos);
  fs::current_path(oldDirectory);

  // Windows may require developer mode or administrator privileges for symlinks.
  std::error_code linkError;
  fs::create_symlink(fs::absolute("tests/tmp/cli-doc.txt"),
                     "tests/tmp/folder with spaces/link.txt", linkError);
  if (!linkError) {
    assert(runCommand(directory + " --query fox", "").find("No results found.") !=
           std::string::npos);
    assert(singleOutput == runCommand(explicitFile +
        " --file \"tests/tmp/folder with spaces/link.txt\" --query fox", ""));
  } else {
    std::cout << "Symlink checks skipped: " << linkError.message() << "\n";
  }

  std::error_code directoryLinkError;
  fs::create_directory_symlink(fs::absolute("tests/tmp/folder with spaces"),
                               "tests/tmp/directory-link", directoryLinkError);
  if (!directoryLinkError) {
    assert(runCommand("--directory tests/tmp/directory-link --query foldertoken", "")
               .find("foldertoken: 1") != std::string::npos);
    assert(folderOutput == runCommand(directory +
        " --directory tests/tmp/directory-link --query foldertoken", ""));
  } else {
    std::cout << "Directory symlink checks skipped: " << directoryLinkError.message() << "\n";
  }

  fs::remove_all("tests/tmp");
  std::cout << "Search regression checks passed.\n";
}
