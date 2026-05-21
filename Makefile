CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
TARGET   := arion
SRCDIR   := src
OBJDIR   := build

SRCS := $(SRCDIR)/main.cpp $(SRCDIR)/lexer.cpp $(SRCDIR)/parser.cpp $(SRCDIR)/parse_tree.cpp $(SRCDIR)/ast.cpp $(SRCDIR)/ast_builder.cpp $(SRCDIR)/symbol_table.cpp $(SRCDIR)/semantic_analyzer.cpp
OBJS := $(OBJDIR)/main.o $(OBJDIR)/lexer.o $(OBJDIR)/parser.o $(OBJDIR)/parse_tree.o $(OBJDIR)/ast.o $(OBJDIR)/ast_builder.o $(OBJDIR)/symbol_table.o $(OBJDIR)/semantic_analyzer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJDIR)/main.o: $(SRCDIR)/main.cpp $(SRCDIR)/lexer.h $(SRCDIR)/token.h $(SRCDIR)/parser.h $(SRCDIR)/parse_tree.h $(SRCDIR)/ast.h $(SRCDIR)/ast_builder.h $(SRCDIR)/semantic_analyzer.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/lexer.o: $(SRCDIR)/lexer.cpp $(SRCDIR)/lexer.h $(SRCDIR)/token.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/parser.o: $(SRCDIR)/parser.cpp $(SRCDIR)/parser.h $(SRCDIR)/parse_tree.h $(SRCDIR)/token.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/parse_tree.o: $(SRCDIR)/parse_tree.cpp $(SRCDIR)/parse_tree.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/ast.o: $(SRCDIR)/ast.cpp $(SRCDIR)/ast.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/ast_builder.o: $(SRCDIR)/ast_builder.cpp $(SRCDIR)/ast_builder.h $(SRCDIR)/ast.h $(SRCDIR)/parse_tree.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/symbol_table.o: $(SRCDIR)/symbol_table.cpp $(SRCDIR)/symbol_table.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/semantic_analyzer.o: $(SRCDIR)/semantic_analyzer.cpp $(SRCDIR)/semantic_analyzer.h $(SRCDIR)/ast.h $(SRCDIR)/symbol_table.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

run: all
ifdef IN
	./$(TARGET) $(IN)
else
	@echo "Gunakan: make run IN=<file_input.txt>"
endif

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all run clean
