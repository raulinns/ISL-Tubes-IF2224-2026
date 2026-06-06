CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -MMD -MP
TARGET   := arion
TEST_TARGET := build/runtime_tests
DIFF     ?= git diff --no-index --
SRCDIR   := src
M4DIR    := $(SRCDIR)/milestone4
OBJDIR   := build

SRCS := $(SRCDIR)/main.cpp $(SRCDIR)/lexer.cpp $(SRCDIR)/parser.cpp $(SRCDIR)/parse_tree.cpp $(SRCDIR)/ast.cpp $(SRCDIR)/ast_builder.cpp $(SRCDIR)/symbol_table.cpp $(SRCDIR)/semantic_analyzer.cpp $(SRCDIR)/frontend_artifact.cpp $(M4DIR)/intermediate_code.cpp $(M4DIR)/codegen_context.cpp $(M4DIR)/runtime_value.cpp $(M4DIR)/runtime_stack.cpp $(M4DIR)/code_generator_core.cpp $(M4DIR)/code_generator_expr.cpp $(M4DIR)/code_generator_call_io.cpp $(M4DIR)/interpreter_core.cpp $(M4DIR)/interpreter_opr.cpp
OBJS := $(OBJDIR)/main.o $(OBJDIR)/lexer.o $(OBJDIR)/parser.o $(OBJDIR)/parse_tree.o $(OBJDIR)/ast.o $(OBJDIR)/ast_builder.o $(OBJDIR)/symbol_table.o $(OBJDIR)/semantic_analyzer.o $(OBJDIR)/frontend_artifact.o $(OBJDIR)/intermediate_code.o $(OBJDIR)/codegen_context.o $(OBJDIR)/runtime_value.o $(OBJDIR)/runtime_stack.o $(OBJDIR)/code_generator_core.o $(OBJDIR)/code_generator_expr.o $(OBJDIR)/code_generator_call_io.o $(OBJDIR)/interpreter_core.o $(OBJDIR)/interpreter_opr.o

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

$(OBJDIR)/frontend_artifact.o: $(SRCDIR)/frontend_artifact.cpp $(SRCDIR)/frontend_artifact.h $(SRCDIR)/ast.h $(SRCDIR)/symbol_table.h $(SRCDIR)/semantic_analyzer.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/intermediate_code.o: $(M4DIR)/intermediate_code.cpp $(M4DIR)/intermediate_code.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/codegen_context.o: $(M4DIR)/codegen_context.cpp $(M4DIR)/codegen_context.h $(M4DIR)/intermediate_code.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/runtime_value.o: $(M4DIR)/runtime_value.cpp $(M4DIR)/runtime_value.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/runtime_stack.o: $(M4DIR)/runtime_stack.cpp $(M4DIR)/runtime_stack.h $(M4DIR)/runtime_value.h $(M4DIR)/arion_runtime_error.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/code_generator_core.o: $(M4DIR)/code_generator_core.cpp $(M4DIR)/code_generator.h $(M4DIR)/codegen_context.h $(M4DIR)/intermediate_code.h $(M4DIR)/runtime_value.h $(SRCDIR)/ast.h $(SRCDIR)/symbol_table.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/code_generator_expr.o: $(M4DIR)/code_generator_expr.cpp $(M4DIR)/code_generator.h $(M4DIR)/codegen_context.h $(M4DIR)/intermediate_code.h $(M4DIR)/runtime_value.h $(SRCDIR)/ast.h $(SRCDIR)/symbol_table.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/code_generator_call_io.o: $(M4DIR)/code_generator_call_io.cpp $(M4DIR)/code_generator.h $(M4DIR)/codegen_context.h $(M4DIR)/intermediate_code.h $(SRCDIR)/ast.h $(SRCDIR)/symbol_table.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/interpreter_core.o: $(M4DIR)/interpreter_core.cpp $(M4DIR)/interpreter.h $(M4DIR)/intermediate_code.h $(M4DIR)/runtime_stack.h $(M4DIR)/runtime_value.h $(M4DIR)/arion_runtime_error.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/interpreter_opr.o: $(M4DIR)/interpreter_opr.cpp $(M4DIR)/interpreter.h $(M4DIR)/intermediate_code.h $(M4DIR)/runtime_stack.h $(M4DIR)/runtime_value.h $(M4DIR)/arion_runtime_error.h | $(OBJDIR)
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

