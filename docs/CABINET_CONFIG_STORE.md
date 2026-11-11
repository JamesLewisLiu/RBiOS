# Cabinet configuration write boundary

`CabinetConfigStore` is native platform code implementing the accepted storage policy. It is not claimed as a recovered AVS file export or XML serializer. The caller provides original serialized bytes, the original relative path and the original write trigger.

Only XML paths under prop/dev are accepted. Scores, profiles and history must never be passed to this API. It rejects traversal, absolute paths, symlinked target components, absent parent directories and backup roots overlapping the resource root. The first existing file version is copied byte-for-byte to a separate backup root; an existing backup is retained. Backup failure prevents replacement.

Writes use an exclusively reserved staging directory on the destination filesystem, check write/flush/close results, then replace the destination using the platform rename operation (MoveFileExW on Windows). Failed staging files are removed. This does not guarantee survival of power loss on every filesystem. The resource directory is trusted application-selected storage, not a hostile concurrent filesystem sandbox. Callers must serialize writers through one store instance per resource root.

The current tests use newly created fixture directories beside the test executable, never the original resource tree. They cover raw non-UTF8 bytes, backup retention, replacement, creation, invalid destinations and backup failure. The iPad owner, security-scoped resource access, original configuration call sites and original XML serialization still need integration. No live resource configuration has been modified by these tests.
