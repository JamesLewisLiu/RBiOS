# AVS thread bootstrap policy

`AvsThreadBootstrap` restores the policy and arithmetic consumed by the original
worker constructor. `AvsNativeWorkerPool` separately creates real native workers.
A calculated plan does not allocate workers or complete the aggregate AVS boot.

## Source evidence

The source DLL SHA-256 is
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Fresh Official IDA evidence is retained in
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/`:
`official-main-thread-pseudocode.json`, `official-thread-bootstrap-pseudocode.json`,
`official-worker-leaves-pseudocode.json`, and `official-worker-bodies-pseudocode.json`.
`thread-pool-instructions.txt` preserves the original instructions used to resolve
register arguments omitted by decompiler prototypes.

| Original routine | Native recovery |
| --- | --- |
| `10039570` | Population count of the 32-bit process affinity mask |
| `10039530` | Spin count 4000 for two or more available processors, otherwise zero; semaphore limit equals configured thread count |
| `1001ABC0` | Stack validation/alignment, initial count clamp, ordered warnings, affinity intersection, original arena sizing and worker constructor arguments |
| `1005C1D0` | Initialize the circular affinity list from a zeroed worker-pool object |
| `1005C1A0`, `1005C0E0` | Select the next CPU and update the original 16-bit usage counter/list ordering |

The thread configuration starts at full AVS image offset 72. Local offsets are
affinity `+0`, worker stack `+8`, expiry `+12`, initial workers `+16`, minimum
stack `+24`, maximum stack `+28`, and thread count `+32`. The native plan reads
bytes explicitly; it never overlays the x86 layout on native C++ objects.

Stack rounding uses the original wrapping 32-bit expression
`(value + 4095) & 0xFFFFF000`. Zero thread count, zero rounded limits, or minimum
greater than maximum are rejected. The worker stack clamp preserves the original
signed-high-bit test, including its unsigned arithmetic. Zero worker stack stays
zero. The expiry-count-one message is recorded as a warning, not interpreted as
permission to skip construction of the requested initial workers.

The original process mask is reduced to a processor count, then the configured
64-bit affinity mask is intersected with that many low bits. It does not preserve
the process mask's original CPU positions. The plan retains this behavior; the
native affinity adapter reports if the resulting pin cannot be applied.

Original task arena consumption is `(48 * count + 51) & ~15`. The worker constructor
receives `72 * count + 331` bytes and the enclosing bootstrap advances its pointer
by `72 * count + 316`. These are original arena-accounting facts, not sizes of
native mutexes, pointers, stacks, or C++ worker objects.

## Validation

`AvsThreadBootstrapTests` validates imported defaults, warnings, CPU count policy,
zero worker stack, and affinity order. The original instruction oracle passed
**1,024 configuration cases** and **36 affinity sequences**, including signed
boundary stack values and 16-bit affinity counter wrap. The oracle executes
`10039530`, `1001ABC0`, and the affinity functions; task allocator and worker
constructor calls are captured only to compare the actual argument values.

Report:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-thread-bootstrap-report.json`.

The Windows process policy change, platform timer setup, original TLS-slot
allocation and other `10029410` subsystems are not
implemented by this plan. See `AVS_NATIVE_WORKERS.md` for real worker creation and
its remaining integration boundaries, and `AVS_TASKS.md` for the finite task-record
allocator, original admission, recycling and affinity release.
