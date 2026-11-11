#import "RBBridge.h"
#import <CommonCrypto/CommonDigest.h>
#include "rb/AvsXmlProperty.hpp"
#include "rb/AvsConfigImport.hpp"
#include "rb/AvsDefaultFile.hpp"
#include "rb/AvsDescriptors.hpp"
#include "rb/BuildIdentity.hpp"
#include <memory>
#include <string_view>

namespace {
rb::Bytes encoded(NSString *text, NSStringEncoding encoding) {
    NSData *data = [text dataUsingEncoding:encoding allowLossyConversion:NO];
    if (!data) throw rb::Error("XML text cannot be represented in its original encoding");
    const auto *p = static_cast<const uint8_t *>(data.bytes);
    return data.length ? rb::Bytes(p, p + data.length) : rb::Bytes{};
}
rb::Bytes readHostFile(NSURL *url) {
    rb::AvsDefaultFile file;
    const auto status = file.open(url.fileSystemRepresentation, 1, 0444, false);
    if (status < 0) throw rb::Error("Cannot open original file: " + std::string(url.fileSystemRepresentation));
    rb::Bytes bytes; uint8_t block[65536];
    while (true) {
        const auto count = file.read(block, sizeof(block));
        if (count < 0) throw rb::Error("Cannot read original file");
        if (!count) break;
        bytes.insert(bytes.end(), block, block + count);
    }
    return bytes;
}
void reportError(NSError **error, const std::exception &e) {
    if (error) *error = [NSError errorWithDomain:@"RBiOS.OriginalStartup" code:1
        userInfo:@{NSLocalizedDescriptionKey: @(e.what())}];
}
}

@interface RBOriginalXmlDelegate : NSObject <NSXMLParserDelegate> {
@public
    std::unique_ptr<rb::AvsXmlPropertyBuilder> builder;
    NSStringEncoding encoding;
    std::string failure;
}
@end
@implementation RBOriginalXmlDelegate
- (void)parser:(NSXMLParser *)parser didStartElement:(NSString *)name namespaceURI:(NSString *)uri qualifiedName:(NSString *)qualified attributes:(NSDictionary<NSString *, NSString *> *)attributes {
    try {
        std::vector<std::pair<std::string, rb::Bytes>> values;
        for (NSString *key in attributes) values.emplace_back(key.UTF8String, encoded(attributes[key], encoding));
        builder->begin(name.UTF8String, values);
    } catch (const std::exception &e) { failure = e.what(); [parser abortParsing]; }
}
- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)text {
    try { builder->text(encoded(text, encoding)); }
    catch (const std::exception &e) { failure = e.what(); [parser abortParsing]; }
}
- (void)parser:(NSXMLParser *)parser foundCDATA:(NSData *)data {
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (!text) { failure = "Invalid XML CDATA encoding"; [parser abortParsing]; return; }
    [self parser:parser foundCharacters:text];
}
- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)name namespaceURI:(NSString *)uri qualifiedName:(NSString *)qualified {
    try { builder->end(name.UTF8String); }
    catch (const std::exception &e) { failure = e.what(); [parser abortParsing]; }
}
- (void)parser:(NSXMLParser *)parser foundInternalEntityDeclarationWithName:(NSString *)name value:(NSString *)value {
    failure = "Custom XML entities are not recovered"; [parser abortParsing];
}
- (void)parser:(NSXMLParser *)parser foundExternalEntityDeclarationWithName:(NSString *)name publicID:(NSString *)publicID systemID:(NSString *)systemID {
    failure = "External XML entities are not supported"; [parser abortParsing];
}
@end

