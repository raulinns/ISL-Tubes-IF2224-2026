CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
TARGET   := lexer
SRCDIR   := src
OBJDIR   := build

SRCS := $(SRCDIR)/main.cpp $(SRCDIR)/lexer.cpp
OBJS := $(OBJDIR)/main.o $(OBJDIR)/lexer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJDIR)/main.o: $(SRCDIR)/main.cpp $(SRCDIR)/lexer.h $(SRCDIR)/token.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/lexer.o: $(SRCDIR)/lexer.cpp $(SRCDIR)/lexer.h $(SRCDIR)/token.h | $(OBJDIR)
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