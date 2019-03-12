#include <string>

std::string base64_encode(unsigned char const*, unsigned int len);

inline std::string base64_encode(std::string const& s)
{
    return base64_encode(reinterpret_cast<const unsigned char*>(s.c_str()), s.size());
}

std::string base64_decode(std::string const& s);
