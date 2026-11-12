# Original startup entry: recovery build

The default Apple root is OriginalBootViewController. It selects or restores resource access and invokes RBOriginalStartup. File/XML preparation uses a serial queue; a dedicated native runtime thread owns AVS TLS and service lifetimes. It does not construct the catalog, menu or StageSession, and does not fall back to the component harness after an error. Debug builds can explicitly use the --component-harness launch argument; Release builds cannot select that route.

## Executed prefix and exact stopping point

The current owner verifies the hash-bound game DLL, reads prop/avs-config.xml through the real native file adapter, and feeds Foundation XML events into recovered AVS property semantics. CP932 strings are converted back to original encoded bytes; binary images and resources are not rewritten. The launcher uses property_create flags 23, including duplicate nodes. XML syntax tokenization is native platform glue, not a claim that Foundation implements the entire original AVS parser. The encoding declaration is scanned through its terminator, including declarations longer than 1024 bytes; UTF-8 BOMs are handled explicitly and unsupported or conflicting encodings fail. Before platform tokenization, an in-memory byte pass protects original CR/CRLF text and literal attribute whitespace with numeric references, including CR inside CDATA. Comments and processing instructions remain unchanged; no original file is rewritten. Unsupported encodings/types/content, document-type declarations, custom entities and CP932 CDATA delimiter overlaps fail explicitly; external entity resolution is disabled. This protection does not claim complete original XML grammar equivalence.

The owner allocates the two 16 MiB heap buffers selected by the original launcher for this game, imports the original 2528-byte configuration, applies product-mode corrections and allocates the recovered descriptor pool. It checks the descriptor arena extent using the original 24-byte record accounting, while native payloads remain pointer-width safe.

It now executes the recovered thread construction chain, starts the reserved alarm task, starts the descriptor-service mutex and binds the main-thread context. With the normal configuration it reports bootComplete=false at libavs-win32.dll:0x10022CD0, the filesystem bootstrap. Earlier failures identify their actual stage. See [native runtime](AVS_NATIVE_RUNTIME.md) for ownership, source ordering and platform substitutions. Filesystem boot, EA3, game entry and TEST are not invoked. The original logging/filtering subsystem is also incomplete; the on-screen trace is adapter diagnostics, not original log replay. These facts prevent interpreting this prefix as successful AVS initialization.

This Release build stops before the game. The next required integration is filesystem bootstrap, followed by heap/network stages and actual EA3/game ownership, while completing missing service exports and logging/property behavior. Do not bypass the stop by supplying a successful callback. Retry and directory changes stop and join the existing native owner before resource access is released.

## Other recovered services in this increment

- XML event-to-property conversion covers the actual original AVS config; full original comparisons include complete trees and errors. Serialization and other content types remain unfinished.
- Real mutex resources and calling-thread TLS registration have explicit ownership, error and completion behavior. Native initial-worker creation and task execution are now implemented independently; task admission/recycling now has a concrete implementation, while original aggregate construction order and exported task/TLS integration remain unfinished.
- A concrete read-only fs mount registry resolves virtual paths, holds busy references during file access, and preserves mount options, including ignored nvram-specific keys in the original fs parser. Full addfs callbacks and other filesystem types remain unfinished.

These components are independently tested but are not invoked out of the original boot order to move the startup screen forward. Original resources and configs remain unchanged in this increment; no server or public SNTP endpoint is contacted.

## Verification

Portable suites cover XML-to-config import, native synchronization and mount/file lifetimes. Apple tests cover UTF-8 and CP932 config bytes, CDATA/entities, malformed/unsupported input, missing DLL failure and default-root selection. A successful build/test does not establish completion of the stopped bootstrap or device playability.

Finite resource pools and the alarm dispatcher are now implemented separately. The boot owner still must bind the reserved alarm task and construct the services in original order. No new component is called out of order to advance the displayed stop address.
