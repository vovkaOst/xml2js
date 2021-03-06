#pragma once
#include "Lexer.h"
#include "XMLnode.h"
#include <forward_list>

class Parser
{
	Lexer* lexer;
	forward_list<Token> controlStack;
	XMLnode* treeRoot;
	ErrorHandler eh;
	 
	XMLnode* parseElement();
	list<pair<string, string>> parseAttributes();
public:
	Parser(Lexer*);
	~Parser();

	void parse();
	XMLnode& getTreeRoot();
};

