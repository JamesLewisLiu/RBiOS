#import <UIKit/UIKit.h>
NS_ASSUME_NONNULL_BEGIN
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
- (nullable NSDictionary<NSString *, id> *)advanceToTick:(int32_t)tick
                                                touches:(NSArray<NSValue *> *)touches
                                                  error:(NSError **)error
    NS_SWIFT_NAME(advance(to:touches:));
- (void)cancelTouches;
@end
@interface RBAudioDecoder : NSObject
@property(nonatomic, readonly) NSUInteger blockCount;
@property(nonatomic, readonly) double sampleRate;
@property(nonatomic, readonly) uint64_t frames;
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable NSData *)decodeBlock:(NSUInteger)index error:(NSError **)error;
@end
NS_ASSUME_NONNULL_END
