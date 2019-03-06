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

#pragma once

#include "Selector.hpp"
#include <boost/utility/string_ref.hpp>

namespace gq
{

	/// <summary>
	/// The AttributeSelector, as the name implies, is designed for matching against nodes using
	/// attribute selectors.
	/// </summary>
	class AttributeSelector final : public Selector
	{

	public:

		/// <summary>
		/// More information here: http://www.w3.org/TR/css3-selectors/#attribute-selectors
		/// </summary>
		enum SelectorOperator
		{
			/// <summary>
			/// Simply verify that the supplied attribute exists.
			/// </summary>
			Exists,

			/// <summary>
			/// The attribute value must exactly match a specific value.
			/// </summary>
			ValueEquals,

			/// <summary>
			/// The attribute value must have a prefix matching a specific value.
			/// </summary>
			ValueHasPrefix,

			/// <summary>
			/// The attribute value must have a suffix matching a specific value.
			/// </summary>
			ValueHasSuffix,

			/// <summary>
			/// The attribute value must contain a substring matching a specific value.
			/// </summary>
			ValueContains,

			/// <summary>
			/// The attribute value must either be an exact match to the specified selector value,
			/// or the attribute value must be a whitespace delimited list where one of the list
			/// entries exactly matches a specific value. This is used, for example, for class
			/// selectors.
			/// </summary>
			ValueContainsElementInWhitespaceSeparatedList,

			/// <summary>
			/// The attribute value must either be an exact match to the specified selector value,
			/// or must start by exactly matching the selector value and be immediately followed by
			/// a hyphen.
			/// </summary>
			ValueIsHyphenSeparatedListStartingWith,
		};

		/// <summary>
		/// Constructs an attribute selector that can only possibly be used for Exists matching. If
		/// the supplied key has a length of zero, this constructor will throw. It is not logical
		/// and therefore not possible to construct a selector with no parameter to function on.
		/// </summary>
		/// <param name="key">
		/// The attribute name to match if it exists. 
		/// </param>
		AttributeSelector(boost::string_ref key);

		/// <summary>
		/// Constructs an attribute selector with a supplied attribute name and value to match
		/// against in a fashion that is according to the supplied SelectorOperator argument. If the
		/// supplied key or value has a length of zero, this constructor will throw. It is not
		/// logical and therefore not possible to construct a selector with no parameter to
		/// function on.
		/// </summary>
		/// <param name="op">
		/// The operator for matching. This value will define how the supplied attribute value is
		/// matched.
		/// </param>
		/// <param name="key">
		/// The attribute name to match. 
		/// </param>
		/// <param name="value">
		/// The attribute value to match. 
		/// </param>
		AttributeSelector(SelectorOperator op, boost::string_ref key, boost::string_ref value);

		/// <summary>
		/// Default destructor.
		/// </summary>
		virtual ~AttributeSelector();

		/// <summary>
		/// Check if this selector is a match against the supplied node. 
		/// </summary>
		/// <param name="node">
		/// The node to attempt to match against. 
		/// </param>
		/// <returns>
		/// True if this selector was successfully matched against the supplied node, false
		/// otherwise.
		/// </returns>
		virtual const MatchResult Match(const Node* node) const;		

	private:

		/// <summary>
		/// Defines how the matching in this selector will work. Based on the option, the attribute
		/// name and value to be matched will be matched in different ways. See comments in the
		/// operator class.
		/// </summary>
		SelectorOperator m_operator;

		/// <summary>
		/// The name of the attribute to search for.
		/// </summary>
		std::string m_attributeNameString;

		/// <summary>
		/// A string_ref wrapper for m_attributeName.
		/// </summary>
		boost::string_ref m_attributeNameRef;

		/// <summary>
		/// The attribute value to match.
		/// </summary>
		std::string m_attributeValueString;

		/// <summary>
		/// A string_ref wrapper for m_attributeValueRef.
		/// </summary>
		boost::string_ref m_attributeValueRef;

	};

} /* namespace gq */

