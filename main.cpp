#include "usb2snes.h"

#include <iostream>
#include <boost/algorithm/hex.hpp>

static auto print_hex()
{
    return [](auto const& hex)
    {
        std::string s;
        boost::algorithm::hex(std::begin(hex), std::end(hex), std::back_inserter(s));
        std::cout << s << '\n';
    };
}

int main()
{
    auto port = open_port("/dev/ttyACM0");

    while (1)
    {
        auto data = read_sram(port.get(), 0xfc2000, 2);
        print_hex()(data);
    }
}
