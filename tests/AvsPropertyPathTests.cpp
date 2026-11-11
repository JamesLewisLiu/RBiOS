#include "rb/AvsPropertyPath.hpp"
#include <iostream>
#include <stdexcept>
using namespace rb;
void require(bool ok, const char *s) { if (!ok) throw std::runtime_error(s); }
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--resolve") {
        Property root; root.name = "config"; root.value = {0};
        for (int i = 1; i <= 3; ++i) { Property p; p.name = i == 2 ? "other" : "item"; p.value = {uint8_t(i)}; root.children.push_back(p); }
        Property leaf; leaf.name = "leaf"; leaf.value = {4}; root.children[2].children.push_back(leaf);
        root.children[0].attributes["id"] = {'a'}; root.children[2].attributes["id"] = {'b'};
        std::string path;
        while (std::getline(std::cin, path)) {
            const auto r = referAvsProperty(root, path);
            if (!r.node) std::cout << "missing\n";
            else if (r.attribute) std::cout << "A:" << unsigned(r.attribute->at(0)) << '\n';
            else std::cout << "N:" << unsigned(r.node->value.at(0)) << '\n';
        }
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        std::string path;
        while (std::getline(std::cin, path)) {
            const auto q = parseAvsPropertyPath(path);
            std::cout << unsigned(q.error);
            for (const auto &s : q.steps) std::cout << " N:" << s.name << ':' << s.selector << ':' << s.attributeContext;
            if (q.hasAttribute && q.error == AvsPathError::none) std::cout << " A:" << q.attribute;
            std::cout << '\n';
        }
        return 0;
    }
    Property context; context.name = "config";
    Property first; first.name = "item"; first.attributes["id"] = {'a'};
    Property other; other.name = "other";
    Property last = first; last.attributes["id"] = {'b'};
    context.children = {first, other, last};
    require(referAvsProperty(context, "").node == &context, "empty path context");
    require(referAvsProperty(context, "///").node == &context, "slash context");
    require(referAvsProperty(context, "/item#0/").node == &context.children[0], "zero selector");
    require(referAvsProperty(context, "item#1").node == &context.children[2], "matching ordinal only");
    require(referAvsProperty(context, "item#-1").node == &context.children[2], "reverse ordinal");
    require(referAvsProperty(context, "item#-2").node == &context.children[0], "reverse second");
    require(!referAvsProperty(context, "item#2").node, "out of range missing");
    const auto attr = referAvsProperty(context, "id@item#-1");
    require(attr.attribute && *attr.attribute == Bytes{'b'}, "attribute prefix");
    require(parseAvsPropertyPath("__type@item").error == AvsPathError::reservedAttribute, "reserved attribute");
    require(referAvsProperty(context, "__type@missing").error == AvsPathError::none, "lookup failure precedes reserved attribute check");
    for (const auto *p : {"item#", "item#-", "item#08", "id@x@y"})
        require(parseAvsPropertyPath(p).error == AvsPathError::syntax, "syntax failure");
    for (const auto *p : {"..", "*", "item#9999999999999"})
        require(parseAvsPropertyPath(p).error == AvsPathError::unsupported, "bounded unresolved operation");
    std::cout << "PASS original AVS path grammar, forward/reverse matching ordinals and attribute references\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
