# Scope revision: stage entry without persistence

Recorded 2026-11-04. The user removed persistence, planned later server-backed data, prioritized working stage entry and required unconverted original resources.

- Remove local score/history/profile databases and deferred upload queues. Keep original scoring/result calculations in session memory only.
- Retain device display preferences and resource-access authorization; neither stores scores or player profiles.
- Prioritize original preprocessing, creation/update/judgment, CPU, audio clock and original-resource presentation, connecting selection through stage and results.
- Keep original PLY/IFS/AFP/fonts/2dx/SD9 files and bytes. Decompression, texture cropping and audio decoding occur in memory, without PNG/WAV or replacement packages.
- Stay offline. Later server read/write still requires original protocol restoration.
- Restrict local work to E:\MBR-2019121100\IDA. This historical revision prohibited commits/pushes; [revision 06](06-english-local-delivery.md) later authorizes a local commit.

This revision overrides local-score persistence in earlier plans. Those records remain for traceability.
