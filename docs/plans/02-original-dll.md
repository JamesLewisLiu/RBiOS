# Original DLL execution plan - historical archive

This unselected proposal summarizes the earlier conversation. Target: M1 iPad Pro, iPadOS 14.6, TrollStore and an app minimum of 14. JIT availability was a proposed requirement, not demonstrated compatibility.

The proposed chain was UIKit -> Boxedwine ARM64 JIT/Linux compatibility -> 32-bit Wine -> original spice.exe -modules modules -smartea -> original AVS/EA3/AFP/reflecbeat.dll.

The reference was BoxedVN commit 7b6e03575922967ca161f2b55c39846e242916f6, with WineD3D Vulkan -> MoltenVK 1.4.1 -> Metal. The plan proposed replacing newer JIT handshakes with suitable 14.6 debugging permissions and actual execution probes, removing FEX64/DXMT/MetalFX, and retaining DirectSound/ACM ring-buffer semantics.

It would preserve PE32 loading, relocations, CRT/TLS, SEH, x86 execution and raw assets. Spice Touch API write/write_reset would bridge multi-touch and original COM1 behavior. A writable Wine prefix would remain separate from original resources.

Proposed gates: JIT/Wine -> D3D9/audio examples -> first original-game frame -> full manual play/results -> M1 performance and synchronization acceptance. The reference project did not establish this game's playability. This route was not implemented or device-tested.
