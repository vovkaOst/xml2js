#include "Lexer.h"
#include <functional> 
#include <cctype>
#include <locale>
Lexer::Lexer(XMLsourse* input)
{
	sourse = input;
}


Lexer::~Lexer()
{

}

// trim from start
static inline std::string &ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
	s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
	s.erase(std::remove(s.begin(), s.end(), '\t'), s.end());
	return ltrim(rtrim(s));
}

Token Lexer::seeToken()
{
	if (fifoTokens.empty()) fifoTokens.push_back(nextToken());
	return fifoTokens.front();
}

Token Lexer::getToken()
{
	if (fifoTokens.empty()) return nextToken();
	Token c = fifoTokens.front();
	fifoTokens.pop_front();
	return c;
}

Token Lexer::getText()
{
	char c;
	string value = "";

	while ((c = sourse->nextChar()) != '<'){
		if (c == EOF) return Token(ENDofFILE);
		c = sourse->getChar();
		if (c == '&') c = processPredef();
		if (c == '"') value += '\\';
		value += c;
	}
	return Token(simpleTEXT, trim(value));
}

Token Lexer::nextToken()
{
	char c;
	string value = "";
	c = sourse->getChar();
	while (iswspace(c) && c != EOF) c = sourse->getChar();
	if (c == EOF) return Token(ENDofFILE);
	
	
	switch (c)
	{
		case '<':
			switch (sourse->nextChar())
			{
				case '!':
				{
					sourse->getChar();
					switch (sourse->nextChar())
					{
					case '-':
					{ //comment
						sourse->getChar();
						eh.errif("Comment: '-' character is expected after '<!-'", sourse->getChar() != '-', 2);
						while (c != EOF && !(c == '-' && sourse->nextChar() == '-')) value += (c = sourse->getChar());
						eh.errif("Unexpected end of file!", c == EOF,4);
						eh.errif("Comment: single '-' character is not allowed", sourse->getChar() != '-', 2);
						eh.errif("Comment: '>' is expected after '--' ", sourse->getChar() != '>', 2);
						value.pop_back();
						return Token(comment, value);
					}break;
					case '[':
					{ //CDATA
						sourse->getChar();
						bool t = true;
						t = t && (sourse->getChar() == 'C');
						t = t && (sourse->getChar() == 'D');
						t = t && (sourse->getChar() == 'A');
						t = t && (sourse->getChar() == 'T');
						t = t && (sourse->getChar() == 'A');
						t = t && (sourse->getChar() == '[');
						eh.errif("Start of CDATA tag is not properly formatted", !t, 3);
						while (c != EOF && !(c == ']'&&sourse->nextChar() == ']'&&sourse->nextChar(1) == '>')) {
							c = sourse->getChar();
							if (c == '"') value+= '\\';
							value += c;
						}
						eh.errif("Unexpected end of file!", c == EOF, 4);
						sourse->getChar(); sourse->getChar();
						value.pop_back();
						return Token(CDATA, trim(value));
					}break;
					default: //otherwise it's DOCTYPE tag untill >
						int o = 1;
						while (c != EOF)
						{
							c = sourse->getChar();
							if (c == '<') o++;
							if (c == '>' && --o == 0) break;
							else value += c;
						}
						eh.errif("Unexpected end of file!", c == EOF, 4);
						sourse->getChar();
						return Token(DOCTYPE, value);
						break;
					}
				} break;  //end <! case
				case '?':
				{//process instruction
					sourse->getChar();
					while (c != '?'&& c != EOF) value += (c = sourse->getChar());
					eh.errif("Unexpected end of file!", c == EOF, 4);
					eh.errif("Process instruction: '>' symbol is espected after '?'", sourse->getChar() != '>',5);
					value.pop_back();
					return Token(proInsTag, value);
				}break; //end <? case
				case '/': sourse->getChar(); return Token(startCloseTag, "</");
				break;
				default: return Token(startTag, "<"); //nextChar is some other symbol
				break;
			}
		break;
		case '>': return Token(closeTag, ">");
		break;
		case '/': 
				eh.errif("Close empty tag: '>' is expected after '/'", sourse->getChar() != '>', 7);
				return Token(closeEmptyTag, "/>");
		break;
		case 34: //quote -> arg value
			while (c != EOF && sourse->nextChar() != 34)
			{
				c = sourse->getChar();
				if (c == '&') c = processPredef();
				if (c == '"') value += '\\'; //????
				value += c;
			}
			eh.errif("Unexpected end of file!", c == EOF, 4);
			sourse->getChar(); //zjadamy pozostaly quote
			return Token(atributeValue, trim(value));
		break;
		case 39: //apostroph -the same
			while (c != EOF && sourse->nextChar() != 39)
			{
				c = sourse->getChar();
				if (c == '&') c = processPredef();
				if (c == '"') value += '\\'; //to omit the problems with quotes in JSon
				value += c;
			}
				eh.errif("Unexpected end of file!", c == EOF, 4);
				sourse->getChar(); //zjadamy pozostaly quote
				return Token(atributeValue, trim(value));
		break;
		case '=': return Token(equalTag, "=");
		break;
		default: //it's some name then - element name or attribute name
				value += c;
				while (c != EOF && sourse->nextChar() != '='&&sourse->nextChar()!='>'
					&&sourse->nextChar() != '<'&&  sourse->nextChar() != '/'&& !iswspace(sourse->nextChar()))
					value += (c = sourse->getChar());
				eh.errif("Unexpected end of file!", c == EOF, 4);
				return Token(nameTag, trim(value));	
		break;
	}
}

//possible predefs:
// &lt; is "<"
// &gt; is ">"
// &amp; is "&"
// &apos; is '
// &quot; is "
// allowed only in atribute values and texts
char Lexer::processPredef()
{
	char c = ' ';
	switch (sourse->getChar())
	{
	case 'g':
		eh.errif("Predefined expression warning: 't' is expected after '&g' and '&l'\n", sourse->getChar() != 't', 0);
		c = '>';
		break;
	case 'l':
		eh.errif("Predefined expression warning: 't' is expected after '&g' and '&l'\n", sourse->getChar() != 't', 0);
		c = '<';
		break;
	case 'q':
		eh.errif("Predefined expression warning: 'u' is expected after '&q'\n", sourse->getChar() != 'u', 0);
		eh.errif("Predefined expression warning: 'o' is expected after '&qu'\n", sourse->getChar() != 'o', 0);
		eh.errif("Predefined expression warning: 't' is expected after '&quo'\n", sourse->getChar() != 't', 0);
		c = '"';
		break;
	case 'a':
		switch (sourse->getChar())
		{
		case 'm':
			eh.errif("Predefined expression warning: 'p' is expected after '&am'\n", sourse->getChar() != 'p', 0);
			c = '&';
			break;
		case 'p':
			eh.errif("Predefined expression warning: 'o' is expected after '&ap'\n", sourse->getChar() != 'o', 0);
			eh.errif("Predefined expression warning: 's' is expected after '&apo'\n", sourse->getChar() != 's', 0);
			c = '\'';
			break;
		default: eh.errif("Predefined expression warning: unexpected symbol after '&a'", true, 0);
			return c;
		}break;

	default: //eh.errif("Predefined expression warning: unknown entity\n", true, 0);
		return c;
	}
	eh.errif("Predefined expression error: must end with ';'\n", sourse->getChar() != ';', 0);
	return c;
}
