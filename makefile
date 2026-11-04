CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic
CPPFLAGS =

TARGET = search
OBJS = main.o Index.o
HEADERS = Index.h ExactSearcher.hpp PrefixSearcher.hpp query_processor.hpp

.PHONY: all clean test
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

tests/search-tests: tests/search_tests.cpp Index.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I. tests/search_tests.cpp Index.cpp $(LDFLAGS) $(LDLIBS) -o $@

test: $(TARGET) tests/search-tests
	./tests/search-tests
	printf 'fox\n' | ./search >/dev/null

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET) tests/search-tests

-include $(OBJS:.o=.d)