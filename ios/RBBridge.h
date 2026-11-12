#import <UIKit/UIKit.h>
NS_ASSUME_NONNULL_BEGIN
@interface RBOriginalStartup : NSObject
- (void)stop;
+ (nullable NSData *)propertyDocumentDataFromXML:(NSData *)xml error:(NSError **)error
    NS_SWIFT_NAME(propertyDocumentData(xml:));
+ (nullable NSData *)configurationImageFromXML:(NSData *)xml error:(NSError **)error
    NS_SWIFT_NAME(configurationImage(xml:));
- (nullable NSDictionary<NSString *, id> *)startAtRoot:(NSURL *)root error:(NSError **)error
    NS_SWIFT_NAME(start(at:));
@end
@interface RBBridge : NSObject
+ (nullable NSArray<NSDictionary<NSString *, id> *> *)catalogAt:(NSURL *)root
                                                          error:(NSError **)error
    NS_SWIFT_NAME(catalog(at:));
+ (nullable NSDictionary<NSString *, id> *)chartAt:(NSURL *)url
                                             error:(NSError **)error NS_SWIFT_NAME(chart(at:));
+ (nullable UIImage *)firstTextureAt:(NSURL *)url
                               error:(NSError **)error NS_SWIFT_NAME(firstTexture(at:));
+ (CGRect)displayRectIn:(CGRect)safe
               fraction:(double)fraction NS_SWIFT_NAME(displayRect(in:fraction:));
+ (nullable NSValue *)gamePoint:(CGPoint)point
                        display:(CGRect)display NS_SWIFT_NAME(gamePoint(_:display:));
+ (NSString *)migrationStatus;
@end
// A session owns only transient C++ state and in-memory decoded original images.
@interface RBGameSession : NSObject
- (nullable instancetype)initWithRoot:(NSURL *)root songIndex:(NSUInteger)index
                          difficulty:(NSUInteger)difficulty error:(NSError **)error
    NS_SWIFT_NAME(init(root:songIndex:difficulty:));
@property(nonatomic, readonly) NSURL *audioURL;
@property(nonatomic, readonly) NSDictionary<NSString *, NSURL *> *soundResources;
- (nullable NSDictionary<NSString *, UIImage *> *)prepareStageImagesWithError:(NSError **)error;
- (BOOL)advanceLogicToTick:(int32_t)tick touches:(NSArray<NSValue *> *)touches error:(NSError **)error
    NS_SWIFT_NAME(advanceLogic(to:touches:));
- (nullable NSDictionary<NSString *, id> *)presentationWithError:(NSError **)error;
- (nullable NSDictionary<NSString *, id> *)advanceToTick:(int32_t)tick
                                                touches:(NSArray<NSValue *> *)touches
                                                  error:(NSError **)error
    NS_SWIFT_NAME(advance(to:touches:));
- (void)cancelTouches;
@end
// Offline menu adapters retain no account, score or history state.
@interface RBDifficultyMenu : NSObject
+ (NSDictionary<NSString *, NSURL *> *)soundResourcesAtRoot:(NSURL *)root NS_SWIFT_NAME(soundResources(at:));
- (nullable instancetype)initWithAvailable:(NSArray<NSNumber *> *)available initial:(NSInteger)initial error:(NSError **)error
    NS_SWIFT_NAME(init(available:initial:));
- (nullable NSDictionary<NSString *, id> *)updateAtTick:(int32_t)tick action:(NSString *)action error:(NSError **)error
    NS_SWIFT_NAME(update(at:action:));
@property(nonatomic, readonly) NSInteger selected;
@end
@interface RBMenuScene : NSObject
- (nullable instancetype)initWithRoot:(NSURL *)root page:(NSString *)page error:(NSError **)error
    NS_SWIFT_NAME(init(root:page:));
- (nullable NSDictionary<NSString *, id> *)frameAtTick:(uint32_t)tick error:(NSError **)error
    NS_SWIFT_NAME(frame(at:));
@end
@interface RBAudioDecoder : NSObject
@property(nonatomic, readonly) NSUInteger blockCount;
@property(nonatomic, readonly) double sampleRate;
@property(nonatomic, readonly) uint64_t frames;
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable NSData *)decodeBlock:(NSUInteger)index error:(NSError **)error;
@end
NS_ASSUME_NONNULL_END