namespace {
bool xmlSpace(unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
NSString *declaredXmlEncoding(NSData *data, bool &utf8BOM) {
    const std::string_view bytes(static_cast<const char *>(data.bytes), data.length);
    if (bytes.find('\0') != std::string_view::npos || bytes.substr(0, 2) == "\xff\xfe" || bytes.substr(0, 2) == "\xfe\xff")
        throw rb::Error("Unrecovered XML encoding");
    utf8BOM = bytes.substr(0, 3) == "\xef\xbb\xbf";
    size_t begin = utf8BOM ? 3 : 0;
    while (begin < bytes.size() && xmlSpace(uint8_t(bytes[begin]))) ++begin;
    if (bytes.substr(begin, 5) != "<?xml" || begin + 5 >= bytes.size() || !xmlSpace(uint8_t(bytes[begin + 5]))) return @"utf-8";
    const auto end = bytes.find("?>", begin + 5);
    if (end == std::string_view::npos) throw rb::Error("Incomplete XML declaration");
    NSString *declaration = [[NSString alloc] initWithBytes:bytes.data() + begin length:end + 2 - begin encoding:NSISOLatin1StringEncoding];
    NSRegularExpression *expression = [NSRegularExpression regularExpressionWithPattern:
        @"\\sencoding\\s*=\\s*['\"]([^'\"]+)['\"]" options:0 error:nil];
    NSTextCheckingResult *match = [expression firstMatchInString:declaration options:0 range:NSMakeRange(0, declaration.length)];
    return match ? [[declaration substringWithRange:[match rangeAtIndex:1]] lowercaseString] : @"utf-8";
}
// The original AVS reader preserves literal CR/CRLF in text and whitespace in
// attributes. XML 1.0 tokenizers normalize those bytes. Protect them with numeric
// references in this in-memory tokenization input; original files stay untouched.
NSData *xmlTokenizationInput(NSData *data, bool cp932) {
    const std::string_view bytes(static_cast<const char *>(data.bytes), data.length);
    std::string output; output.reserve(bytes.size());
    size_t pos = 0; size_t depth = 0;
    auto starts = [&](std::string_view prefix) { return bytes.substr(pos, prefix.size()) == prefix; };
    auto copyThrough = [&](std::string_view ending, size_t offset) {
        const auto end = bytes.find(ending, pos + offset);
        if (end == std::string_view::npos) throw rb::Error("Incomplete XML markup");
        output.append(bytes.substr(pos, end + ending.size() - pos)); pos = end + ending.size();
    };
    while (pos < bytes.size()) {
        if (starts("<!--")) { copyThrough("-->", 4); continue; }
        if (starts("<?")) { copyThrough("?>", 2); continue; }
        if (starts("<!DOCTYPE")) throw rb::Error("XML document type declarations are not recovered");
        if (starts("<![CDATA[")) {
            size_t end = pos + 9;
            // ']' can be a CP932 trail byte. Match the CDATA terminator only
            // at character boundaries, never inside a multibyte character.
            while (end < bytes.size() && bytes.substr(end, 3) != "]]>") {
                const auto c = uint8_t(bytes[end]);
                const bool lead = cp932 && ((c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc));
                if (lead && bytes.substr(end + 1, 3) == "]]>")
                    throw rb::Error("CP932 CDATA delimiter overlap is not recovered");
                end += lead ? 2 : 1;
            }
            if (end >= bytes.size()) throw rb::Error("Incomplete XML CDATA");
            output += "<![CDATA["; pos += 9;
            while (pos < end) {
                if (bytes[pos] == '\r') output += "]]>&#13;<![CDATA[";
                else output += bytes[pos];
                ++pos;
            }
            output += "]]>"; pos += 3; continue;
        }
        if (bytes[pos] == '<') {
            const bool closing = starts("</");
            const bool declaration = starts("<!");
            char quote = 0;
            do {
                const char c = bytes[pos++];
                if (quote && (c == '\t' || c == '\n' || c == '\r')) {
                    output += c == '\t' ? "&#9;" : c == '\n' ? "&#10;" : "&#13;";
                } else {
                    output += c;
                    if (quote) { if (c == quote) quote = 0; }
                    else if (c == '\'' || c == '"') quote = c;
                    else if (c == '>') break;
                }
            } while (pos < bytes.size());
            if (closing) { if (depth) --depth; }
            else if (!declaration && pos >= 2 && bytes[pos - 2] != '/') ++depth;
            continue;
        }
        if (depth && bytes[pos] == '\r') output += "&#13;";
        else output += bytes[pos];
        ++pos;
    }
    return [NSData dataWithBytes:output.data() length:output.size()];
}
rb::PropertyDocument decodeOriginalXml(NSData *data) {
    // Foundation supplies XML tokenization only. Typed values and node behavior
    // are restored by AvsXmlPropertyBuilder, not inferred from Foundation types.
    bool utf8BOM = false;
    NSString *name = declaredXmlEncoding(data, utf8BOM);
    NSStringEncoding encoding; uint8_t propertyEncoding;
    if ([name isEqualToString:@"shift-jis"] || [name isEqualToString:@"shift_jis"] || [name isEqualToString:@"cp932"]) {
        if (utf8BOM) throw rb::Error("XML byte order mark conflicts with its encoding");
        encoding = CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingDOSJapanese); propertyEncoding = 0x80;
    } else if ([name isEqualToString:@"utf-8"] || [name isEqualToString:@"utf8"]) {
        encoding = NSUTF8StringEncoding; propertyEncoding = 0xA0;
    } else throw rb::Error("Unrecovered XML encoding");
    RBOriginalXmlDelegate *delegate = [RBOriginalXmlDelegate new];
    delegate->encoding = encoding;
    // The launcher uses property_create(23): the duplicate-node flag is set.
    delegate->builder = std::make_unique<rb::AvsXmlPropertyBuilder>(propertyEncoding, true);
    NSXMLParser *parser = [[NSXMLParser alloc] initWithData:xmlTokenizationInput(data, propertyEncoding == 0x80)];
    parser.delegate = delegate; parser.shouldResolveExternalEntities = NO;
    parser.externalEntityResolvingPolicy = NSXMLParserResolveExternalEntitiesNever;
    if (![parser parse]) throw rb::Error(delegate->failure.empty() ?
        std::string(parser.parserError.localizedDescription.UTF8String ?: "Malformed XML") : delegate->failure);
    return delegate->builder->finish();
}
}

