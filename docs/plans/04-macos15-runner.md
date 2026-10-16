# Build revision: macos-15

Recorded 2026-11-04 following the request to continue and evaluate macos-15. This supersedes revision 03's default self-hosted Xcode 15.4 choice; platform, resource and network constraints remain.

- Default hosted macos-15 with explicitly selected Xcode 16.4.
- Keep minimum iPadOS 14.0; do not silently raise it to 15.
- Archive unsigned device arm64, then validate application plist and all Mach-O minimum versions/platforms before packaging.
- Run adapter tests on an installed iPad simulator; do not call a modern simulator a14.6 device test.
- Retain repository-variable overrides for self-hosted runner labels and Xcode path. Do not modify hosted systems or mix SDKs.
- The implementation host is Windows. Project/scripts existed at this revision, but Xcode, Actions and device installation had not run. Actual build/artifact results decide success.

References: [GitHub runner inventory](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-Readme.md), [Apple requirements](https://developer.apple.com/xcode/system-requirements).
