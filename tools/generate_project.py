"""Deterministic Xcode project generation, no XcodeGen/Homebrew dependency."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
objects = {}
def ident(key): return hashlib.sha256(key.encode()).hexdigest()[:24].upper()
def obj(key, isa, **fields):
    uid = ident(key); objects[uid] = {'isa': isa, **fields}; return uid
class Raw(str): pass
def ref(key): return Raw(ident(key))
def render(v):
    if isinstance(v, Raw): return v
    if isinstance(v, dict): return '{ ' + ' '.join(f'{k} = {render(x)};' for k, x in v.items()) + ' }'
    if isinstance(v, list): return '(' + ', '.join(render(x) for x in v) + ')'
    if isinstance(v, (int, float)): return str(v)
    return json.dumps(v, ensure_ascii=True)

app_sources = sorted([*ROOT.glob('ios/*.swift'), *ROOT.glob('ios/*.mm'), *ROOT.glob('core/src/*.cpp')])
test_sources = sorted(ROOT.glob('iosTests/*.swift'))
all_refs = []
def sources(key, paths):
    build = []
    for p in paths:
        rel = p.relative_to(ROOT).as_posix(); suffix = p.suffix
        kind = {'.swift': 'sourcecode.swift', '.mm': 'sourcecode.cpp.objcpp', '.cpp': 'sourcecode.cpp.cpp'}[suffix]
        f = obj('file:'+rel, 'PBXFileReference', lastKnownFileType=kind, path=rel, sourceTree='SOURCE_ROOT'); all_refs.append(Raw(f))
        b = obj('build:'+rel, 'PBXBuildFile', fileRef=Raw(f)); build.append(Raw(b))
    return obj(key, 'PBXSourcesBuildPhase', buildActionMask=2147483647, files=build, runOnlyForDeploymentPostprocessing=0)
sources('appSources', app_sources); sources('testSources', test_sources)
for p in sorted(ROOT.glob('ios/*.h')) + sorted(ROOT.glob('core/include/rb/*.hpp')) + [ROOT/'ios/Info.plist']:
    rel = p.relative_to(ROOT).as_posix(); kind = 'text.plist.xml' if p.suffix == '.plist' else 'sourcecode.cpp.h' if p.suffix == '.hpp' else 'sourcecode.c.h'
    all_refs.append(Raw(obj('file:'+rel, 'PBXFileReference', lastKnownFileType=kind, path=rel, sourceTree='SOURCE_ROOT')))
framework_build = []
for name in ['UIKit', 'Foundation', 'SpriteKit', 'MetalKit', 'Metal', 'AVFoundation', 'QuartzCore', 'UniformTypeIdentifiers']:
    f = obj('framework:'+name, 'PBXFileReference', lastKnownFileType='wrapper.framework', name=name+'.framework', path=f'System/Library/Frameworks/{name}.framework', sourceTree='SDKROOT')
    all_refs.append(Raw(f)); framework_build.append(Raw(obj('frameworkBuild:'+name,'PBXBuildFile',fileRef=Raw(f))))
obj('appFrameworks','PBXFrameworksBuildPhase',buildActionMask=2147483647,files=framework_build,runOnlyForDeploymentPostprocessing=0)
obj('testFrameworks','PBXFrameworksBuildPhase',buildActionMask=2147483647,files=[],runOnlyForDeploymentPostprocessing=0)
obj('appResources','PBXResourcesBuildPhase',buildActionMask=2147483647,files=[],runOnlyForDeploymentPostprocessing=0)
obj('appProduct','PBXFileReference',explicitFileType='wrapper.application',path='RBiOS.app',sourceTree='BUILT_PRODUCTS_DIR')
obj('testProduct','PBXFileReference',explicitFileType='wrapper.cfbundle',path='RBiOSTests.xctest',sourceTree='BUILT_PRODUCTS_DIR')
obj('products','PBXGroup',children=[ref('appProduct'),ref('testProduct')],name='Products',sourceTree='<group>')
obj('main','PBXGroup',children=all_refs+[ref('products')],sourceTree='<group>')
common = {'IPHONEOS_DEPLOYMENT_TARGET':'14.0','SDKROOT':'iphoneos','CLANG_CXX_LANGUAGE_STANDARD':'c++17','CLANG_ENABLE_OBJC_ARC':'YES','CLANG_ENABLE_MODULES':'YES','SWIFT_VERSION':'5.0','SWIFT_STRICT_CONCURRENCY':'minimal','ENABLE_BITCODE':'NO','TARGETED_DEVICE_FAMILY':'2','HEADER_SEARCH_PATHS':['$(inherited)','$(SRCROOT)/core/include'],'SUPPORTED_PLATFORMS':'iphoneos iphonesimulator','CODE_SIGN_STYLE':'Automatic','PRODUCT_NAME':'$(TARGET_NAME)'}
for target in ['project','app','test']:
    configs = []
    for mode in ['Debug','Release']:
        settings = dict(common)
        settings['OTHER_CPLUSPLUSFLAGS'] = ['$(inherited)', '-ffp-contract=off']
        settings['SWIFT_ACTIVE_COMPILATION_CONDITIONS'] = ['DEBUG'] if mode == 'Debug' else []
        settings.update({'SWIFT_OPTIMIZATION_LEVEL':'-Onone' if mode=='Debug' else '-O','GCC_OPTIMIZATION_LEVEL':'0' if mode=='Debug' else '2','DEBUG_INFORMATION_FORMAT':'dwarf','ENABLE_TESTABILITY':'YES' if mode=='Debug' else 'NO'})
        if target=='app':settings.update({'PRODUCT_BUNDLE_IDENTIFIER':'org.rbios.native','INFOPLIST_FILE':'ios/Info.plist','SWIFT_OBJC_BRIDGING_HEADER':'ios/RBiOS-Bridging-Header.h','LD_RUNPATH_SEARCH_PATHS':['$(inherited)','@executable_path/Frameworks']})
        if target=='test':settings.update({'PRODUCT_BUNDLE_IDENTIFIER':'org.rbios.native.tests','GENERATE_INFOPLIST_FILE':'YES','SWIFT_OBJC_BRIDGING_HEADER':'ios/RBiOS-Bridging-Header.h','TEST_HOST':'$(BUILT_PRODUCTS_DIR)/RBiOS.app/RBiOS','BUNDLE_LOADER':'$(TEST_HOST)','LD_RUNPATH_SEARCH_PATHS':['$(inherited)','@executable_path/Frameworks','@loader_path/Frameworks']})
        configs.append(Raw(obj(target+mode,'XCBuildConfiguration',buildSettings=settings,name=mode)))
    obj(target+'Configurations','XCConfigurationList',buildConfigurations=configs,defaultConfigurationIsVisible=0,defaultConfigurationName='Release')
obj('proxy','PBXContainerItemProxy',containerPortal=ref('project'),proxyType=1,remoteGlobalIDString=ref('app'),remoteInfo='RBiOS')
obj('dependency','PBXTargetDependency',target=ref('app'),targetProxy=ref('proxy'))
obj('app','PBXNativeTarget',buildConfigurationList=ref('appConfigurations'),buildPhases=[ref('appSources'),ref('appFrameworks'),ref('appResources')],buildRules=[],dependencies=[],name='RBiOS',productName='RBiOS',productReference=ref('appProduct'),productType='com.apple.product-type.application')
obj('test','PBXNativeTarget',buildConfigurationList=ref('testConfigurations'),buildPhases=[ref('testSources'),ref('testFrameworks')],buildRules=[],dependencies=[ref('dependency')],name='RBiOSTests',productName='RBiOSTests',productReference=ref('testProduct'),productType='com.apple.product-type.bundle.unit-test')
obj('project','PBXProject',attributes={'LastUpgradeCheck':'1640','TargetAttributes':{ident('test'):{'TestTargetID':ref('app')}}},buildConfigurationList=ref('projectConfigurations'),compatibilityVersion='Xcode 14.0',developmentRegion='en',hasScannedForEncodings=0,knownRegions=['en','Base'],mainGroup=ref('main'),productRefGroup=ref('products'),projectDirPath='',projectRoot='',targets=[ref('app'),ref('test')])
project = ROOT/'RBiOS.xcodeproj'; project.mkdir(exist_ok=True)
text = '// !$*UTF8*$!\n' + render({'archiveVersion':1,'classes':{},'objectVersion':56,'objects':{k:objects[k] for k in sorted(objects)},'rootObject':ref('project')}) + '\n'
(project/'project.pbxproj').write_text(text, encoding='utf-8')
scheme = project/'xcshareddata/xcschemes';scheme.mkdir(parents=True,exist_ok=True)
def buildref(target, name):return f'<BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{ident(target)}" BuildableName="{name}" BlueprintName="{name.split(".")[0]}" ReferencedContainer="container:RBiOS.xcodeproj"/>'
(scheme/'RBiOS.xcscheme').write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1640" version="1.3">
<BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES">{buildref('app','RBiOS.app')}</BuildActionEntry></BuildActionEntries></BuildAction>
<TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables><TestableReference skipped="NO">{buildref('test','RBiOSTests.xctest')}</TestableReference></Testables></TestAction>
<LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0">{buildref('app','RBiOS.app')}</BuildableProductRunnable></LaunchAction>
<ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES" savedToolIdentifier="" useCustomWorkingDirectory="NO" debugDocumentVersioning="YES"><BuildableProductRunnable runnableDebuggingMode="0">{buildref('app','RBiOS.app')}</BuildableProductRunnable></ProfileAction>
<AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>''',encoding='utf-8')
print('Generated RBiOS.xcodeproj')