@implementation RBOriginalStartup {
    std::unique_ptr<rb::AvsDescriptors> _descriptors;
    rb::AvsBootConfigImage _config;
    rb::Bytes _standardHeap, _avsHeap;
}
+ (NSData *)propertyDocumentDataFromXML:(NSData *)xml error:(NSError **)error {
    try {
        const auto bytes = decodeOriginalXml(xml).encode();
        return [NSData dataWithBytes:bytes.data() length:bytes.size()];
    } catch (const std::exception &e) { reportError(error, e); return nil; }
}
+ (NSData *)configurationImageFromXML:(NSData *)xml error:(NSError **)error {
    try {
        const auto document = decodeOriginalXml(xml);
        const auto image = rb::importAvsBootConfig(&document.root);
        return [NSData dataWithBytes:image.data() length:image.size()];
    } catch (const std::exception &e) { reportError(error, e); return nil; }
}
- (NSDictionary<NSString *, id> *)startAtRoot:(NSURL *)root error:(NSError **)error {
    _descriptors.reset(); _standardHeap.clear(); _avsHeap.clear();
    try {
        const auto dll = readHostFile([root URLByAppendingPathComponent:@"modules/reflecbeat.dll"]);
        unsigned char digest[CC_SHA256_DIGEST_LENGTH]; CC_SHA256(dll.data(), CC_LONG(dll.size()), digest);
        constexpr char digits[] = "0123456789abcdef"; std::string hash;
        for (auto byte : digest) { hash += digits[byte >> 4]; hash += digits[byte & 15]; }
        if (hash != rb::sourceDLLSHA256) throw rb::Error("Original DLL version does not match recovered code");
        const auto bytes = readHostFile([root URLByAppendingPathComponent:@"prop/avs-config.xml"]);
        const auto document = decodeOriginalXml([NSData dataWithBytes:bytes.data() length:bytes.size()]);
        // Original launcher allocates two 0x1000000 heaps for this game.
        _standardHeap.resize(0x1000000); _avsHeap.resize(0x1000000);
        _config = rb::importAvsBootConfig(&document.root);
        const auto fixes = rb::applyAvsProductMode(_config, _config[0] != 0);
        const auto count = uint16_t(_config[68]) | uint16_t(_config[69]) << 8;
        if (((24u * (uint32_t(count) + 4) + 15u) & ~15u) > _avsHeap.size())
            throw rb::Error("AVS descriptor arena exceeds supplied heap");
        _descriptors = std::make_unique<rb::AvsDescriptors>(count);
        // 10013BC0 calls the complete thread bootstrap here, before starting
        // descriptors or mounting files. Never jump over missing services.
        return @{ @"bootComplete": @NO, @"blockedAddress": @"libavs-win32.dll:0x10029410",
            @"message": @"Thread bootstrap is not fully recovered. EA3 and game entry were not executed.",
            @"trace": @[@"Original DLL identity verified", @"Original AVS XML loaded",
                @"AVS heap buffers allocated", @"AVS configuration imported",
                [NSString stringWithFormat:@"Product-mode corrections: %lu", (unsigned long)fixes.size()],
                [NSString stringWithFormat:@"Descriptor pool allocated: %u", unsigned(count)]] };
    } catch (const std::exception &e) {
        _descriptors.reset(); rb::Bytes{}.swap(_standardHeap); rb::Bytes{}.swap(_avsHeap);
        reportError(error, e); return nil;
    }
}
@end
