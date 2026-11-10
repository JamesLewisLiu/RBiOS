#import "RBBridge.h"
#include "rb/MenuSelection.hpp"
#include "rb/MenuPresentation.hpp"
#include <algorithm>
#include <memory>

namespace {
void menuError(NSError **error, const std::exception &e) {
    if (error) *error = [NSError errorWithDomain:@"RBiOS.Menu" code:1
        userInfo:@{NSLocalizedDescriptionKey: @(e.what())}];
}
UIImage *menuImage(const rb::AfpBitmap &bitmap) {
    NSData *data = [NSData dataWithBytes:bitmap.bgra.data() length:bitmap.bgra.size()];
    auto provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
    auto color = CGColorSpaceCreateDeviceRGB();
    auto image = CGImageCreate(bitmap.width, bitmap.height, 8, 32, bitmap.width * 4, color,
        CGBitmapInfo(kCGBitmapByteOrder32Little | kCGImageAlphaFirst), provider, nullptr, false,
        kCGRenderingIntentDefault);
    CGColorSpaceRelease(color); CGDataProviderRelease(provider);
    if (!image) throw rb::Error("original menu image creation failed");
    UIImage *result = [UIImage imageWithCGImage:image]; CGImageRelease(image); return result;
}
}
@implementation RBDifficultyMenu {
    rb::DifficultyMenu _menu;
    int32_t _lastTick;
}
- (instancetype)initWithAvailable:(NSArray<NSNumber *> *)available initial:(NSInteger)initial error:(NSError **)error {
    self = [super init]; if (!self) return nil;
    try {
        _menu.enabled.fill(false); _lastTick = -1;
        for (NSNumber *value in available) {
            const auto index = value.integerValue;
            if (index < 0 || index >= 4) throw rb::Error("invalid available chart difficulty");
            _menu.enabled[size_t(index)] = true;
        }
        if (initial < 0 || initial >= 4 || !_menu.enabled[size_t(initial)])
            throw rb::Error("initial chart difficulty is unavailable");
        _menu.selected = int32_t(initial);
    } catch (const std::exception &e) { menuError(error, e); return nil; }
    return self;
}
+ (NSDictionary<NSString *, NSURL *> *)soundResourcesAtRoot:(NSURL *)root {
    NSMutableDictionary *resources = [NSMutableDictionary dictionary];
    for (int32_t difficulty = 0; difficulty < 4; ++difficulty) {
        const auto &cue = rb::difficultyMenuSound(difficulty);
        resources[@(cue.key)] = [root URLByAppendingPathComponent:@(cue.path)];
    }
    return resources;
}
- (NSInteger)selected { return _menu.selected; }
- (NSDictionary<NSString *, id> *)updateAtTick:(int32_t)tick action:(NSString *)action error:(NSError **)error {
    try {
        if (tick < 0 || tick < _lastTick) throw rb::Error("menu clock moved backwards");
        rb::DifficultyMenuInput input;
        if ([action isEqualToString:@"basic"]) input.panel = 0;
        else if ([action isEqualToString:@"medium"]) input.panel = 1;
        else if ([action isEqualToString:@"upper"]) input.panel = 2;
        else if ([action isEqualToString:@"next"]) input.next = true;
        else if ([action isEqualToString:@"confirm"]) input.confirm = true;
        else if ([action isEqualToString:@"detail"]) input.detail = true;
        else if (![action isEqualToString:@"idle"]) throw rb::Error("unknown menu action");
        // Empty original ticks only drain this five-tick cooldown. No large catch-up loop.
        const int64_t empty = std::max<int64_t>(0, int64_t(tick) - _lastTick - 1);
        for (int64_t i = 0; i < std::min<int64_t>(empty, 5); ++i) _menu.update({});
        const auto result = _menu.update(input); _lastTick = tick;
        return @{@"difficulty": @(result.difficulty), @"changed": @(result.changed),
            @"sound": result.changed ? @(rb::difficultyMenuSound(result.difficulty).key) : @"",
            @"confirmed": @(result.confirmed), @"showDetail": @(_menu.showDetail)};
    } catch (const std::exception &e) { menuError(error, e); return nil; }
}
@end

@implementation RBMenuScene {
    std::unique_ptr<rb::MenuPresentation> _presentation;
    NSMutableDictionary<NSString *, UIImage *> *_images;
}
- (instancetype)initWithRoot:(NSURL *)root page:(NSString *)page error:(NSError **)error {
    self = [super init]; if (!self) return nil;
    try {
        rb::MenuPage kind;
        if ([page isEqualToString:@"title"]) kind = rb::MenuPage::Title;
        else if ([page isEqualToString:@"music"]) kind = rb::MenuPage::Music;
        else if ([page isEqualToString:@"result"]) kind = rb::MenuPage::Result;
        else throw rb::Error("unknown original menu page");
        _presentation = std::make_unique<rb::MenuPresentation>(root.fileSystemRepresentation, kind);
        _images = [NSMutableDictionary dictionary];
    } catch (const std::exception &e) { menuError(error, e); return nil; }
    return self;
}
- (NSDictionary<NSString *, id> *)frameAtTick:(uint32_t)tick error:(NSError **)error {
    try {
        const auto frame = _presentation->frame(tick);
        NSMutableArray *sprites = [NSMutableArray array];
        for (const auto &sprite : frame.sprites) {
            NSMutableArray *quad = [NSMutableArray array], *clips = [NSMutableArray array], *multiply = [NSMutableArray array];
            for (const auto &p : sprite.quad) [quad addObject:[NSValue valueWithCGPoint:CGPointMake(p.x, p.y)]];
            for (auto value : sprite.multiply) [multiply addObject:@(value)];
            for (const auto &clip : sprite.clips) {
                NSMutableArray *points = [NSMutableArray array];
                for (const auto &p : clip) [points addObject:[NSValue valueWithCGPoint:CGPointMake(p.x, p.y)]];
                [clips addObject:points];
            }
            NSString *identity = @((sprite.package + ":" + sprite.texture).c_str());
            NSMutableDictionary *draw = [@{@"id": @(sprite.identity.c_str()), @"name": identity,
                @"quad": quad, @"clips": clips, @"multiply": multiply, @"layer": @(sprite.layer),
                @"alpha": @(sprite.alpha), @"blend": @(sprite.blend)} mutableCopy];
            if (sprite.solidColor) {
                NSMutableArray *color = [NSMutableArray array];
                for (auto value : *sprite.solidColor) [color addObject:@(value)];
                draw[@"solidColor"] = color;
            } else {
                UIImage *image = _images[identity];
                if (!image) { image = menuImage(_presentation->bitmap(sprite.package, sprite.texture)); _images[identity] = image; }
                draw[@"image"] = image;
            }
            [sprites addObject:draw];
        }
        NSMutableArray *pending = [NSMutableArray array];
        for (const auto &item : frame.unresolved) [pending addObject:@(item.c_str())];
        return @{@"sprites": sprites, @"pending": pending};
    } catch (const std::exception &e) { menuError(error, e); return nil; }
}
@end
