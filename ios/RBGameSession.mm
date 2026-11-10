#import "RBBridge.h"
#include "rb/GamePresentation.hpp"
#include "rb/StageScoreInitialization.hpp"
#include "rb/StageSound.hpp"
#include <cstdlib>
#include <memory>

namespace {
void sessionError(NSError **error, const std::exception &e) {
    if (error)
        *error = [NSError errorWithDomain:@"RBiOS.Session"
                                     code:1
                                 userInfo:@{
                                     NSLocalizedDescriptionKey : @(e.what())
                                 }];
}
std::string filePath(NSURL *url) {
    return std::string(url.fileSystemRepresentation);
}
UIImage *originalImage(const rb::AfpBitmap &bitmap) {
    NSData *data = [NSData dataWithBytes:bitmap.bgra.data() length:bitmap.bgra.size()];
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(bitmap.width, bitmap.height, 8, 32, bitmap.width * 4, color,
                                     CGBitmapInfo(kCGBitmapByteOrder32Little | kCGImageAlphaFirst),
                                     provider, nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(color);
    CGDataProviderRelease(provider);
    if (!image)
        throw rb::Error("original texture CGImage creation failed");
    UIImage *result = [UIImage imageWithCGImage:image];
    CGImageRelease(image);
    return result;
}
} // namespace
@implementation RBGameSession {
    // Reverse destruction order keeps the callback target alive until session destruction.
    std::unique_ptr<rb::GamePresentation> _presentation;
    std::unique_ptr<rb::StageSoundQueue> _soundQueue;
    std::unique_ptr<rb::StageSession> _session;
    NSMutableArray<NSString *> *_soundEvents;
    NSDictionary<NSString *, NSURL *> *_soundResources;
    NSMutableDictionary<NSString *, UIImage *> *_images;
    NSURL *_audioURL;
    unsigned _difficulty;
}
- (instancetype)initWithRoot:(NSURL *)root
                   songIndex:(NSUInteger)index
                  difficulty:(NSUInteger)difficulty
                       error:(NSError **)error {
    self = [super init];
    if (!self)
        return nil;
    try {
        if (difficulty >= 4)
            throw rb::Error("invalid original difficulty");
        NSError *catalogError = nil;
        if (![RBBridge catalogAt:root error:&catalogError]) {
            if (error)
                *error = catalogError;
            return nil;
        }
        auto songs = rb::readCatalog(
            rb::readFile(filePath([root URLByAppendingPathComponent:@"modules/reflecbeat.dll"])));
        if (index >= songs.size())
            throw rb::Error("song catalog index outside original table");
        const auto &song = songs[index];
        auto chartURL =
            [root URLByAppendingPathComponent:@(song.chartPath(unsigned(difficulty)).c_str())];
        auto chart = rb::Chart::parse(rb::readFile(filePath(chartURL)));
        rb::SheetOptions sheetOptions;
        sheetOptions.difficulty = int32_t(difficulty);
        const uint32_t sessionSeed = arc4random(); // platform entropy, restored RNG thereafter
        sheetOptions.seed = sessionSeed;
        auto sheet = rb::MusicSheet::load(std::move(chart), sheetOptions);
        sheet.prepare();
        _presentation =
            std::make_unique<rb::GamePresentation>(filePath(root), unsigned(difficulty));
        auto *presentation = _presentation.get();
        _soundQueue = std::make_unique<rb::StageSoundQueue>();
        auto *soundQueue = _soundQueue.get();
        _soundEvents = [NSMutableArray array];
        auto soundEvents = _soundEvents;
        NSMutableDictionary *resources = [NSMutableDictionary dictionary];
        for (const auto &cue : rb::stageSoundResources())
            resources[@(cue.key)] = [root URLByAppendingPathComponent:@(cue.path)];
        _soundResources = [resources copy];
        rb::SessionOptions options;
        options.onJudgment = [soundQueue](const rb::SessionJudgmentEvent &event) {
            soundQueue->submit(event.tick, event.side, event.kind, event.result.original.grade,
                               event.result.original.reason, event.switchDirection);
        };
        options.randomSeed = sessionSeed;
        options.initialBattle = rb::initializeStageScore(
            rb::StageScoreInitialization::offline(sheet, song.levels[difficulty]));
        options.endTick = 1000;
        options.onNoteCreated = [presentation](int32_t id, const auto &bindings) {
            presentation->noteCreated(id, bindings);
        };
        options.onNoteReleased = [presentation](int32_t id) { presentation->noteReleased(id); };
        options.onPresentationFrame = [presentation, soundQueue, soundEvents](const auto &snapshot) {
            presentation->advance(snapshot);
            for (const auto &cue : soundQueue->drain(snapshot.tick))
                [soundEvents addObject:@(cue.key)];
        };
        _session = std::make_unique<rb::StageSession>(std::move(sheet), options);
        _audioURL =
            [root URLByAppendingPathComponent:@(song.audioPath(unsigned(difficulty)).c_str())];
        _difficulty = unsigned(difficulty);
        _images = [NSMutableDictionary dictionary];
    } catch (const std::exception &e) {
        sessionError(error, e);
        return nil;
    }
    return self;
}
- (NSDictionary<NSString *, NSURL *> *)soundResources { return _soundResources; }
- (NSURL *)audioURL {
    return _audioURL;
}
- (UIImage *)imageInPackage:(const std::string &)package texture:(const std::string &)texture {
    const auto identity = package + ":" + texture;
    NSString *key = @(identity.c_str());
    UIImage *cached = _images[key];
    if (cached)
        return cached;
    auto image = originalImage(_presentation->bitmap(package, texture));
    _images[key] = image;
    return image;
}
- (NSDictionary<NSString *, UIImage *> *)prepareStageImagesWithError:(NSError **)error {
    try {
        for (const auto &entry : _presentation->prepareStageBitmaps())
            [self imageInPackage:entry.first texture:entry.second];
        return [_images copy];
    } catch (const std::exception &e) { sessionError(error, e); return nil; }
}
- (NSDictionary<NSString *, id> *)advanceToTick:(int32_t)tick
                                        touches:(NSArray<NSValue *> *)touches
                                          error:(NSError **)error {
    if (![self advanceLogicToTick:tick touches:touches error:error]) return nil;
    return [self presentationWithError:error];
}
- (BOOL)advanceLogicToTick:(int32_t)tick touches:(NSArray<NSValue *> *)touches error:(NSError **)error {
    try {
        std::vector<rb::TouchPoint> points;
        for (NSValue *value in touches) {
            CGPoint p = value.CGPointValue;
            points.push_back({int32_t(p.x), int32_t(p.y)});
        }
        _session->advanceToTick(tick, points);
        return YES;
    } catch (const std::exception &e) { sessionError(error, e); return NO; }
}
- (NSDictionary<NSString *, id> *)presentationWithError:(NSError **)error {
    try {
        auto snapshot = _session->snapshot();
        if (!snapshot.failure.empty())
            throw rb::Error(snapshot.failure);
        auto frame = _presentation->frame(snapshot);
        NSMutableArray *draws = [NSMutableArray array];
        for (const auto &sprite : frame.sprites) {
            NSMutableArray *quad = [NSMutableArray array], *multiply = [NSMutableArray array];
            for (const auto &p : sprite.quad)
                [quad addObject:[NSValue valueWithCGPoint:CGPointMake(p.x, p.y)]];
            for (float channel : sprite.multiply)
                [multiply addObject:@(channel)];
            NSMutableArray *clips = [NSMutableArray array];
            for (const auto &clip : sprite.clips) {
                NSMutableArray *points = [NSMutableArray array];
                for (const auto &point : clip)
                    [points addObject:[NSValue valueWithCGPoint:CGPointMake(point.x, point.y)]];
                [clips addObject:points];
            }
            const auto textureIdentity = sprite.package + ":" + sprite.texture;
            [draws addObject:@{
                @"id" : @(sprite.identity.c_str()),
                @"name" : @(textureIdentity.c_str()),
                @"image" : [self imageInPackage:sprite.package texture:sprite.texture],
                @"quad" : quad,
                @"clips" : clips,
                @"layer" : @(sprite.layer),
                @"alpha" : @(sprite.alpha),
                @"multiply" : multiply,
                @"blend" : @(sprite.blend)
            }];
        }
        NSMutableSet<NSString *> *pending = [NSMutableSet set];
        for (const auto &item : frame.unresolved)
            [pending addObject:@(item.c_str())];
        NSMutableArray *players = [NSMutableArray array];
        for (unsigned side = 0; side < 2; ++side) {
            const auto &p = snapshot.players[side];
            NSMutableArray *counts = [NSMutableArray array];
            for (auto n : p.counts)
                [counts addObject:@(n)];
            [players addObject:@{
                @"score" : @(snapshot.battle.score[side]),
                @"combo" : @(p.combo),
                @"maxCombo" : @(p.maxCombo),
                @"rate" : @(p.currentRate()),
                @"finalRate" : @(p.finalRate()),
                @"counts" : counts,
                @"medal" : @(p.medal(_difficulty, 0)),
                @"clearType" : @(p.clearType(_difficulty, 0))
            }];
        }
        NSArray *sounds = [_soundEvents copy];
        [_soundEvents removeAllObjects];
        return @{
            @"sounds" : sounds,
            @"tick" : @(snapshot.tick),
            @"complete" : @(snapshot.complete),
            @"created" : @(snapshot.created),
            @"processed" : @(snapshot.processed),
            @"sprites" : draws,
            @"players" : players,
            @"pendingPresentation" : pending.allObjects
        };
    } catch (const std::exception &e) {
        sessionError(error, e);
        return nil;
    }
}
- (void)cancelTouches {
    [_soundEvents removeAllObjects];
    if (_session)
        _session->cancelTouches();
}
@end
