# Original startup entry: recovery build

The default Apple root is now OriginalBootViewController. It selects or restores resource access and invokes RBOriginalStartup on one serial queue. It does not construct the catalog, menu or StageSession, and does not fall back to the component harness after an error. Debug builds can explicitly use the --component-harness launch argument; Release builds cannot select that route.

## Executed prefix and exact stopping point

The current owner verifies the hash-bound game DLL, reads prop/avs-config.xml through the real native file adapter, and feeds Foundation XML events into recovered AVS property semantics. CP932 strings are converted back to original encoded bytes; binary images and resources are not rewritten. The launcher uses property_create flags 23, including duplicate nodes. XML syntax tokenization is native platform glue, not a claim that Foundation implements the entire original AVS parser. The encoding declaration is scanned through its terminator, including declarations longer than 1024 bytes; UTF-8 BOMs are handled explicitly and unsupported or conflicting encodings fail. Before platform tokenization, an in-memory byte pass protects original CR/CRLF text and literal attribute whitespace with numeric references, including CR inside CDATA. Comments and processing instructions remain unchanged; no original file is rewritten. Unsupported encodings/types/content, document-type declarations, custom entities and CP932 CDATA delimiter overlaps fail explicitly; external entity resolution is disabled. This protection does not claim complete original XML grammar equivalence.

The owner allocates the two 16 MiB heap buffers selected by the original launcher for this game, imports the original 2528-byte configuration, applies product-mode corrections and allocates the recovered descriptor pool. It checks the descriptor arena extent using the original 24-byte record accounting, while native payloads remain pointer-width safe.

It then reports bootComplete=false at libavs-win32.dll:0x10029410. This complete thread bootstrap remains missing. It initializes thread policy, workers and several resource pools; the separately recovered mutex and calling-thread components do not replace that chain. No descriptor-service start, filesystem boot, EA3 boot, game entry or TEST scene is invoked after that boundary. The original logging/filtering subsystem is also not complete; the on-screen trace is adapter diagnostics, not original log replay. These facts prevent interpreting this prefix as successful AVS initialization.

This Release build intentionally stops before the game. It replaces the misleading automatic harness route with visible recovery state, not with a completed original game runtime. The next required implementation is the complete 10029410 chain, followed by heap/time/filesystem/network stages and actual EA3/game ownership. Do not bypass the stop by supplying a successful callback.

## Other recovered services in this increment

- XML event-to-property conversion covers the actual original AVS config; full original comparisons include complete trees and errors. Serialization and other content types remain unfinished.
- Real mutex resources and calling-thread TLS registration have explicit ownership, error and completion behavior. Native initial-worker creation and task execution are now implemented independently; full task admission/recycling and aggregate initialization remain unfinished.
- A concrete read-only fs mount registry resolves virtual paths, holds busy references during file access, and preserves mount options, including ignored nvram-specific keys in the original fs parser. Full addfs callbacks and other filesystem types remain unfinished.

These components are independently tested but are not invoked out of the original boot order to move the startup screen forward. Original resources and configs remain unchanged in this increment; no server or public SNTP endpoint is contacted.

## Verification

Portable suites cover XML-to-config import, native synchronization and mount/file lifetimes. Apple tests cover UTF-8 and CP932 config bytes, CDATA/entities, malformed/unsupported input, missing DLL failure and default-root selection. A successful build/test does not establish completion of the stopped bootstrap or device playability.
