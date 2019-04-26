#include <iostream>
#include <stdexcept>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <sstream>

#include <libserialport.h>

#include <boost/algorithm/hex.hpp>
#include <boost/endian/conversion.hpp>

static auto print_hex()
{
    return [](auto const& hex)
    {
        std::string s;
        boost::algorithm::hex(std::begin(hex), std::end(hex), std::back_inserter(s));
        std::cout << s << '\n';
    };
}

std::unique_ptr<sp_port, decltype(&sp_close)> open_port(std::string const& name)
{
    sp_port * port = nullptr;
    auto err = sp_get_port_by_name(name.c_str(), &port);
    if (err != SP_OK)
    {
        throw std::runtime_error("Could not find port name");
    }

    err = sp_open(port, SP_MODE_READ_WRITE);
    if (err != SP_OK)
    {
        std::ostringstream os;
        os << "Failed opening port: " << err << '\n';
        throw std::runtime_error(os.str());
    }
    static int counter = 0;
    std::cout << "Opened port: " << counter << " number of times\n";
    ++counter;
    sp_set_baudrate(port, 9600);
    sp_set_parity(port, SP_PARITY_NONE);
    sp_set_bits(port, 8);
    sp_set_stopbits(port, 1);
    sp_set_dtr(port, SP_DTR_ON);
    // sp_set_dsr(port, SP_DSR_FLOW_CONTROL);

    return std::unique_ptr<sp_port, decltype(&sp_close)>(port, sp_close);
}

uint32_t read_from_port(sp_port * port, uint32_t bytes, unsigned char * out_buffer)
{
    uint32_t total_read = sp_blocking_read(port, out_buffer, bytes, 4000);
    std::cout << "Read: " << total_read << "/" << bytes << '\n';
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ms);
    return total_read;
}

template<typename OIter>
void insert_number(uint32_t number, OIter iter)
{
    auto out = boost::endian::native_to_big(number);
    std::copy(reinterpret_cast<const char*>(&out),
              reinterpret_cast<const char*>(&out)+sizeof(number),
              iter);
}

std::array<unsigned char, 512> create_read_sram_request(uint32_t address, uint32_t size)
{

    std::array<unsigned char, 512> full_buffer {{}};
    static constexpr char header[] = "USBA\x00\x01\x40";

    std::copy(std::begin(header), std::end(header), full_buffer.begin());
    insert_number(size, full_buffer.begin()+252);
    insert_number(address, full_buffer.begin()+252+sizeof(size));

    return full_buffer;
}

std::vector<unsigned char> read_bytes(sp_port * port, uint32_t size)
{
    uint32_t read{};
    std::vector<unsigned char> bytes(size);
    int recv_count = 0;
    while (read < size)
    {
        std::vector<uint8_t> b(512);
        read += read_from_port(port, 512, &b[0]);
        std::cout << "Read: " << read << "/" << size << " bytes\n";
        std::cout << recv_count++ << '\n';
        print_hex()(b);
    }

    return bytes;
}

std::vector<unsigned char> read_sram(sp_port * port, uint32_t address, uint32_t bytes)
{
    auto const request = create_read_sram_request(address, bytes);

    std::cout << "Serial: Writing request\n";
    auto const result = sp_blocking_write(port, request.data(), request.size(), 4000);
    if (result != request.size())
    {
        std::cerr << "Error: " << result << '\n';
        throw std::runtime_error("Failed writing");
    }

    std::cout << "Serial: Reading response\n";
    auto const data = read_bytes(port, bytes);

    return data;


    unsigned char recv_buffer[512] {};
    auto const read = read_from_port(port, sizeof(recv_buffer), recv_buffer);
    if (read != sizeof(recv_buffer))
    {
        throw std::runtime_error("Failed reading");
    }

    print_hex()(recv_buffer);

    uint32_t to_read {};
    std::copy(&recv_buffer[252], &recv_buffer[252]+sizeof(to_read), reinterpret_cast<char*>(&to_read));
    to_read = boost::endian::big_to_native(to_read);

    std::cout << "SRAM to read: " << to_read << '\n';
    if (to_read != bytes)
    {
        throw std::runtime_error("Failed to read size");
    }

    std::vector<unsigned char> sram_buffer(to_read);
    if (read_from_port(port, to_read, sram_buffer.data()) != to_read)
    {
        throw std::runtime_error("Failed reading sram");
    }

    std::cout << "Read SRAM\n";
    
    return sram_buffer;
}

