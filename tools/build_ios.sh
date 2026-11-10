#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode_16.4.app/Contents/Developer}"
export TMPDIR="$PWD/.cache/tmp/"
mkdir -p "$TMPDIR" .build
export CLANG_MODULE_CACHE_PATH="$PWD/.cache/clang"
python3 -B tools/generate_project.py
test -d "$DEVELOPER_DIR" || { echo "Selected Xcode is unavailable: $DEVELOPER_DIR" >&2; exit 1; }
xcodebuild -version | tee .build/xcode-version.txt
xcrun --sdk iphoneos --show-sdk-version | tee .build/iphoneos-sdk.txt
xcodebuild -project RBiOS.xcodeproj -scheme RBiOS -configuration Release \
  -sdk iphoneos -destination 'generic/platform=iOS' -derivedDataPath .build/DerivedData \
  -archivePath .build/RBiOS.xcarchive archive CODE_SIGNING_ALLOWED=NO \
  IPHONEOS_DEPLOYMENT_TARGET=14.0 ARCHS=arm64 ONLY_ACTIVE_ARCH=NO 2>&1 | tee .build/xcode-device.log
APP="$PWD/.build/RBiOS.xcarchive/Products/Applications/RBiOS.app"
python3 -B tools/verify_ios_binary.py "$APP" | tee .build/minimum-os-check.txt
mkdir -p .build/package/Payload
ditto "$APP" .build/package/Payload/RBiOS.app
(cd .build/package && ditto -c -k --keepParent Payload ../RBiOS-unsigned.ipa)
cp .build/RBiOS-unsigned.ipa .build/RBiOS-unsigned.tipa
cmp .build/RBiOS-unsigned.ipa .build/RBiOS-unsigned.tipa
(cd .build && shasum -a 256 RBiOS-unsigned.ipa RBiOS-unsigned.tipa > package-sha256.txt)
echo 'IPA and TrollStore TIPA built; device installation/signing and iPadOS 14.6 runtime checks remain separate.'
