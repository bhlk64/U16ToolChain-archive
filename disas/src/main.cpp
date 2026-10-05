#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include "lib.h"

#define separator "// ------------------------------------------------------------------ "

int main(int argc, char** argv) {
	if (argc != 3) {
		char* st = new char[0x400];
		std::ifstream fi {"help.txt"};
		while (true) {
			fi.getline(st, 0x400);
			if (fi.fail() || std::strcmp(st, "*") == 0) {
				fi.close();
				return 0;
			}
			std::cout << st << "\n";
		}
	}

	std::ifstream in {argv[1]}, ex {"example.cpp"};
	std::ofstream out {argv[2]};
	char* st = new char[0x400];
	while (true) {
		ex.getline(st, 0x400);
		out << st << "\n";
		if (std::strcmp(st, separator "1") == 0) {
			break;
		}
	}
	while (true) {
		in.getline(st, 0x400);
		if (std::strcmp(st, "*") == 0) {
			break;
		}
		out << st << "\n";
	}
	bool write = false;
	while (true) {
		ex.getline(st, 0x400);
		if (std::strcmp(st, separator "2") == 0) {
			write = true;
		}
		if (write) {
			out << st << "\n";
		}
		if (std::strcmp(st, separator "3") == 0) {
			break;
		}
	}

	// start main part.
	char open, close, space;
	int maxlen, unitlen, opcodelen = -1;
	in >> open >> close; // read exactly one char each time
	in >> maxlen >> unitlen;
	std::stringstream temp;

	uint8_t mask, val, chardata [3][0x100];
	// chardata: [0] : byte it is in (buf[k])
	// [1] : shift length
	// [2] : number length (binary)

	do {
		in.getline(st, 0x400);

		if (std::strcmp(st, "*") == 0) break;
		if (std::strcmp(st, "") == 0) continue;
		if (st[0] == ';') continue; // allow for comments
		out << "// " << st << "\n";
		if (st[0] == '#') {
			opcodelen = static_cast<int>(std::strtol(&st[1], nullptr, 0));
			continue;
		}
		// well, it is in usual format now.

		out << "if (buf.size() >= " << opcodelen << " && ";
		// conditions

		temp.clear();
		temp.str(st);
		for (int i = 0; i < 0x100; i++)
			chardata[0][i] = chardata[1][i] = chardata[2][i] = 0xFF;
		for (int i = 0; i < opcodelen; i++) {
			temp >> st; // must be 8 chars
			mask = val = 0;
			for (int j = 0; j < 8; j++) {
				unsigned char uc = static_cast<unsigned char>(st[j]);
				if (st[j] == '0' || st[j] == '1') {
					mask = mask << 1 | 1;
					val = val << 1 | (st[j] - '0');
				} else {
					if (chardata[0][uc] == 0xFF) {
						chardata[0][uc] = i;
						chardata[1][uc] = 7 - j;
						chardata[2][uc] = 1;
					} else {
						chardata[1][uc]--;
						chardata[2][uc]++;
					}
					mask <<= 1;
					val  <<= 1;
				}
			}
			if (i != 0) out << " && ";
			out << "(buf[" << i << "] & 0b" << tobin(mask, 8)
				<< ") == 0b" << tobin(val, 8);
		}
		temp >> space;
		if (space != ' ') temp.unget();
		temp.getline(st, 0x400);

		int read_idx = 0, write_idx = 0;
		while (st[read_idx] == ' ') read_idx++;
		bool in_space = false;
		for (; st[read_idx] != '\0'; read_idx++) {
			if (st[read_idx] == ' ') {
				if (!in_space) {
					st[write_idx++] = ' ';
					in_space = true;
				}
			} else {
				st[write_idx++] = st[read_idx];
				in_space = false;
			}
		}
		if (write_idx > 0 && st[write_idx - 1] == ' ') write_idx--;
		st[write_idx] = '\0';

		out << ") {\n";

		// implementation

		std::stringstream tempss {""};

		// First line start. (assignment)
		tempss << "	int ";
		bool notisfirst = false, declaredat = false; // declared anything
		for (int c_int = -128; c_int <= 127; c_int++) {
			char c = static_cast<char>(c_int);
			if (chardata[0][static_cast<unsigned char>(c)] != 0xFF) {
				declaredat = true;
				if (notisfirst) {
					tempss << ", ";
				} else {
					notisfirst = true;
				}
				tempss << c << " = buf[" << static_cast<int>(chardata[0][static_cast<unsigned char>(c)])
					<< "] >> " << static_cast<int>(chardata[1][static_cast<unsigned char>(c)]) << " & 0b";
				for (int j = 0; j < chardata[2][static_cast<unsigned char>(c)]; j++)
					tempss << "1";
			}
		}
		if (declaredat) {
			out << tempss.str() << ";\n";
		}

		out << "	std::stringstream cmd_ss;\n";
		out << "	cmd_ss << \"";
		for (int i = 0; st[i] != 0; i++) { // st is C-style
			if (st[i] == open) out << "\" << (";
			else if (st[i] == close) out << ") << \"";
			else out << st[i];
		}
		out << "\";\n";
		out << "	std::string cmd = cmd_ss.str();\n";
		out << "	for (char& c : cmd) c = std::toupper((unsigned char)c);\n";
		out << "	size_t pos = cmd.find(\"<UNRECOGNIZED>\");\n";
		out << "	if (pos != std::string::npos) cmd.replace(pos, 14, \"<unrecognized>\");\n";
		out << "	while (cmd.length() < 30) cmd += ' ';\n";
		out << "	out << \"\\t\" << cmd << \"; \" << tohex_trim(ip >> 16) << \":\" << tohex(ip & 0xFFFF, 4) << \"H | \";\n";
		out << "	for (int i = 0; i < " << opcodelen << "; i += 2) {\n";
		out << "		if (i + 1 < " << opcodelen << ") out << tohex(buf[i+1], 2) << tohex(buf[i], 2);\n";
		out << "		else out << tohex(buf[i], 2);\n";
		out << "	}\n";
		out << "	out << \"\\n\";\n";

		// Fifth line (repeat several times)
		for (int i = 0; i < opcodelen; i++) out << " buf.pop_front();";

		// Sixth line
		out << "\n	goto done;\n}\n";
	} while (true);

	// "Unrecognized command"
		// First line start.
		out << ";\n	{\n	int pop_len = std::min((int)buf.size(), " << unitlen << ");\n";
		out << "	std::string cmd = \"<unrecognized>\";\n";
		out << "	while (cmd.length() < 30) cmd += ' ';\n";
		out << "	out << \"\\t\" << cmd << \"; \" << tohex_trim(ip >> 16) << \":\" << tohex(ip & 0xFFFF, 4) << \"H | \";\n";
		out << "	for (int i = 0; i < pop_len; i += 2) {\n";
		out << "		if (i + 1 < pop_len) out << tohex(buf[i+1], 2) << tohex(buf[i], 2);\n";
		out << "		else out << tohex(buf[i], 2);\n";
		out << "	}\n";
		out << "	out << \"\\n\";\n";
		out << "	for (int i = 0; i < pop_len; i++) buf.pop_front();\n	}\n";

	// done. finalize it.

	write = false;
	do {
		if (!ex.getline(st, 0x400)) {
			in.close();
			out.close();
			ex.close();
			return 0;
		}
		if (std::strcmp(st, separator "4") == 0) {
			write = true;
		}
		if (write) out << st << "\n";
	} while (true);
}
