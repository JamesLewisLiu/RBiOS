#include "rb/AFP.hpp"
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc == 4) {
            AfpPackage package(readFile(argv[2]));
            const std::string op = argv[1];
            Bytes bytes;
            if (op == "--bitmap")
                bytes = package.bitmap(argv[3]).bgra;
            else if (op == "--movie")
                bytes = package.movieBytes(argv[3]);
            else if (op == "--bsi")
                bytes = package.bitStreamBytes(argv[3]);
            else
                throw Error("unknown operation");
#ifdef _WIN32
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            std::cout.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            return 0;
        }
        size_t checks = 0;
        auto expect = [&](bool value) {
            if (!value)
                throw Error("AFP check failed");
            ++checks;
        };
        bool rejected = false;
        try {
            AfpPackage bad(Bytes{});
        } catch (const Error &) {
            rejected = true;
        }
        expect(rejected);
        if (argc == 2) {
            const auto root = std::filesystem::path(argv[1]);
            size_t packages = 0, movies = 0, shapes = 0, bitmaps = 0;
            for (const auto &entry : std::filesystem::recursive_directory_iterator(root / "data")) {
                if (entry.path().extension() != ".ifs")
                    continue;
                const auto bytes = readFile(entry.path().string());
                IfsArchive archive(bytes);
                bool hasMagic = false;
                for (const auto &e : archive.entries())
                    if (e.path == "magic")
                        hasMagic = true;
                if (!hasMagic)
                    continue;
                AfpPackage p(bytes);
                ++packages;
                for (const auto &m : p.movies()) {
                    auto b = p.movieBytes(m.name);
                    expect(b.size() >= 8);
                    Reader r(b);
                    expect(r.be32() == 0xc1d0b208);
                    expect(r.be32() == b.size());
                    expect(!p.bitStreamBytes(m.name).empty());
                    ++movies;
                    for (auto id : m.shapes) {
                        expect(!p.geometryBytes(m.name, id).empty());
                        ++shapes;
                    }
                }
                // Exercise every UV binding without writing decoded assets.
                for (const auto &t : p.textures()) {
                    const auto b = p.bitmap(t.nameCP932);
                    const auto sheet = p.textureSheetSize(t.nameCP932);
                    expect(t.uvRect[0] >= 0 && t.uvRect[2] >= 0 && t.uvRect[1] <= sheet[0] &&
                           t.uvRect[3] <= sheet[1]);
                    expect(b.width > 0 && b.height > 0);
                    expect(b.bgra.size() == size_t(b.width) * b.height * 4);
                    ++bitmaps;
                }
                bool bitmapMissing = false, movieMissing = false;
                try {
                    p.texture("__absent__");
                } catch (const Error &) {
                    bitmapMissing = true;
                }
                try {
                    p.movie("__absent__");
                } catch (const Error &) {
                    movieMissing = true;
                }
                expect(bitmapMissing);
                expect(movieMissing);
            }
            expect(packages > 0);
            std::cout << packages << " packages, " << movies << " movies, " << shapes << " shapes, "
                      << bitmaps << " bitmaps; ";
        }
        std::cout << "PASS AFP " << checks << " checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
