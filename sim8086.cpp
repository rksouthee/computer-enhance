#include <bitset>
#include <format>
#include <iostream>
#include <unordered_map>
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

struct Instruction
{
	const u8* pointer;
	std::string assembly;
};

std::vector<u8> file_read_entire(const char* file_name)
{
	std::vector<u8> result;
	FILE* file;
	errno_t error = ::fopen_s(&file, file_name, "rb");
	if (error == 0 && file)
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

[[nodiscard]] const u8* load_little_endian(const u8* data, u16& result)
{
	result = data[0] | (static_cast<unsigned>(data[1]) << 8);
	data += 2;
	return data;
}

[[nodiscard]] const u8* load_little_endian(const u8* data, u8& result)
{
	result = *data++;
	return data;
}

[[nodiscard]] const u8* decode_jump(const char* mnemonic, const u8* data, std::string& assembly, std::unordered_map<const u8*, std::string>& labels)
{
	u8 inc8 = 0;
	data = load_little_endian(data, inc8);
	const s8 inc_s8 = static_cast<s8>(inc8);
	std::string& label = labels[data + inc_s8];
	if (label.empty())
	{
		label = std::format("test_label{}", labels.size() - 1);
	}
	assembly = std::format("{} {} ; {}", mnemonic, label, static_cast<int>(inc_s8));
	return data;
}

const u8* read0(const char* const mnemonic, const u8 w, const u8 d, const u8 byte, const u8* data, const u8* const end, std::string& assembly)
{
	const auto output_operands = [d, mnemonic](const auto& reg, const auto& r_m) {
		if (d == 1)
		{
			// the reg register is the destination and
			// the r/m register is the source
			return std::format("{} {}, {}", mnemonic, reg, r_m);
		}
		else
		{
			// the r/m register is the destination and
			// the reg register is the source
			return std::format("{} {}, {}", mnemonic, r_m, reg);
		}
	};

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
		assembly = output_operands(reg_str, r_m_str);
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
		assembly = output_operands(reg_str, r_m_str);
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
		assembly = output_operands(reg_str, r_m_str);
	}
	else if (mod == 0b11)
	{
		assembly = output_operands(s_reg[w][reg], s_reg[w][r_m]);
	}
	return data;
}

