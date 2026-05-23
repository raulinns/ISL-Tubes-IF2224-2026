#include "parse_tree.h"

#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string &text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

ParseNode *nodeAtPath(ParseNode &root, const std::vector<std::size_t> &path) {
    ParseNode *node = &root;
    for (std::size_t index : path) {
        if (index >= node->children.size()) {
            throw std::runtime_error("Invalid parse tree indentation");
        }
        node = &node->children[index];
    }
    return node;
}

void renderBranch(const ParseNode &node, const std::string &prefix, bool isLast,
                  std::ostringstream &out) {
    out << prefix << (isLast ? "\\-- " : "|-- ") << node.label << "\n";

    const std::string nextPrefix = prefix + (isLast ? "    " : "|   ");
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        renderBranch(node.children[i], nextPrefix,
                     i + 1 == node.children.size(), out);
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

ParseNode parseRenderedParseTree(const std::string &text) {
    std::istringstream input(text);
    std::string line;

    while (std::getline(input, line)) {
        line = trim(line);
        if (!line.empty()) {
            break;
        }
    }

    if (line.empty()) {
        throw std::runtime_error("Empty parse tree input");
    }

    ParseNode root(line);
    std::vector<std::vector<std::size_t>> paths(1);

    while (std::getline(input, line)) {
        if (trim(line).empty()) {
            continue;
        }

        std::size_t marker = line.find("|-- ");
        if (marker == std::string::npos) {
            marker = line.find("\\-- ");
        }
        if (marker == std::string::npos || marker % 4 != 0) {
            throw std::runtime_error("Unsupported parse tree line: " + line);
        }

        const std::size_t depth = marker / 4 + 1;
        if (depth == 0 || depth > paths.size()) {
            throw std::runtime_error("Invalid parse tree indentation near: " + line);
        }

        const std::string label = trim(line.substr(marker + 4));
        ParseNode *parent = nodeAtPath(root, paths[depth - 1]);
        parent->addChild(ParseNode(label));

        if (paths.size() <= depth) {
            paths.resize(depth + 1);
        }
        paths[depth] = paths[depth - 1];
        paths[depth].push_back(parent->children.size() - 1);
        paths.resize(depth + 1);
    }

    return root;
}
