# macos-15 and iPadOS 14 builds

The default Actions runner is macos-15, selecting /Applications/Xcode_16.4.app explicitly. The [official runner inventory](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-Readme.md), checked on 2026-11-05, lists Xcode 16.4. No XcodeGen, Homebrew installation or game files are needed to build.

## Workflow

Native core and iPadOS build runs portable C++/Python checks, an unsigned device archive, minimum-OS validation and adapter tests on an available iPad simulator. Simulated networking remains inside an in-memory URLProtocol.

A local commit does not start Actions; pushing to the GitHub repository starts the workflow. The stage-frame implementation at `9cbc657` passed [Actions run 42052166298](https://github.com/JamesLewisLiu/RBiOS/actions/runs/42052166298): 22 portable C++ programs, 9 Python tests, the arm64 archive and all 21 simulator tests. The GPU regression includes nested clipping at 50%, 75% and 100% display size.

The rbios-build-and-diagnostics artifact contains the IPA if packaging succeeded, Xcode/SDK versions, device log, minimum-OS report and XCTest results. A modern simulator does not validate iPadOS 14.6.

The workflow_dispatch input xcode_path, or repository variable XCODE_PATH, selects another installed Xcode application. Specify its .app path; the workflow adds /Contents/Developer.

Repository variable IOS_RUNNER_LABELS can select a self-hosted machine, for example ["self-hosted","macOS","rb-ios14"]. Point XCODE_PATH at its actual installation. The historical Xcode 15.4 option remains for compatible self-hosted macOS. Hosted macos-15 is not modified with an old Xcode or mixed SDK.

## Archive and package

~~~sh
DEVELOPER_DIR=/Applications/Xcode_16.4.app/Contents/Developer bash tools/build_ios.sh
~~~

The script regenerates the deterministic project, records tool versions and archives arm64 with the iphoneos SDK and signing disabled. It validates the application and embedded Mach-O files before creating .build/RBiOS-unsigned.ipa.

Info.plist and Mach-O minimum OS values must be at most 14.0. Wrong platforms, simulator code, non-arm64 slices and malformed binaries fail validation. The script never rewrites a binary minimum version to conceal incompatibility.

Apple's [requirements table](https://developer.apple.com/xcode/system-requirements) lists iOS 15-18 for Xcode 16.4 and explains deployment ranges in terms of App Store Connect uploads. This alone does not validate an unsigned iOS 14 archive or iPadOS 14.6 execution. Actual compilation, artifact inspection and device tests decide compatibility.

The unsigned package needs an appropriate installation/signing mechanism. No JIT, Wine/Boxedwine or x86 DLL execution layer is included.

## Portable checks

~~~sh
cmake -S . -B .build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/host --parallel 3
ctest --test-dir .build/host --output-on-failure
python3 -B -m unittest discover -s tests -p 'test_*.py'
python3 -B tools/check_repository_text.py
~~~

On the prepared Windows workspace:

~~~powershell
python -B tools/build_host.py
~~~

The generator discovers all core sources, Apple adapters and XCTest files. CMake and the host build share the component test list. C++ uses -ffp-contract=off; ARM64 behavior still needs validation.

Keep analysis dependencies in .tools, caches in .cache and outputs in .build. Do not commit game data, analysis binaries or decoded media. Swift/Objective-C++ and SDK diagnostics require an actual Apple build.