const u8* read1(const char* mnemonic, const u8 s, const u8 w, const u8 byte, const u8* data, const u8* const /*end*/, const bool is_arithmetic, std::string& assembly)
{
	const u8 mod = (byte >> 6) & 0x3;
	const u8 reg = (byte >> 3) & 0x7;
	if (is_arithmetic)
	{
		if (reg == 0b000)
		{
			mnemonic = "add ";
		}
		else if (reg == 0b101)
		{
			mnemonic = "sub ";
		}
		else if (reg == 0b111)
		{
			mnemonic = "cmp ";
		}
		else
		{
			std::cout << "Invalid reg: " << std::bitset<3>(reg) << std::endl;
			assert(!"Shouldn't be here!");
		}
	}
	const u8 r_m = (byte >> 0) & 0x7;
	std::string r_m_str;
	if (mod == 0b00)
	{
		if (r_m == 0b110)
		{
			u16 displacement = 0;
			data = load_little_endian(data, displacement);
			r_m_str = std::format("[{}]", static_cast<s16>(displacement));
		}
		else
		{
			r_m_str = std::format("[{}]", effective_address[r_m]);
		}
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
	if (s == 0 && w == 1)
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
	assembly = std::format("{}{}, {}", mnemonic, r_m_str, value_str);
	return data;
}

const u8* decode(const u8* data, const u8* end, std::string& assembly, std::unordered_map<const u8*, std::string>& labels)
{
	assert(data != end);
	u8 byte = *data++;
	if ((byte >> 2) == 0b100010)
	{
		// MOV: Register/memory to/from register
		unsigned char w = (byte >> 0) & 0x1;
		unsigned char d = (byte >> 1) & 0x1;
		byte = *data++;
		data = read0("mov ", w, d, byte, data, end, assembly);
	}
	else if ((byte >> 4) == 0b1011)
	{
		// MOV: Immediate to register
		const u8 w = (byte >> 3) & 0x1;
		const u8 reg = (byte >> 0) & 0x7;
		if (w == 0)
		{
			u8 displacement = 0;
			data = load_little_endian(data, displacement);
			assembly = std::format("mov {}, {}", s_reg[w][reg], static_cast<unsigned>(displacement));
		}
		else
		{
			u16 displacement = 0;
			data = load_little_endian(data, displacement);
			assembly = std::format("mov {}, {}", s_reg[w][reg], displacement);
		}
	}
	else if ((byte >> 1) == 0b1100011)
	{
		// MOV: Immediate to register/memory
		const u8 w = (byte >> 0) & 0x1;
		byte = *data++;
		data = read1("mov", 0, w, byte, data, end, false, assembly);
	}
	else if ((byte >> 1) == 0b1010000)
	{
		// MOV: Memory to accumulator
		const u8 w = (byte >> 1) & 0x1;
		u16 address = 0;
		data = load_little_endian(data, address);
		if (w == 1)
		{
			assembly = std::format("mov ax, [{}]", address);
		}
		else
		{
			assembly = std::format("mov al, [{}]", address);
		}
	}
	else if ((byte >> 1) == 0b1010001)
	{
		// MOV: Accumulator to memory
		const u8 w = (byte >> 1) & 0x1;
		u16 address = 0;
		data = load_little_endian(data, address);
		if (w == 1)
		{
			assembly = std::format("mov [{}], ax", address);
		}
		else
		{
			assembly = std::format("mov [{}], al", address);
		}
	}
	else if ((byte >> 2) == 0b000000)
	{
		// ADD: Reg/memory with register to either
		const u8 d = (byte >> 1) & 0x1;
		const u8 w = (byte >> 0) & 0x1;
		byte = *data++;
		data = read0("add", w, d, byte, data, end, assembly);
	}
	else if ((byte >> 2) == 0b100000)
	{
		// ADD/SUB/CMP: Immediate to register/memory
		const u8 s = (byte >> 1) & 0x1;
		const u8 w = (byte >> 0) & 0x1;
		byte = *data++;
		data = read1(nullptr, s, w, byte, data, end, true, assembly);
	}
	else if ((byte >> 1) == 0b0000010)
	{
		// ADD: Immediate to accumulator
		const u8 w = (byte >> 0) & 0x1;
		if (w == 1)
		{
			u16 value = 0;
			data = load_little_endian(data, value);
			assembly = std::format("add ax, {}", static_cast<s16>(value));
		}
		else
		{
			u8 value = 0;
			data = load_little_endian(data, value);
			assembly = std::format("add al, {}", static_cast<s16>(static_cast<s8>(value)));
		}
	}
	else if ((byte >> 2) == 0b001010)
	{
		// SUB: Reg/memory and register to either
		const u8 d = (byte >> 1) & 0x1;
		const u8 w = (byte >> 0) & 0x1;
		byte = *data++;
		data = read0("sub", w, d, byte, data, end, assembly);
	}
	else if ((byte >> 1) == 0b0010110)
	{
		// SUB: Immediate from accumulator
		const u8 w = (byte >> 0) & 0x1;
		if (w == 1)
		{
			u16 value = 0;
			data = load_little_endian(data, value);
			assembly = std::format("sub ax, ", static_cast<s16>(value));
		}
		else
		{
			u8 value = 0;
			data = load_little_endian(data, value);
			assembly = std::format("sub al, ", static_cast<s16>(static_cast<s8>(value)));
		}
	}
	else if ((byte >> 2) == 0b001110)
	{
		const u8 d = (byte >> 1) & 0x1;
		const u8 w = (byte >> 0) & 0x1;
		byte = *data++;
		data = read0("cmp", w, d, byte, data, end, assembly);
	}
	else if ((byte >> 1) == 0b0011110)
	{
		// CMP: Immediate with accumulator
		const u8 w = (byte >> 0) & 0x1;
		if (w == 1)
		{
			u16 value = 0;
			data = load_little_endian(data, value);
			assembly = std::format("cmp ax, {}", static_cast<s16>(value));
		}
		else
		{
			u8 value = 0;
			data = load_little_endian(data, value);
			assembly = std::format("cmp al, {}", static_cast<s16>(static_cast<s8>(value)));
		}
	}
	else if (byte == 0b01110101)
	{
		data = decode_jump("jnz", data, assembly, labels);
	}
	else if (byte == 0b01110100)
	{
		data = decode_jump("jz", data, assembly, labels);
	}
	else if (byte == 0b01111100)
	{
		data = decode_jump("jl", data, assembly, labels);
	}
	else if (byte == 0b01111110)
	{
		data = decode_jump("jle", data, assembly, labels);
	}
	else if (byte == 0b01110010)
	{
		data = decode_jump("jb", data, assembly, labels);
	}
	else if (byte == 0b01110110)
	{
		data = decode_jump("jbe", data, assembly, labels);
	}
	else if (byte == 0b01111010)
	{
		data = decode_jump("jp", data, assembly, labels);
	}
	else if (byte == 0b01110000)
	{
		data = decode_jump("jo", data, assembly, labels);
	}
	else if (byte == 0b01111000)
	{
		data = decode_jump("js", data, assembly, labels);
	}
	else if (byte == 0b01110101)
	{
		data = decode_jump("jne", data, assembly, labels);
	}
	else if (byte == 0b01111101)
	{
		data = decode_jump("jnl", data, assembly, labels);
	}
	else if (byte == 0b01111111)
	{
		data = decode_jump("jg", data, assembly, labels);
	}
	else if (byte == 0b01110011)
	{
		data = decode_jump("jnb", data, assembly, labels);
	}
	else if (byte == 0b01110111)
	{
		data = decode_jump("ja", data, assembly, labels);
	}
	else if (byte == 0b01111011)
	{
		data = decode_jump("jnp", data, assembly, labels);
	}
	else if (byte == 0b01110001)
	{
		data = decode_jump("jno", data, assembly, labels);
	}
	else if (byte == 0b01111001)
	{
		data = decode_jump("jns", data, assembly, labels);
	}
	else if (byte == 0b11100010)
	{
		data = decode_jump("loop", data, assembly, labels);
	}
	else if (byte == 0b11100001)
	{
		data = decode_jump("loopz", data, assembly, labels);
	}
	else if (byte == 0b11100000)
	{
		data = decode_jump("loopnz", data, assembly, labels);
	}
	else if (byte == 0b11100011)
	{
		data = decode_jump("jcxz", data, assembly, labels);
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
		std::vector<Instruction> instructions;
		std::unordered_map<const u8*, std::string> labels;
		while (data_begin != data_end)
		{
			Instruction instruction;
			instruction.pointer = data_begin;
			data_begin = decode(data_begin, data_end, instruction.assembly, labels);
			instructions.push_back(std::move(instruction));
		}

		for (const Instruction& instruction : instructions)
		{
			const auto iter = labels.find(instruction.pointer);
			if (iter != labels.end())
			{
				std::cout << iter->second << ':' << std::endl;
			}
			std::cout << instruction.assembly << std::endl;
		}
	}
	else
	{
		std::cerr << "Usage: sim8086 <file>" << std::endl;
	}
	return 0;
}