$(TEST_TARGET): test/milestone-4/runtime_tests.cpp $(filter-out $(OBJDIR)/main.o,$(OBJS))
	$(CXX) $(CXXFLAGS) -o $@ $^

test: all $(TEST_TARGET)
	@set -e; \
	$(TEST_TARGET); \
	./$(TARGET) --source test/milestone-4/test-1-memory-assignment.txt build/test-report.txt; \
	awk '/^----- Intermediate Code -----$$/{seen=1; getline; next} /^----- Program Output -----$$/{seen=0} seen{print}' build/test-report.txt | sed '/^$$/d' > build/test-ic.txt; \
	$(DIFF) test/milestone-4/expected-ic-1-memory-assignment.txt build/test-ic.txt; \
	for n in 2 3 4; do \
	  input=$$(ls test/milestone-4/test-$$n-*.txt); \
	  expected=$$(ls test/milestone-4/expected-output-$$n-*.txt); \
	  ./$(TARGET) --source "$$input" build/test-report.txt; \
	  awk '/^----- Program Output -----$$/{seen=1; getline; next} seen{print}' build/test-report.txt | sed '/^$$/d' > build/test-output.txt; \
	  $(DIFF) "$$expected" build/test-output.txt; \
	done; \
	printf '42 3.5 A hello true\n' | ./$(TARGET) --source test/milestone-4/test-5-io.txt build/test-report.txt; \
	awk '/^----- Program Output -----$$/{seen=1; getline; next} seen{print}' build/test-report.txt | sed '/^$$/d' > build/test-output.txt; \
	$(DIFF) test/milestone-4/expected-output-5-io.txt build/test-output.txt; \
	! printf 'not-an-integer\n' | ./$(TARGET) --source test/milestone-4/test-5-io.txt >/dev/null 2>build/input-error.txt; \
	grep -q 'ip .*invalid type' build/input-error.txt; \
	! ./$(TARGET) --source test/milestone-4/test-6-definite-assignment-error.txt build/semantic-error.txt; \
	! grep -q '^----- Intermediate Code -----$$' build/semantic-error.txt; \
	! ./$(TARGET) --source test/milestone-4/test-7-runtime-bounds-error.txt >/dev/null 2>build/runtime-error.txt; \
	grep -q 'ip .*outside' build/runtime-error.txt; \
	./$(TARGET) --source test/milestone-4/test-8-artifact-roundtrip.txt build/artifact.txt; \
	./$(TARGET) build/artifact.txt build/artifact-roundtrip.txt; \
	test "$$(grep -c '^----- Intermediate Code -----$$' build/artifact-roundtrip.txt)" -eq 1; \
	test "$$(grep -c '^----- Program Output -----$$' build/artifact-roundtrip.txt)" -eq 1; \
	awk '/^----- Program Output -----$$/{seen=1; getline; next} seen{print}' build/artifact-roundtrip.txt | sed '/^$$/d' > build/test-output.txt; \
	$(DIFF) test/milestone-4/expected-output-8-artifact-roundtrip.txt build/test-output.txt; \
	for n in 1 2 3 4 5; do \
	  ./$(TARGET) --source test/milestone-3/test-$$n.txt >/dev/null 2>&1 && rc=0 || rc=$$?; \
	  case $$n in 1) expected_rc=0;; *) expected_rc=1;; esac; \
	  test $$rc -eq $$expected_rc; \
	done; \
	for spec in '1:1' '2:0' '3:1' '4:1' '5:1'; do \
	  n=$${spec%%:*}; expected_rc=$${spec##*:}; \
	  ./$(TARGET) --source test/milestone-1/test-$$n.txt >/dev/null 2>&1 && rc=0 || rc=$$?; \
	  test $$rc -eq $$expected_rc; \
	done; \
	for spec in '1:0' '2:1' '3:0' '4:0' '5:1'; do \
	  n=$${spec%%:*}; expected_rc=$${spec##*:}; \
	  ./$(TARGET) --source test/milestone-2/test-$$n.txt >/dev/null 2>&1 && rc=0 || rc=$$?; \
	  test $$rc -eq $$expected_rc; \
	done; \
	echo "all tests passed"

test-sanitize:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="-std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" test

.PHONY: all run clean test test-sanitize

-include $(OBJS:.o=.d)
