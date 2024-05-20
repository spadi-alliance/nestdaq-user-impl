// infixtorp.h //

#include <string>
#include <memory>

struct AstNode {
    std::string value;
    std::shared_ptr<AstNode> left, right;
  public:
    std::string to_rpn() {
        std::string rpn;
        if (left) {
            rpn += left->to_rpn();
        }
        if (right) {
            rpn += right->to_rpn();
        }
        rpn += value + " ";
        return rpn;
    }
};

std::shared_ptr<AstNode> infixtorp(const std::string& input);


// - - - - - - - - - - - - - - - - 8< - - - - - - - - - - - - - - - - //
// infixtorp.cxx //

#include <string>
#include <memory>
#include <deque>
#include <stdexcept>
#include <cctype>
//#include "infixtorp.h"

static std::deque<std::string> tokenize(const std::string& line)
{
    std::deque<std::string> tokens;
    
    std::string::const_iterator i = line.begin();
    while (i != line.end()) {
        while ((i != line.end()) && std::isspace(*i)) {
            i++;
        }
        if (i == line.end()) {
            break;
        }

        std::string token;
        if ((*i == '(') || (*i == ')') || (*i == '|') || (*i == '&')) {
            token = *(i++);
        }
        else if (std::isdigit(*i)) {
            while ((i != line.end()) && std::isdigit(*i)) {
                token += *(i++);
            }
        }
        else {
            throw std::runtime_error(std::string("invalid char: ") + char(*i));
        }
        tokens.push_back(token);
    }

    return tokens;
}

static std::shared_ptr<AstNode> parse_or(std::deque<std::string>&);
static std::shared_ptr<AstNode> parse_and(std::deque<std::string>&);
static std::shared_ptr<AstNode> parse_value(std::deque<std::string>&);

static std::shared_ptr<AstNode> parse_or(std::deque<std::string>& tokens)
{
    std::shared_ptr<AstNode> left = parse_and(tokens);
    while (left && ! tokens.empty() && tokens.front() == "|") {
        auto node = std::make_shared<AstNode>();
        node->left = left;
        node->value = tokens.front(); tokens.pop_front();
        node->right = parse_and(tokens);
        left = node;
    }

    return left;
}

static std::shared_ptr<AstNode> parse_and(std::deque<std::string>& tokens)
{
    std::shared_ptr<AstNode> left = parse_value(tokens);
    while (left && ! tokens.empty() && tokens.front() == "&") {
        auto node = std::make_shared<AstNode>();
        node->left = left;
        node->value = tokens.front(); tokens.pop_front();
        node->right = parse_value(tokens);
        left = node;
    }

    return left;
}

static std::shared_ptr<AstNode> parse_value(std::deque<std::string>& tokens)
{
    if (tokens.empty()) {
        return 0;
    }
    if (tokens.front() == "(") {
        tokens.pop_front();
        auto node = parse_or(tokens);
        if (tokens.empty() || tokens.front() != ")") {
            throw std::runtime_error("unmatched '('");
        }
        tokens.pop_front();
        return node;
    }
    else if (std::isdigit(tokens[0][0])) {
        auto node = std::make_shared<AstNode>();
        node->value = tokens.front(); tokens.pop_front();
        return node;
    }
    else {
        throw std::runtime_error("invalid char: " + tokens.front());
    }
}

std::shared_ptr<AstNode> infixtorp(const std::string& input)
{
    std::deque<std::string> tokens = tokenize(input);
    auto ast = parse_or(tokens);
    if (! tokens.empty()) {
        throw std::runtime_error("invalid char: " + tokens.front());
    }
    
    return ast;
}



// - - - - - - - - - - - - - - - - 8< - - - - - - - - - - - - - - - - //

#ifdef INFIXTORP_TEST_MAIN

#include <iostream>
//#include "pasrse.h"


int main(void)
{
    std::string input = "(0 | 1) & (2 | 3 | 4) & (5 & 6)";

    try {
        std::cout << infixtorp(input)->to_rpn() << std::endl;  // --> "1 2 | 3 4 5 | 6 & 7 & | & "
    }
    catch (std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
    }
    
    return 0;
}

#endif
