# Native reimplementation plan - historical archive

This record summarizes the earlier conversation. It was superseded by source-logic recovery and native migration. It is a design archive, not evidence of an implemented application.

The target was iPadOS 14+, portrait 768x1360, offline selection, CPU matches, results and local scores, with networking deferred.

Swift/UIKit/SpriteKit/AVFoundation would provide the platform. RBAssets would directly read original RBFF, IFS/KBinXML/AVSLZ, 2dx/SD9 and fonts with runtime decoding only. RBCore would reimplement routes, judgment, score and page animation from analysis. Audio would drive stage timing; textures would load on demand.

Acceptance covered original-resource hashes, format coverage, judgment replays, screenshot comparison and M1/iPadOS 14.6 play. The original build proposal used hosted macOS14/Xcode 15.4 with a self-hosted migration option; that hosted image is retired.

All local work was restricted to E:\MBR-2019121100\IDA, with user-managed pushing. Later revisions govern current implementation, persistence and delivery.
