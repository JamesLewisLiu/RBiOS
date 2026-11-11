import XCTest
import UIKit
@testable import RBiOS

final class OriginalStartupTests: XCTestCase {
    func testOriginalXmlFeedsBootImage() throws {
        let xml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <config><desc><nr_desc __type="u16">2048</nr_desc></desc><fs><root><device __type="str">.</device><option __type="str">posix=0</option></root></fs></config>
        """
        let image = try RBOriginalStartup.configurationImage(xml: Data(xml.utf8))
        XCTAssertEqual(image.count, 2528)
        XCTAssertEqual(image[68], 0); XCTAssertEqual(image[69], 8)
        XCTAssertEqual(image[155], 46)
        XCTAssertEqual(image[418], 48)
    }
    func testShiftJisRoundTripPreservesOriginalBytes() throws {
        var input = Data("<?xml version=\"1.0\" encoding=\"Shift-JIS\"?><config><fs><root><device __type=\"str\">".utf8)
        input.append(contentsOf: [0x93, 0xfa, 0x96, 0x7b, 0x8c, 0xea])
        input.append(Data("</device></root></fs></config>".utf8))
        let image = try RBOriginalStartup.configurationImage(xml: input)
        XCTAssertEqual(Array(image[155..<162]), [0x93, 0xfa, 0x96, 0x7b, 0x8c, 0xea, 0])
    }
    func testMalformedAndUnrecoveredXmlFailWithoutBootImage() {
        for xml in ["<config>", "<other/>", "<config><x __type=\"double\">1</x></config>",
                    "<!DOCTYPE config [<!ENTITY x SYSTEM 'http://127.0.0.1:1/'>]><config>&x;</config>"] {
            XCTAssertThrowsError(try RBOriginalStartup.configurationImage(xml: Data(xml.utf8)))
        }
    }
    func testEntityAndCDATAFeedOriginalStringBytes() throws {
        let xml = "<config><fs><root><device __type=\"str\">a&amp;b<![CDATA[<c>]]></device></root></fs></config>"
        let image = try RBOriginalStartup.configurationImage(xml: Data(xml.utf8))
        XCTAssertEqual(Array(image[155..<162]), [97, 38, 98, 60, 99, 62, 0])
        var cp932 = Data("<?xml version=\"1.0\" encoding=\"Shift-JIS\"?><config><fs><root><device __type=\"str\"><![CDATA[".utf8)
        cp932.append(contentsOf: [0x93, 0xfa, 0x96, 0x7b, 0x8c, 0xea])
        cp932.append(Data("]]></device></root></fs></config>".utf8))
        let cdataImage = try RBOriginalStartup.configurationImage(xml: cp932)
        XCTAssertEqual(Array(cdataImage[155..<162]), [0x93, 0xfa, 0x96, 0x7b, 0x8c, 0xea, 0])
    }
    func testMissingOriginalDllDoesNotEnterHarness() {
        XCTAssertThrowsError(try RBOriginalStartup().start(at: URL(fileURLWithPath: "/rbios-no-fixture")))
    }
    func testLongShiftJisDeclarationDoesNotFallBackToUtf8() throws {
        var input = Data(("<?xml version=\"1.0\"" + String(repeating: " ", count: 1100) + "encoding=\"Shift-JIS\"?>\r\n<config><fs><root><device __type=\"str\">").utf8)
        input.append(contentsOf: [0x93, 0xfa, 0x96, 0x7b, 0x8c, 0xea])
        input.append(Data("</device></root></fs></config>\r\n".utf8))
        let image = try RBOriginalStartup.configurationImage(xml: input)
        XCTAssertEqual(Array(image[155..<162]), [0x93, 0xfa, 0x96, 0x7b, 0x8c, 0xea, 0])
    }
    func testOriginalLiteralLineEndingsSurviveXmlTokenization() throws {
        for content in ["a\r\nb\rc", "<![CDATA[a\r\nb\rc]]>"] {
            let xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<config>\r\n<fs><root><device __type=\"str\">\(content)</device></root></fs>\r\n</config>\r\n"
            let image = try RBOriginalStartup.configurationImage(xml: Data(xml.utf8))
            XCTAssertEqual(Array(image[155..<163]), [97, 13, 10, 98, 13, 99, 0, 0])
        }
    }
    func testBomHandlingAndUnsupportedDeclarations() throws {
        var utf8 = Data([0xef, 0xbb, 0xbf])
        utf8.append(Data("<?xml version=\"1.0\" encoding=\"UTF-8\"?><config/>".utf8))
        XCTAssertEqual(try RBOriginalStartup.configurationImage(xml: utf8).count, 2528)
        var conflict = Data([0xef, 0xbb, 0xbf])
        conflict.append(Data("<?xml version=\"1.0\" encoding=\"Shift-JIS\"?><config/>".utf8))
        XCTAssertThrowsError(try RBOriginalStartup.configurationImage(xml: conflict))
        let utf16 = try XCTUnwrap("<?xml version=\"1.0\" encoding=\"UTF-16\"?><config/>".data(using: .utf16))
        XCTAssertThrowsError(try RBOriginalStartup.configurationImage(xml: utf16))
        XCTAssertThrowsError(try RBOriginalStartup.configurationImage(xml: Data("<!DOCTYPE config><config/>".utf8)))
    }
    func testOriginalAttributeWhitespaceSurvivesXmlTokenization() throws {
        let literal = "<config><v id=\"a\tb\nc\rd\"/></config>"
        let references = "<config><v id=\"a&#9;b&#10;c&#13;d\"/></config>"
        let bytes = try RBOriginalStartup.propertyDocumentData(xml: Data(literal.utf8))
        XCTAssertEqual(bytes, try RBOriginalStartup.propertyDocumentData(xml: Data(references.utf8)))
        XCTAssertNotNil(bytes.range(of: Data([97, 9, 98, 10, 99, 13, 100, 0])))
    }
    func testCp932CdataDelimiterOverlapIsRejected() {
        var xml = Data("<?xml version=\"1.0\" encoding=\"Shift-JIS\"?><config><fs><root><device __type=\"str\"><![CDATA[".utf8)
        // 0x815d is a CP932 character whose trail byte equals ']'.
        xml.append(contentsOf: [0x81, 0x5d, 0x5d, 0x3e, 13])
        xml.append(Data("]]></device></root></fs></config>".utf8))
        // The original reader rejects this ambiguous byte sequence too.
        XCTAssertThrowsError(try RBOriginalStartup.configurationImage(xml: xml))
    }
    func testDefaultApplicationRootIsOriginalStartup() throws {
        let root = try XCTUnwrap((UIApplication.shared.delegate as? AppDelegate)?.window?.rootViewController as? UINavigationController)
        XCTAssertTrue(root.viewControllers.first is OriginalBootViewController)
    }
}
