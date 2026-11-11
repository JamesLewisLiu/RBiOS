#include "rb/AvsXmlMetadata.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace rb;
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        std::string text;
        while (std::getline(std::cin, text)) {
            const auto count = avsXmlCount(text);
            std::cout << unsigned(avsXmlType(text)) << ' ' << count.status << ' ' << count.value << '\n';
        }
        return 0;
    }
    if (avsXmlType("boolean") || avsXmlType("BOOL") || avsXmlType(" bool") || avsXmlType("b") != 52 || avsXmlType("string") != 11)
        throw std::runtime_error("original XML type aliases");
    if (avsXmlCount("08").value != 8 || avsXmlCount("1A").value != 20 || avsXmlCount("2 trailing").value != 2 || avsXmlCount("+1").status != -1)
        throw std::runtime_error("original XML count quirks");
    if (avsXmlCount("0").status != 1 || avsXmlCount("4294967295").value != UINT32_MAX || avsXmlCount("4294967296").status != -1)
        throw std::runtime_error("original XML count bounds");
    std::cout << "PASS original XML type aliases and size/count text conversion\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
