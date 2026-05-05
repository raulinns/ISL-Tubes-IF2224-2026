#include "parse_tree.h"

#include <sstream>

namespace {

void renderBranch(const ParseNode &node, const std::string &prefix,
                  bool isLast, std::ostringstream &out) {
  out << prefix << (isLast ? "\\-- " : "|-- ") << node.label << "\n";

  const std::string nextPrefix = prefix + (isLast ? "    " : "|   ");
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    renderBranch(node.children[i], nextPrefix, i + 1 == node.children.size(),
                 out);
  }
}

} // namespace

std::string renderParseTree(const ParseNode &root) {
  std::ostringstream out;
  out << root.label << "\n";

  for (std::size_t i = 0; i < root.children.size(); ++i) {
    renderBranch(root.children[i], "", i + 1 == root.children.size(), out);
  }

  return out.str();
}
