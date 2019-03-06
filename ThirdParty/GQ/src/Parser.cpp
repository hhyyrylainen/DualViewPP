/*
* This is a heavily modified fork of gumbo-query by Hoping White aka LazyTiger.
* The original software can be found at: https://github.com/lazytiger/gumbo-query
*
* gumbo-query is based on cascadia, written by Andy Balholm.
*
* Copyright (c) 2011 Andy Balholm. All rights reserved.
* Copyright (c) 2015 Hoping White aka LazyTiger (hoping@baimashi.com)
* Copyright (c) 2015 Jesse Nicholson
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include "Parser.hpp"
#include "AttributeSelector.hpp"
#include "BinarySelector.hpp"
#include "TextSelector.hpp"
#include "UnarySelector.hpp"

namespace gq
{

	const std::unordered_map<boost::string_ref, Parser::PseudoOp, StringRefHash> Parser::PseudoOps =
	{ 
		{ u8"not", PseudoOp::Not },
		{ u8"has", PseudoOp::Has },
		{ u8"haschild", PseudoOp::HasChild },
		{ u8"contains", PseudoOp::Contains },
		{ u8"containsown", PseudoOp::ContainsOwn },
		{ u8"matches", PseudoOp::Matches },
		{ u8"matchesown", PseudoOp::MatchesOwn },
		{ u8"nth-child", PseudoOp::NthChild },
		{ u8"nth-last-child", PseudoOp::NthLastChild },
		{ u8"nth-of-type", PseudoOp::NthOfType },
		{ u8"nth-last-of-type", PseudoOp::NthLastOfType },
		{ u8"first-child", PseudoOp::FirstChild },
		{ u8"last-child", PseudoOp::LastChild },
		{ u8"first-of-type", PseudoOp::FirstOfType },
		{ u8"last-of-type", PseudoOp::LastOfType },
		{ u8"only-child", PseudoOp::OnlyChild },
		{ u8"only-of-type", PseudoOp::OnlyOfType },
		{ u8"empty", PseudoOp::Empty }
	};

	Parser::Parser()
	{
    #ifdef __linux__
        // The locale has a '_' instead of '-' on linux
        m_localeEnUS = std::locale(u8"en_US");
    #else
        m_localeEnUS = std::locale(u8"en-US");
    #endif //__linux__
	}

	Parser::~Parser()
	{
	}

	SharedSelector Parser::CreateSelector(std::string selectorString, const bool retainOriginalString) const
	{
		boost::string_ref input = boost::string_ref(selectorString);

		try
		{
			SharedSelector result = ParseSelectorGroup(input);

			if (input.size() != 0) 
			{ 
				// There's no assert on debug here, because it's expected that user input may
				// contain invalid data. Passing a poorly formatted selector string isn't a matter
				// of a developer user using the library improperly, so no assert. The idea is that
				// any user should expect this method to throw and hence be ready to handle it, and
				// that a detailed message of the issue be returned in the error.
				throw std::runtime_error(u8"In Parser::CreateSelector(std::string, const bool) - Improperly formatted selector string."); 
			}

			if (retainOriginalString)
			{
				result->m_originalSelectorString = std::move(selectorString);
			}			

			return result;
		}
		catch (std::runtime_error& e)
		{
			std::string errorMessage(e.what());
			errorMessage.append(std::string(u8" -- [HERE>>>>>") + input.to_string() + std::string(u8"<<<<<]"));

			throw std::runtime_error(errorMessage.c_str());
		}		
	}

	SharedSelector Parser::ParseSelectorGroup(boost::string_ref& selectorStr) const
	{
		// Parse the first selector object from the input supplied. 
		SharedSelector ret = ParseSelector(selectorStr);

		// ParseSelector() will stop if it encounters a character in the selector string that
		// indicates that the supplied input is a selector group. That is, if "," is encountered
		// while ParseSelector() is consuming input, it will break and return the most recently
		// constructed Selector. This also applies if it finds a closing parenthesis, an
		// indication that the internals of a pseudo selector are finished being built.
		// 
		// So, after we get an initial return, we'll continue to recursively build selectors and
		// combine them until no more "," group indicators are found and/or the end of the input has
		// been reached.
		while (selectorStr.size() > 0 && selectorStr[0] == ',')
		{
			selectorStr = selectorStr.substr(1);

			SharedSelector second = ParseSelector(selectorStr);

			ret = std::make_shared<BinarySelector>(BinarySelector::SelectorOperator::Union, ret, second);
		}

		return ret;
	}

	SharedSelector Parser::ParseSelector(boost::string_ref& selectorStr) const
	{
		TrimLeadingWhitespace(selectorStr);

		SharedSelector ret = ParseSimpleSelectorSequence(selectorStr);

		char combinator = 0;

		bool notDone = true;

		while (notDone && selectorStr.size() > 0)
		{
			if (TrimLeadingWhitespace(selectorStr))
			{
				combinator = ' ';

				// In the event of " > ", we do this.
				if (selectorStr.size() > 0 && combinator == ' ' && IsCombinator(selectorStr[0]))
				{
					combinator = selectorStr[0];
					
					if (!IsCombinator(combinator))
					{
						throw std::runtime_error(u8"In Parser::ParseSelector(boost::string_ref&) - Invalid combinator supplied.");
					}

					selectorStr = selectorStr.substr(1);
					if (!TrimLeadingWhitespace(selectorStr))
					{
						throw std::runtime_error(u8"In Parser::ParseSelector(boost::string_ref&) - Invalid combinator supplied. Combinator had leading whitespace without trailing whitespace.");
					}
				}
			}

			if (selectorStr.size() == 0)
			{
				// Could have been some trailing whitespace.
				return ret;
			}

			switch (selectorStr[0])
			{
				case ',':
				case ')':
				{
					// This smells to me (the closing paren). Perhaps it's for parsing stuff inside
					// of pseudo selector parenthesis? Either way this should really be handled
					// already by the pseudo parsing code, not showing up here.
					return ret;
				}
				break;
			}

			if (combinator == 0)
			{
				// No combinator, we're done here.
				return ret;
			}

			SharedSelector selector = ParseSimpleSelectorSequence(selectorStr);

			switch (combinator)
			{
				case ' ':
				{
					ret = std::make_shared<BinarySelector>(BinarySelector::SelectorOperator::Descendant, ret, selector);
				}
				break;

				case '>':
				{
					ret = std::make_shared<BinarySelector>(BinarySelector::SelectorOperator::Child, ret, selector);
				}
				break;

				case '+':
				{
					ret = std::make_shared<BinarySelector>(BinarySelector::SelectorOperator::Adjacent, ret, selector);
				}
				break;

				case '~':
				{
					ret = std::make_shared<BinarySelector>(BinarySelector::SelectorOperator::Sibling, ret, selector);
				}
				break;

				default:
					// This should never happen, since we've correctly only accepted valid
					// combinators. However, if somehow this happens, we should explode the
					// universe.
					throw std::runtime_error(u8"In Parser::ParseSelector(boost::string_ref&) - Invalid combinator supplied.");
			}

			combinator = 0;
		}

		return ret;
	}

	SharedSelector Parser::ParseSimpleSelectorSequence(boost::string_ref& selectorStr) const
	{
		if (selectorStr.size() == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseSimpleSelectorSequence(boost::string_ref&) - Expected selector string, received empty string.");
		}

		SharedSelector ret = nullptr;

		char zero = selectorStr[0];

		switch (zero)
		{
			case '*':
			{
				// Dummy selector. Matches anything.
				selectorStr = selectorStr.substr(1);
				return std::make_shared<Selector>(Selector::SelectorOperator::Dummy);
			}
			break;

			case '#':
			case '.':
			case '[':
			case ':':
			{
				// If it's an ID, class, attribute, or pseudo class selector, just move on, this
				// will be handled a little later.
				break;
			}
			break;

			default:
			{
				// Assume it's a type selector. If it is valid, will return a valid object. If not,
				// it will be nullptr. We'll handle either situation a little later.
				ret = ParseTypeSelector(selectorStr);
			}
			break;
		}

		bool notDone = true;

		while (notDone && selectorStr.size() > 0)
		{
			SharedSelector selector = nullptr;

			zero = selectorStr[0];

			if (IsCombinator(zero))
			{
				// If the next part of the string is a combinator, then we'll break and return,
				// allowing ParseSelector() to correctly handle combined selectors.
				break;
			}

			switch (zero)
			{
				case '#':
				{
					selector = ParseIDSelector(selectorStr);
				}
				break;

				case '.':
				{
					selector = ParseClassSelector(selectorStr);
				}
				break;

				case '[':
				{
					selector = ParseAttributeSelector(selectorStr);
				}
				break;

				case ':':
				{
					selector = ParsePseudoclassSelector(selectorStr);

					if (ret == nullptr)
					{
						// This means that we've encountered a pseudo class selector that has no
						// previous qualifiers such as a specific tag or other attribute to bind to,
						// such as div:last-child. As such, in order to properly handle this when
						// using the map based approach to candidate searching, a dummy selector
						// needs to be combined with this pseudo class selector. A dummy selector
						// will generate appropriate search attributes in the event that this pseudo
						// class selector makes up the primary function of the selector. If we
						// didn't do this, there would be no reliable attributes by which to
						// discover candidates.
						// 
						// Consider the pseudo class selector ":not(p)". Without this little trick,
						// the only attributes we'd get from such a selector are {
						// NORMALIZED_TAG_NAME_KEY, p }, which would only find candidates which
						// actually cannot possibly ever match this selector.
						selector = std::make_shared<BinarySelector>(BinarySelector::SelectorOperator::Intersection, std::make_shared<Selector>(Selector::SelectorOperator::Dummy), selector);
					}
				}
				break;

				default:
				{					
					notDone = false;
				}
				break;
			}

			if (ret == nullptr)
			{
				ret = selector;
			}
			else if(selector != nullptr)
			{
				ret = std::make_shared<BinarySelector>(BinarySelector::SelectorOperator::Intersection, ret, selector);
			}
		}

		if (ret == nullptr)
		{
			throw std::runtime_error(u8"In Parser::ParseSimpleSelectorSequence(boost::string_ref&) - Failed to generate a single selector. The supplied selector string must have been invalid.");
		}

		return ret;
	}

	SharedSelector Parser::ParsePseudoclassSelector(boost::string_ref& selectorStr) const
	{
		if (selectorStr.size() == 0 || selectorStr[0] != ':')
		{
			throw std::runtime_error(u8"In Parser::ParsePseudoclassSelector(boost::string_ref&) - Expected pseudo class selector string.");
		}

		selectorStr = selectorStr.substr(1);

		boost::string_ref name = ParseIdentifier(selectorStr);

		// Unfortunately, we need to copy out to a new string so we can force it to lower case 
		std::string nameAsString = name.to_string();
		boost::to_lower(nameAsString);
		name = boost::string_ref(nameAsString);

		const auto pseudoOperatorResult = PseudoOps.find(name);

		if (pseudoOperatorResult == PseudoOps.end())
		{
			std::string errString = u8"In Parser::ParsePseudoclassSelector(boost::string_ref&) - Unsupported Pseudo selector type: " + nameAsString;
			throw std::runtime_error(errString.c_str());
		}

		switch (pseudoOperatorResult->second)
		{
			case PseudoOp::Not:
			case PseudoOp::Has:
			case PseudoOp::HasChild:
			{
				ConsumeOpeningParenthesis(selectorStr);

				SharedSelector sel = ParseSelectorGroup(selectorStr);

				ConsumeClosingParenthesis(selectorStr);

				UnarySelector::SelectorOperator op;

				if (pseudoOperatorResult->second == PseudoOp::Not)
				{
					op = UnarySelector::SelectorOperator::Not;
				}
				else if (pseudoOperatorResult->second == PseudoOp::Has)
				{
					op = UnarySelector::SelectorOperator::HasDescendant;
				}
				else if (pseudoOperatorResult->second == PseudoOp::HasChild)
				{
					op = UnarySelector::SelectorOperator::HasChild;
				}

				return std::make_shared<UnarySelector>(op, sel);
			}
			break;

			case PseudoOp::Contains:
			case PseudoOp::ContainsOwn:
			case PseudoOp::Matches:
			case PseudoOp::MatchesOwn:
			{
				ConsumeOpeningParenthesis(selectorStr);

				boost::string_ref value;

				const char& c = selectorStr[0];
				if (c == '\'' || c == '"')
				{
					value = ParseString(selectorStr);
				}
				else
				{
					value = ParseIdentifier(selectorStr);
				}

				TrimLeadingWhitespace(selectorStr);

				ConsumeClosingParenthesis(selectorStr);

				TextSelector::SelectorOperator op;

				if (pseudoOperatorResult->second == PseudoOp::Contains)
				{
					op = TextSelector::SelectorOperator::Contains;
				}
				else if (pseudoOperatorResult->second == PseudoOp::ContainsOwn)
				{
					op = TextSelector::SelectorOperator::ContainsOwn;
				}
				else if (pseudoOperatorResult->second == PseudoOp::Matches)
				{
					op = TextSelector::SelectorOperator::Matches;
				}
				else if (pseudoOperatorResult->second == PseudoOp::MatchesOwn)
				{
					op = TextSelector::SelectorOperator::MatchesOwn;
				}

				return std::make_shared<TextSelector>(op, value);
			}
			break;

			case PseudoOp::NthChild:
			case PseudoOp::NthLastChild:
			case PseudoOp::NthOfType:
			case PseudoOp::NthLastOfType:
			{
				ConsumeOpeningParenthesis(selectorStr);

				int lhs, rhs;
				ParseNth(selectorStr, lhs, rhs);

				ConsumeClosingParenthesis(selectorStr);

				bool matchLast = (pseudoOperatorResult->second == PseudoOp::NthLastChild || pseudoOperatorResult->second == PseudoOp::NthLastOfType);
				bool matchType = (pseudoOperatorResult->second == PseudoOp::NthOfType || pseudoOperatorResult->second == PseudoOp::NthLastOfType);
				
				return std::make_shared<Selector>(lhs, rhs, matchLast, matchType);
			}
			break;

			case PseudoOp::FirstChild:
			{
				return std::make_shared<Selector>(0, 1, false, false);
			}
			break;

			case PseudoOp::LastChild:
			{
				return std::make_shared<Selector>(0, 1, true, false);
			}
			break;

			case PseudoOp::FirstOfType:
			{
				return std::make_shared<Selector>(0, 1, false, true);
			}
			break;

			case PseudoOp::LastOfType:
			{
				return std::make_shared<Selector>(0, 1, true, true);
			}
			break;

			case PseudoOp::OnlyChild:
			{
				return std::make_shared<Selector>(false);
			}
			break;

			case PseudoOp::OnlyOfType:
			{
				return std::make_shared<Selector>(true);
			}
			break;

			case PseudoOp::Empty:
			{
				return std::make_shared<Selector>(Selector::SelectorOperator::Empty);
			}
			break;
		} /* switch (pseudoOperatorResult->second) */

		return nullptr;
	}

	SharedSelector Parser::ParseAttributeSelector(boost::string_ref& selectorStr) const
	{
		if (selectorStr.size() == 0 || selectorStr[0] != '[')
		{
			throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Expected atrribute selector string.");
		}

		selectorStr = selectorStr.substr(1);
		TrimLeadingWhitespace(selectorStr);

		if (selectorStr.length() == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Expected identifier, reached EOF instead.");
		}

		// This is used for matching attributes not exactly, but by a specific prefix. If this is
		// the case, rather than matching exactly the supplied key against an attribute entry, the
		// attribute entry will be tested if whether or not its name key has the prefix specified.
		// So [^some$="end"] will match:
		//		something="theend"
		//		someone="frontend"
		//		someplace="theplaceattheend"
		//
		// XXX TODO it has been decided that the complexity this adds is undesirable.
		bool doesAttributeHasPrefix = false;
		if (selectorStr[0] == '^')
		{
			throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Attribute name as prefix selector is unsupported.");
			doesAttributeHasPrefix = true;
			selectorStr = selectorStr.substr(1);
		}

		boost::string_ref key = ParseIdentifier(selectorStr);

		if (selectorStr.length() == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - No value for identifier specified and no closing brace found.");
		}

		const char& valueMatchFirstChar = selectorStr[0];

		AttributeSelector::SelectorOperator op;

		size_t trimLength = 2;

		switch (valueMatchFirstChar)
		{
		case ']':
		{
			// This is just an EXISTS attribute selector. So, this will match an element that has
			// the specified attribute, regardless of value. We need to consume the closing bracket
			// though as well.
			selectorStr = selectorStr.substr(1);

			return std::make_shared<AttributeSelector>(key);
		}
		break;

		case '|':
		{
			// This is a hypen delimited list selector where the first attribute value (left to
			// right) starts with a specific value followed by a hyphen, or exactly equals the
			// specified selector value.
			if (selectorStr.length() > 3 && selectorStr[1] == '=')
			{
				op = AttributeSelector::SelectorOperator::ValueIsHyphenSeparatedListStartingWith;
			}
			else
			{
				throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Broken hypen attribute value match supplied.");
			}
		}
		break;

		case '~':
		{
			// This is a whitespace delimited list selector where one of the attribute list items
			// exactly matches a specific value.
			if (selectorStr.length() > 3 && selectorStr[1] == '=')
			{
				op = AttributeSelector::SelectorOperator::ValueContainsElementInWhitespaceSeparatedList;
			}
			else
			{
				throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Broken whitespace attribute value match supplied.");
			}
		}
		break;

		case '^':
		{
			// This is a prefix matching selector where the value of the specified attribute must
			// have a prefix that exactly matches a specific value.
			if (selectorStr.length() > 3 && selectorStr[1] == '=')
			{
				op = AttributeSelector::SelectorOperator::ValueHasPrefix;
			}
			else
			{
				throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Broken prefix attribute value match supplied.");
			}
		}
		break;

		case '$':
		{
			// This is a suffix matching selector where the value of the specified attribute must
			// have a suffix that exactly matches a specific value.
			if (selectorStr.length() > 3 && selectorStr[1] == '=')
			{
				op = AttributeSelector::SelectorOperator::ValueHasSuffix;
			}
			else
			{
				throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Broken suffix attribute value match supplied.");
			}
		}
		break;

		case '*':
		{
			// This is a substring matching selector where the value of the specified attribute must
			// contain the a specific substring.
			if (selectorStr.length() > 3 && selectorStr[1] == '=')
			{
				op = AttributeSelector::SelectorOperator::ValueContains;
			}
			else
			{
				throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Broken substring attribute value match supplied.");
			}
		}
		break;

		case '=':
		{
			// This is an exact equality selector, where the value of the specified attribute must
			// exactly match a specific value.
			if (selectorStr.length() >= 3)
			{
				op = AttributeSelector::SelectorOperator::ValueEquals;
				trimLength = 1;
			}
			else
			{
				throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Broken substring attribute value match supplied. Expected value, got EOF.");
			}
		}
		break;

		default:
			throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Invalid attribute value specifier.");
		} /* switch (valueMatchFirstChar) */

		// Trim off the match specifier
		selectorStr = selectorStr.substr(trimLength);

		boost::string_ref value;

		const char& firstValueChar = selectorStr[0];

		if (firstValueChar == '"' || firstValueChar == '\'')
		{
			value = ParseString(selectorStr);
		}
		else
		{
			value = ParseIdentifier(selectorStr);
		}

		TrimLeadingWhitespace(selectorStr);

		if (selectorStr.length() == 0 || selectorStr[0] != ']')
		{
			throw std::runtime_error(u8"In Parser::ParseAttributeSelector(boost::string_ref&) - Expected attribute closing tag aka ']', found invalid character or EOF instead.");
		}

		// Consume the closing bracket
		selectorStr = selectorStr.substr(1);

		return std::make_shared<AttributeSelector>(op, key, value);
	}

	SharedSelector Parser::ParseClassSelector(boost::string_ref& selectorStr) const
	{
		if (selectorStr.size() < 2 || selectorStr[0] != '.')
		{
			throw std::runtime_error(u8"In Parser::ParseClassSelector(boost::string_ref&) - Expected class specifier, got insufficient string or non-class definition.");
		}

		selectorStr = selectorStr.substr(1);

		boost::string_ref className;

		if (selectorStr[0] == '"' || selectorStr[0] == '\'')
		{
			className = ParseString(selectorStr);
		}
		else
		{
			className = ParseIdentifier(selectorStr);
		}

		boost::string_ref clazz = u8"class";

		return std::make_shared<AttributeSelector>(AttributeSelector::SelectorOperator::ValueContainsElementInWhitespaceSeparatedList, clazz, className);
	}

	SharedSelector Parser::ParseIDSelector(boost::string_ref& selectorStr) const
	{
		if (selectorStr.size() < 2 || selectorStr[0] != '#')
		{
			throw std::runtime_error(u8"In Parser::ParseIDSelector(boost::string_ref&) - Expected ID specifier, got insufficient string or non-ID definition.");
		}

		selectorStr = selectorStr.substr(1);

		boost::string_ref elementId;

		if (selectorStr[0] == '"' || selectorStr[0] == '\'')
		{
			elementId = ParseString(selectorStr);
		}
		else
		{
			elementId = ParseName(selectorStr);
		}

		boost::string_ref id = u8"id";
		
		return std::make_shared<AttributeSelector>(AttributeSelector::SelectorOperator::ValueEquals, id, elementId);
	}

	SharedSelector Parser::ParseTypeSelector(boost::string_ref& selectorStr) const
	{
		if (selectorStr.size() == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseTypeSelector(boost::string_ref&) - Expected Tag specifier, got empty string.");
		}

		boost::string_ref tag = ParseIdentifier(selectorStr);
			 
		return std::make_shared<Selector>(gumbo_tag_enum(tag.to_string().c_str()));
	}

	void Parser::ParseNth(boost::string_ref& selectorStr, int& lhs, int& rhs) const
	{
		TrimLeadingWhitespace(selectorStr);

		if (selectorStr.length() == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - Expected Nth(...) definintion, got empty string.");
		}

		const size_t nPosition = selectorStr.find_first_of(u8"nNdD");
		const size_t closingParenPosition = selectorStr.find(')');

		if (closingParenPosition == boost::string_ref::npos)
		{
			throw std::runtime_error(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - No closing parenthesis was found for nth parameter.");
		}

		if (nPosition != boost::string_ref::npos && nPosition < closingParenPosition)
		{
			if (nPosition > 0)
			{
				if (std::isalpha(selectorStr[nPosition - 1], m_localeEnUS))
				{
					// This is either odd, even or an invalid string

					boost::string_ref name = ParseName(selectorStr);
					std::string nameString = name.to_string();
					boost::to_lower(nameString);

					if (nameString.compare(u8"odd") == 0)
					{
						lhs = 2;
						rhs = 1;
					}
					else if (nameString.compare(u8"even") == 0)
					{
						lhs = 2;
						rhs = 0;
					}
					else
					{
						throw std::runtime_error(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - Expected 'odd' or 'even', invalid nth value found.");
					}

					return;
				}
				else
				{
					// This is almost certainly an expression using N, such as -n+ 6
					boost::string_ref wholeParam = selectorStr.substr(0, closingParenPosition);
					boost::string_ref leftHandSide = wholeParam.substr(0, nPosition);
					boost::string_ref rightHandSide = wholeParam.substr(nPosition + 1);

					selectorStr = selectorStr.substr(closingParenPosition);

					std::string lhss = leftHandSide.to_string();
					std::string rhss = rightHandSide.to_string();

					boost::replace_all(lhss, u8"\t\f\r\n ", u8"");
					boost::replace_all(rhss, u8"\t\f\r\n ", u8"");

					// The left hand side could be "-n", etc.
					if (lhss.size() == 1)
					{
						if (lhss[0] == '-')
						{
							lhs = -0;
						}
						else if (lhss[0] == '+')
						{
							lhs = 0;
						}
						else if (std::isdigit(lhss[0], m_localeEnUS))
						{
							lhs = std::atoi(&lhss[0]);
						}
						else
						{
							throw std::runtime_error(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - Single character left hand side of nth is neither '+', '-' or a digit.");
						}
					}
					else
					{
						if (lhss.size() > 0)
						{
							size_t i = 0;

							if (lhss[0] == '-' || lhss[0] == '+')
							{
								i = 1;
							}

							for (i; i < lhss.length(); ++i)
							{
								if (!std::isdigit(lhss[i], m_localeEnUS))
								{
									throw std::runtime_error(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - Nth parameter left hand side contained non-digit input.");
								}
							}

							lhs = std::stoi(lhss);
						}						
					}

					// The right hand side must be just a number, be it positive, negative, doesn't
					// matter. That's all it can be.
					size_t rhsStartPos = 0;

					if (rhss[0] == '-' || rhss[0] == '+')
					{
						rhsStartPos = 1;
					}

					size_t rhsslen = rhss.length();
					
					for (size_t i = rhsStartPos; i < rhsslen; ++i)
					{
						if (!std::isdigit(rhss[i], m_localeEnUS))
						{							
							std::string errMessage(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - Nth parameter right hand side \"");
							errMessage.append(rhss).append(u8"\" contained non-digit input.");
							throw std::runtime_error(errMessage.c_str());
						}
					}

					rhs = std::stoi(rhss);

					return;
				}
			}
			else
			{
				// Possible expression starting with n
				if (selectorStr[0] == 'n' || selectorStr[0] == 'N')
				{
					lhs = 0;

					boost::string_ref wholeParam = selectorStr.substr(0, closingParenPosition);
					boost::string_ref rightHandSide = wholeParam.substr(1);

					selectorStr = selectorStr.substr(closingParenPosition);

					std::string rhss = rightHandSide.to_string();

					boost::replace_all(rhss, u8"\t\f\r\n ", u8"");

					size_t rhsStartPos = 0;

					if (rhss[0] == '-' || rhss[0] == '+')
					{
						rhsStartPos = 1;
					}

					for (size_t i = rhsStartPos; i < rhss.length(); ++i)
					{
						if (!std::isdigit(rhss[i], m_localeEnUS))
						{
							std::string errMessage(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - Nth parameter right hand side \"");
							errMessage.append(rhss).append(u8"\" contained non-digit input.");
							throw std::runtime_error(errMessage.c_str());
						}
					}

					rhs = std::stoi(rhss);
				}
				else
				{
					throw std::runtime_error(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - Nth parameter starts with alphabetical character other than N.");
				}
			}
		}
		else
		{
			// No 'N' is part of the nth parameter, so it must be a single integer value

			boost::string_ref wholeParam = selectorStr.substr(0, closingParenPosition);

			selectorStr = selectorStr.substr(closingParenPosition);

			std::string paramStr = wholeParam.to_string();

			boost::replace_all(paramStr, u8"\t\f\r\n ", u8"");

			size_t startPos = 0;

			if (paramStr[0] == '-' || paramStr[0] == '+')
			{
				startPos = 1;
			}

			for (size_t i = startPos; i < paramStr.length(); ++i)
			{
				if (!std::isdigit(paramStr[i], m_localeEnUS))
				{
					std::string errorMessage(u8"In Parser::ParseNth(boost::string_ref&, const int& ,const int&) - Single integer Nth parameter contained non-digit input. String is: ");
					errorMessage.append(paramStr);
					throw std::runtime_error(errorMessage.c_str());
				}
			}

			rhs = std::stoi(paramStr);

			lhs = 0;

			return;
		}
	}

	const int Parser::ParseInteger(boost::string_ref& selectorStr) const
	{
		if (selectorStr.length() == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseInteger(boost::string_ref&) - Expected number in string representation, got empty string.");
		}

		TrimLeadingWhitespace(selectorStr);

		size_t endPos = 0;

		if (selectorStr[0] == '-' || selectorStr[1] == '+')
		{
			endPos = 1;
		}

		if (!std::isdigit(selectorStr[endPos], m_localeEnUS))
		{
			throw std::runtime_error(u8"In Parser::ParseInteger(boost::string_ref&) - Expected number in string representation, got non-digit characters.");
		}

		++endPos;

		if (endPos >= selectorStr.size())
		{
			throw std::runtime_error(u8"In Parser::ParseInteger(boost::string_ref&) - Expected number in string representation, got EOF instead.");
		}

		while (endPos < selectorStr.size())
		{
			if (!std::isdigit(selectorStr[endPos], m_localeEnUS))
			{
				break;
			}

			endPos++;
		}

		boost::string_ref numStr = selectorStr.substr(endPos);
		selectorStr = selectorStr.substr(endPos);

		return stoi(numStr.to_string());
	}

	void Parser::ConsumeClosingParenthesis(boost::string_ref& selectorStr) const
	{
		TrimLeadingWhitespace(selectorStr);

		if (selectorStr.size() == 0 || selectorStr[0] != ')')
		{
			std::string errorMessage(u8"In Parser::ConsumeClosingParenthesis(boost::string_ref&) - Expected string with closing parenthesis, got empty string or string not preceeded by opening parenthesis. String Is: ");
			errorMessage.append(selectorStr.to_string());
			throw std::runtime_error(errorMessage.c_str());
		}

		selectorStr = selectorStr.substr(1);
	}

	void Parser::ConsumeOpeningParenthesis(boost::string_ref& selectorStr) const
	{
		if (selectorStr.size() == 0 || selectorStr[0] != '(')
		{
			std::string errorMessage(u8"In Parser::ConsumeOpeningParenthesis(boost::string_ref&) - Expected string with opening parenthesis, got empty string or string not preceeded by opening parenthesis. String Is: ");
			errorMessage.append(selectorStr.to_string());
			throw std::runtime_error(errorMessage.c_str());
		}

		selectorStr = selectorStr.substr(1);

		TrimLeadingWhitespace(selectorStr);
	}

	const bool Parser::TrimLeadingWhitespace(boost::string_ref& str) const
	{
		// The original SkipWhitespace method in the gumbo-query CParser class not only skipped over
		// whitespace, but also skipped over comments like /*....*/. I can't see a reason to
		// implement this functionality. There should not be comments in supplied selector strings.

		if (str.size() == 0)
		{
			return false;
		}

		bool trimmed = false;

		while (str.length() > 0)
		{
			if (std::isspace(str[0], m_localeEnUS))
			{
				str = str.substr(1);
				trimmed = true;
			}
			else
			{
				break;
			}
		}

		return trimmed;
	}

	boost::string_ref Parser::ParseString(boost::string_ref& selectorStr) const
	{
		// This method assumes it has been called when the first character in the supplied
		// string_ref is either a ' or " quote. This function is a complete rewrite over the
		// original gumbo-query version, as the gumbo-query version was needlessly complex and took
		// all escape sequences into account, unescaping character code points. This method simply
		// looks for a valid opening and closing quote and takes everything in between that and an
		// unescaped closing quote of the same character.

		if (selectorStr.size() == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseString(boost::string_ref&) - Expected quoted string, got empty string.");
		}

		const char& quoteChar = selectorStr[0];

		if (quoteChar != '\'' && quoteChar != '"')
		{
			throw std::runtime_error(u8"In Parser::ParseString(boost::string_ref&) - Expected quoted string, string does not begin with valid quote characters.");
		}

		// Remove the opening quote
		selectorStr = selectorStr.substr(1);

		auto pos = selectorStr.find_first_of(quoteChar);

		if (pos == std::string::npos)
		{
			throw std::runtime_error(u8"In Parser::ParseString(boost::string_ref&) - No closing quote found in supplied quoted string.");
		}
		else if (pos == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseString(boost::string_ref&) - String begins with unescaped quote character.");
		}

		size_t endOffset = 0;
		
		for (pos; pos < selectorStr.size(); ++pos)
		{
			if (selectorStr[pos] == quoteChar)
			{
				if (selectorStr[pos - 1] == '\\')
				{
					// Escaped quotes don't count. Skip.
					continue;
				}
				else
				{
					endOffset = pos;
					break;
				}
				
			}
		}

		if (endOffset == boost::string_ref::npos || endOffset > selectorStr.size())
		{
			throw std::runtime_error(u8"In Parser::ParseString(boost::string_ref&) - No unescaped closing quote found in supplied quoted string.");
		}

		boost::string_ref value = selectorStr.substr(0, endOffset);

		selectorStr = selectorStr.substr(endOffset + 1);

		return value;
	}

	boost::string_ref Parser::ParseName(boost::string_ref& selectorStr) const
	{
		return ParseIdentifier(selectorStr);
	}

	boost::string_ref Parser::ParseIdentifier(boost::string_ref& selectorStr) const
	{
		if (selectorStr.size() == 0)
		{
			throw std::runtime_error(u8"In Parser::ParseIdentifier(boost::string_ref&) - Expected selector containing identifier, got empty string.");
		}

		bool notDone = true;

		int ind = 0;

		while (notDone && ind < static_cast<int>(selectorStr.size()))
		{
			if (selectorStr[ind] == '&')
			{
				boost::string_ref sub = selectorStr.substr(ind);
				auto endPos = sub.find(';');

				if (endPos == boost::string_ref::npos)
				{
					throw std::runtime_error(u8"In Parser::ParseIdentifier(boost::string_ref&) - Encountered improperly formatted named or numbered character reference.");
				}

				ind += static_cast<int>(endPos+1);

				continue;
			}
			else if (selectorStr[ind] == '\\')
			{
				++ind;

				// Since Gumbo Parser directly embeds escaped character sequences directly, and
				// unmodified, we need to accept them as well. These will always, in a properly
				// formatted element, be followed by a space in order to make it clear that the
				// characters immediately following a '\\' and then followed by a space are the hex
				// value for a unicode character.
				bool foundEscapeSequenceEnd = false;
				for (ind; ind < static_cast<int>(selectorStr.size()); ++ind)
				{
					if (IsSpecial(selectorStr[ind]))
					{
						++ind;
						foundEscapeSequenceEnd = true;
						break;
					}
					else if (IsHexDigit(selectorStr[ind]))
					{
						continue;
					}
					else if (std::isspace(selectorStr[ind], m_localeEnUS))
					{
						++ind;
						foundEscapeSequenceEnd = true;
						break;
					}
				}

				if (!foundEscapeSequenceEnd)
				{
					throw std::runtime_error(u8"In Parser::ParseIdentifier(boost::string_ref&) - Encountered improperly formatted character escape sequence. Escaped character sequences must be followed by a space.");
				}
			}
			else if (!IsNameChar(selectorStr[ind]))
			{
				notDone = false;
				break;
			}

			++ind;
		}		

		if (ind <= 0)
		{
			throw std::runtime_error(u8"In Parser::ParseIdentifier(boost::string_ref&) - Expected selector containing identifier, yet no valid identifier was found.");
		}		

		if (ind > static_cast<int>(selectorStr.size()))
		{
			ind = static_cast<int>(selectorStr.size());
		}

		boost::string_ref value = selectorStr.substr(0, ind);
		selectorStr = selectorStr.substr(ind);		
		
		return value;
	}

	const bool Parser::IsNameChar(const char& c) const
	{
		// We need to be able to support alphabet, 0-9, underscores
		return IsNameStart(c) || (c == '-') || (c >= '0' && c <= '9');
	}

	const bool Parser::IsNameStart(const char& c) const
	{
		return std::isalpha(c, m_localeEnUS) || c == '_';
	}

	const bool Parser::IsCombinator(const char& c) const
	{
		switch (c)
		{
			case ' ':
			case '~':
			case '>':
			case '+':
			{
				return true;
			}
			break;

			default:
				return false;
		}
	}

	const bool Parser::IsSpecial(const char& c) const
	{
		switch (c)
		{
			case ' ':
			case '~':
			case '>':
			case '+':
			case ':':
			case '|':
			case '*':
			case ';':
			case '&':
			case ',':
			{
				return true;
			}
			break;

			default:
				return false;
		}
	}

	const bool Parser::IsHexDigit(const char& c) const
	{
		return std::isxdigit(c, m_localeEnUS);
	}

} /* namespace gq */
