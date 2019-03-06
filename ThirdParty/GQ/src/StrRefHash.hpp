/*
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

#include <boost/functional/hash.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string.hpp>

namespace gq
{

	/// <summary>
	/// Hash implementation for string_ref.
	/// </summary>
	struct StringRefHash
	{
		size_t operator()(const boost::string_ref& strRef) const
		{
			return boost::hash_range(strRef.begin(), strRef.end());
		}
	};

	struct StringRefEquality
	{
		bool operator()(const boost::string_ref& strRef1, const boost::string_ref& strRef2) const
		{
			auto oneSize = strRef1.size();
			auto twoSize = strRef2.size();

			if (oneSize != twoSize)
			{
				return false;
			}

			return std::memcmp(strRef1.begin(), strRef2.begin(), oneSize) == 0;
		}
	};

} /* namespace gq */