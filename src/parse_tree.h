#ifndef PARSE_TREE_H
#define PARSE_TREE_H

#include <string>
#include <utility>
#include <vector>

struct ParseNode {
  std::string label;
  std::vector<ParseNode> children;

  explicit ParseNode(std::string text) : label(std::move(text)) {}

  ParseNode &addChild(ParseNode child) {
    children.push_back(std::move(child));
    return children.back();
  }
};

std::string renderParseTree(const ParseNode &root);

#endif // PARSE_TREE_H