struct SuperMetroid
{
    std::string name;
    unsigned int byte_offset;
    unsigned char mask;
};

static std::map<std::string, SuperMetroid> super_metroid
{
     {"morph_ball"   , {"Morphing Ball", 0xB3, 0x04}}
    ,{"bombs"        , {"Bomb",          0xB0, 0x80}}
    ,{"spring_ball"  , {"Spring Ball", 0xC2, 0x40}}
    ,{"high_jump"    , {"High Jump", 0xB6, 0x20}}
    ,{"varia_suite"  , {"Varia Suite", 0xB6, 0x01}}
    ,{"gravity_suite", {"Gravity Suite", 0xC0, 0x80}}
    ,{"speed_booster", {"Speed Booster", 0xB8, 0x04}}
    ,{"space_jump"   , {"Space Jump", 0xC3, 0x04}}
    ,{"screw"        , {"Screw Attack", 0xB9, 0x80}}
    ,{"charge"       , {"Charge", 0xB2, 0x80}}
    ,{"ice"          , {"Ice Beam", 0xB6, 0x04}}
    ,{"wave"         , {"Wave Beam", 0xB8, 0x10}}
    ,{"spacer"       , {"Spacer Beam", 0xB5, 0x04}}
    ,{"plasma"       , {"Plasma Beam", 0xC1, 0x80}}
    ,{"grappling"    , {"Grappling Hook", 0xB7, 0x10}}
    ,{"x-ray"        , {"X-Ray", 0xB4, 0x40}}
    ,{"kraid"        , {"Kraid", 0x69, 0x01}}
    ,{"phantoon"     , {"Phantoon", 0x6B, 0x01}}
    ,{"botwoon"      , {"Botwoon", 0x6C, 0x02}}
    ,{"draygon"      , {"Draygoon", 0x6C, 0x01}}
    ,{"ridley"       , {"Ridley", 0x6A, 0x01}}
    ,{"pb_red_tower" , {"Power Bombs (Red Tower)", 0xB5, 0x01}}
    ,{"golden"       , {"Golden", 0x61, 0x04}}
    ,{"mb1"          , {"Mother Brain 1", 0x60, 0x04}}
    ,{"mb3"          , {"Mother Brain 3", 0x6D, 0x02}}
    ,{"ship"         , {"Ship", 0xB5, 0x01}}
};

static constexpr uint32_t base_address = 0xf50000;

std::map<std::string, bool> get_sm_state(sp_port * serial_port)
{
    static constexpr uint32_t base_items_address = base_address + 0xd7c0;
    auto const sram = read_sram(serial_port, 0xf5d7c0, 512);
    using namespace std::chrono_literals;

    std::cout << "SRAM: \n";
    print_hex()(sram);
    auto m = [](auto const& sram)
    {
        std::map<std::string, bool> state;
        for (auto & [key,value] : super_metroid)
        {
            state[key] = sram[value.byte_offset] & value.mask;
        }

        return state;
    }(sram);

    return m;
}

bool game_started(sp_port * serial_port)
{
    static constexpr uint32_t autostart_address = base_address + 0x0998;
    auto const autostart = read_sram(serial_port, autostart_address, 64);
    std::cout << "SRAM: \n";
    print_hex()(autostart);

    return autostart.size() && autostart[0] == 0x1f;
}

bool entered_ship(sp_port * serial_port)
{
    static constexpr uint32_t done_address = base_address + 0xfb2;
    auto const done = read_sram(serial_port, done_address, 64);
    std::cout << "SRAM: \n";
    print_hex()(done);

    return done.size() >= 2 && done[0] == 0x4f && done[1] == 0xaa;
}
