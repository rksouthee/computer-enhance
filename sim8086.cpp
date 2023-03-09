#include <bitset>
#include <format>
#include <iostream>
#include <vector>

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdint>

using s8 = std::int8_t;
using s16 = std::int16_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;

const char* s_reg[2][8] = {
	{ "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" },
	{ "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" },
};

const char* effective_address[8] = {
	"bx + si",
	"bx + di",
	"bp + si",
	"bp + di",
	"si",
	"di",
	"bp",
	"bx",
};

std::vector<u8> file_read_entire(const char* file_name)
{
	std::vector<u8> result;
	FILE* const file = std::fopen(file_name, "rb");
	if (file)
	{
		if (std::fseek(file, 0, SEEK_END) == 0)
		{
			const long file_size = std::ftell(file);
			if (file_size > 0 && std::fseek(file, 0, SEEK_SET) == 0)
			{
				result.resize(file_size, 0);
				const std::size_t read = std::fread(&result[0], result.size(), 1, file);
				if (read == 1)
				{
				}
				else
				{
					result.clear();
				}
			}
			else
			{
			}
		}
	}
	return result;
}

const u8* load_little_endian(const u8* data, u16& result)
{
	result = data[0] | (static_cast<unsigned>(data[1]) << 8);
	data += 2;
	return data;
}

const u8* load_little_endian(const u8* data, u8& result)
{
	result = *data++;
	return data;
}

const u8* decode(const u8* data, const u8* end)
{
	assert(data != end);
	u8 byte = *data++;
	if ((byte >> 2) == 0b100010)
	{
		std::cout << "mov ";
		unsigned char w = (byte >> 0) & 0x1;
		unsigned char d = (byte >> 1) & 0x1;
		const auto output_operands = [d](const auto& reg, const auto& r_m) {
			if (d == 1)
			{
				// the reg register is the destination and
				// the r/m register is the source
				std::cout << reg << ", " << r_m;
			}
			else
			{
				// the r/m register is the destination and
				// the reg register is the source
				std::cout << r_m << ", " << reg;
			}
		};

		byte = *data++;
		unsigned char r_m = (byte >> 0) & 0x7;
		unsigned char reg = (byte >> 3) & 0x7;
		unsigned char mod = (byte >> 6) & 0x3;
		if (mod == 0b00)
		{
			std::string r_m_str;
			if (r_m == 0b110)
			{
				u16 displacement = 0;
				assert((end - data) >= 2);
				data = load_little_endian(data, displacement);
				r_m_str = std::format("[{}]", static_cast<s16>(displacement));
			}
			else
			{
				r_m_str = std::format("[{}]", effective_address[r_m]);
			}
			const char* reg_str = s_reg[w][reg];
			output_operands(reg_str, r_m_str);
		}
		else if (mod == 0b01)
		{
			u8 displacement = 0;
			data = load_little_endian(data, displacement);
			const char* const reg_str = s_reg[w][reg];
			std::string r_m_str;
			auto d16 = static_cast<s16>(static_cast<s8>(displacement));
			if (d16 > 0)
			{
				r_m_str = std::format("[{} + {}]", effective_address[r_m], d16);
			}
			else if (d16 < 0)
			{
				r_m_str = std::format("[{} - {}]", effective_address[r_m], -d16);
			}
			else
			{
				r_m_str = std::format("[{}]", effective_address[r_m]);
			}
			output_operands(reg_str, r_m_str);
		}
		else if (mod == 0b10)
		{
			u16 displacement = 0;
			data = load_little_endian(data, displacement);
			const char* const reg_str = s_reg[w][reg];
			auto d16 = static_cast<s16>(displacement);
			std::string r_m_str;
			if (d16 > 0)
			{
				r_m_str = std::format("[{} + {}]", effective_address[r_m], d16);
			}
			else if (d16 < 0)
			{
				r_m_str = std::format("[{} - {}]", effective_address[r_m], -d16);
			}
			else
			{
				r_m_str = std::format("[{}]", effective_address[r_m]);
			}
			output_operands(reg_str, r_m_str);
		}
		else if (mod == 0b11)
		{
			output_operands(s_reg[w][reg], s_reg[w][r_m]);
		}
		std::cout << std::endl;
	}
	else if ((byte >> 4) == 0b1011)
	{
		std::cout << "mov ";
		const u8 w = (byte >> 3) & 0x1;
		const u8 reg = (byte >> 0) & 0x7;
		if (w == 0)
		{
			u8 displacement = 0;
			data = load_little_endian(data, displacement);
			std::cout << s_reg[w][reg] << ", " << static_cast<unsigned>(displacement);
		}
		else
		{
			u16 displacement = 0;
			data = load_little_endian(data, displacement);
			std::cout << s_reg[w][reg] << ", " << displacement;
		}
		std::cout << std::endl;
	}
	else if ((byte >> 1) == 0b1100011)
	{
		const u8 w = (byte >> 0) & 0x1;
		byte = *data++;
		const u8 mod = (byte >> 6) & 0x3;
		const u8 r_m = (byte >> 0) & 0x7;
		std::string r_m_str;
		if (mod == 0b00)
		{
			r_m_str = std::format("[{}]", effective_address[r_m]);
		}
		else if (mod == 0b01)
		{
			u8 displacement = 0;
			data = load_little_endian(data, displacement);
			r_m_str = std::format("[{} + {}]", effective_address[r_m], displacement);
		}
		else if (mod == 0b10)
		{
			u16 displacement = 0;
			data = load_little_endian(data, displacement);
			r_m_str = std::format("[{} + {}]", effective_address[r_m], displacement);
		}
		else if (mod == 0b11)
		{
			r_m_str = s_reg[w][r_m];
		}

		std::string value_str;
		if (w == 1)
		{
			u16 value = 0;
			data = load_little_endian(data, value);
			value_str = "word " + std::to_string(value);
		}
		else
		{
			u8 value = 0;
			data = load_little_endian(data, value);
			value_str = "byte " + std::to_string(value);
		}

		std::cout << "mov " << r_m_str << ", " << value_str << std::endl;
	}
	else if ((byte >> 1) == 0b1010000)
	{
		const u8 w = (byte >> 1) & 0x1;
		u16 address = 0;
		data = load_little_endian(data, address);
		std::cout << "mov ax, [" << address << ']' << std::endl;
	}
	else if ((byte >> 1) == 0b1010001)
	{
		const u8 w = (byte >> 1) & 0x1;
		u16 address = 0;
		data = load_little_endian(data, address);
		std::cout << "mov [" << address << "], ax" << std::endl;
	}
	else
	{
		std::cout << std::bitset<8>(byte) << std::endl;
		assert(!"Shouldn't be here!");
	}
	return data;
}

int main(int argc, char** argv)
{
	if (argc == 2)
	{
		const char* file_name = argv[1];
		std::cout << "; " << file_name << "\nbits 16\n";
		const std::vector<u8> data = file_read_entire(file_name);
		auto data_begin = static_cast<const u8*>(data.data());
		auto data_end = data_begin + data.size();
		while (data_begin != data_end)
		{
			data_begin = decode(data_begin, data_end);
		}
	}
	else
	{
		std::cerr << "Usage: sim8086 <file>" << std::endl;
	}
	return 0;
}
