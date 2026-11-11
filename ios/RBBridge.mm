#import "RBBridge.h"
#include "rb/Assets.hpp"
#include "rb/BuildIdentity.hpp"
#include "rb/Chart.hpp"
#include "rb/Viewport.hpp"
#import <CommonCrypto/CommonDigest.h>
#include <memory>

static void failure(NSError **error, const std::exception &e) {
    if (error)
        *error = [NSError errorWithDomain:@"RBiOS"
                                     code:1
                                 userInfo:@{
                                     NSLocalizedDescriptionKey : @(e.what())
                                 }];
}
static NSString *cp932(const std::string &s) {
    return
        [[NSString alloc]
            initWithBytes:s.data()
                   length:s.size()
                 encoding:CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingDOSJapanese)]
            ?: @"(Unsupported text encoding)";
}
static std::string path(NSURL *u) {
    return std::string(u.fileSystemRepresentation);
}
@implementation RBBridge
+ (NSArray<NSDictionary<NSString *, id> *> *)catalogAt:(NSURL *)root error:(NSError **)error {
    try {
        const auto bytes =
            rb::readFile(path([root URLByAppendingPathComponent:@"modules/reflecbeat.dll"]));
        if (bytes.size() > UINT32_MAX)
            throw rb::Error("DLL exceeds SHA-256 input limit");
        unsigned char digest[CC_SHA256_DIGEST_LENGTH];
        CC_SHA256(bytes.data(), CC_LONG(bytes.size()), digest);
        constexpr char hex[] = "0123456789abcdef";
        std::string hash;
        for (auto value : digest) {
            hash.push_back(hex[value >> 4]);
            hash.push_back(hex[value & 15]);
        }
        if (hash != rb::sourceDLLSHA256)
            throw rb::Error("reflecbeat.dll version does not match MBR:J:A:A:2019121100");
        auto songs = rb::readCatalog(bytes);
        NSMutableArray *out = [NSMutableArray array];
        for (const auto &s : songs) {
            if (s.chartPrefix.empty())
                continue;
            NSMutableArray *available = [NSMutableArray array];
            for (unsigned d = 0; d < s.levels.size(); ++d) {
                if (s.levels[d] == 0 || s.levels[d] >= 128)
                    continue;
                NSURL *url = [root URLByAppendingPathComponent:@(s.chartPath(d).c_str())];
                if ([[NSFileManager defaultManager] fileExistsAtPath:url.path])
                    [available addObject:@(d)];
            }
            if (available.count == 0)
                continue;
            NSMutableArray *levels = [NSMutableArray array];
            for (auto l : s.levels)
                [levels addObject:@(l)];
            [out addObject:@{
                @"index" : @(s.index),
                @"title" : cp932(s.titleCP932),
                @"artist" : cp932(s.artistCP932),
                @"prefix" : @(s.chartPrefix.c_str()),
                @"audio" : @(s.audioPrefix.c_str()),
                @"levels" : levels,
                @"difficulties" : available
            }];
        }
        return out;
    } catch (const std::exception &e) {
        failure(error, e);
        return nil;
    }
}
+ (NSDictionary<NSString *, id> *)chartAt:(NSURL *)url error:(NSError **)error {
    try {
        auto c = rb::Chart::parse(rb::readFile(path(url)));
        size_t added = 0;
        for (size_t i = 0; i < c.objects.size(); ++i)
            added += c.additionalTicks(i);
        return @{
            @"version" : @(c.version),
            @"bpm" : @(c.bpm),
            @"duration" : @(c.durationMs),
            @"objects" : @(c.objects.size()),
            @"additionalTicks" : @(added)
        };
    } catch (const std::exception &e) {
        failure(error, e);
        return nil;
    }
}
+ (UIImage *)firstTextureAt:(NSURL *)url error:(NSError **)error {
    try {
        rb::IfsArchive a(rb::readFile(path(url)));
        auto textures = rb::textureList(a);
        if (textures.empty())
            throw rb::Error("IFS has no supported texture");
        const auto &t = textures.front();
        const auto &r = t.imageRect;
        const auto &u = t.uvRect;
        auto w = r[1] - r[0], h = r[3] - r[2];
        auto bgra = rb::texturePixels(a, t);
        NSData *data = [NSData dataWithBytes:bgra.data() length:bgra.size()];
        CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGImageRef raw = CGImageCreate(
            w, h, 8, 32, w * 4, cs, CGBitmapInfo(kCGBitmapByteOrder32Little | kCGImageAlphaFirst),
            provider, nullptr, false, kCGRenderingIntentDefault);
        CGColorSpaceRelease(cs);
        CGDataProviderRelease(provider);
        if (!raw)
            throw rb::Error("CGImage creation failed");
        CGRect crop = CGRectMake(u[0] - r[0], u[2] - r[2], u[1] - u[0], u[3] - u[2]);
        CGImageRef cropped = CGImageCreateWithImageInRect(raw, crop);
        CGImageRelease(raw);
        if (!cropped)
            throw rb::Error("UV rectangle outside image");
        UIImage *result = [UIImage imageWithCGImage:cropped];
        CGImageRelease(cropped);
        return result;
    } catch (const std::exception &e) {
        failure(error, e);
        return nil;
    }
}
+ (CGRect)displayRectIn:(CGRect)safe fraction:(double)f {
    try {
        auto v = rb::ViewportTransform::fit(
            {safe.origin.x, safe.origin.y, safe.size.width, safe.size.height}, f);
        return CGRectMake(v.display.x, v.display.y, v.display.width, v.display.height);
    } catch (...) {
        return CGRectZero;
    }
}
+ (NSValue *)gamePoint:(CGPoint)p display:(CGRect)d {
    rb::ViewportTransform t{{d.origin.x, d.origin.y, d.size.width, d.size.height},
                            d.size.width / 768.0};
    if (t.scale <= 0)
        return nil;
    auto q = t.toGame({p.x, p.y});
    return q ? [NSValue valueWithCGPoint:CGPointMake(q->x, q->y)] : nil;
}
+ (NSString *)migrationStatus {
    return @"Component validation harness. Original AVS2/EA3 boot, game loading and TEST dispatch are not restored. "
           @"This native page flow is not the original game runtime.";
}
@end
@implementation RBAudioDecoder {
    std::unique_ptr<rb::Wave> _wave;
}
- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error {
    self = [super init];
    if (self) {
        try {
            _wave = std::make_unique<rb::Wave>(rb::Wave::parse(rb::readFile(path(url))));
        } catch (const std::exception &e) {
            failure(error, e);
            return nil;
        }
    }
    return self;
}
- (NSUInteger)blockCount {
    return _wave->blockCount();
}
- (double)sampleRate {
    return _wave->sampleRate;
}
- (uint64_t)frames {
    return _wave->frames;
}
- (NSData *)decodeBlock:(NSUInteger)i error:(NSError **)error {
    try {
        auto b = _wave->decodeBlock(i);
        return [NSData dataWithBytes:b.data() length:b.size() * sizeof(int16_t)];
    } catch (const std::exception &e) {
        failure(error, e);
        return nil;
    }
}
@end
