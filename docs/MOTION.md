# Original motion and field geometry

Motion restores typed field geometry, segment velocities, timelines and position integration. It does not overlay x86 pointers onto ARM64 objects. Source addresses and hash refer to [MIGRATION](MIGRATION.md).

## Original mapping

| Address | Native interface | Behavior and boundary |
| --- | --- | --- |
| 0x100A4C90 | FieldGeometry defaults | X=0..768, Y=80..1222, lines 204/1098, Top X=128/640 and Y=431/871; offsets default zero |
| 0x100A4C10 | x | Original selectors 0..7 and offsets; retain default handling |
| 0x100A4B90 | y | Original Y-down selectors, not SpriteKit coordinates |
| 0x100A51B0 | laneX | Six lanes; original offset branches |
| 0x100A5140 | laneY | Six lanes; original 80/70 constants |
| 0x100A50B0 | region | Original three-region boundaries |
| 0x100A53C0 | destination | Indices 0..34 including wide/side/default cases |
| 0x100A4D40 | verticalIntersection | Original 36-based boundary extension and float stores |
| 0x100A4DF0 | horizontalIntersection | Original horizontal intersection and float stores |
| 0x100A4E90 | firstBoundary | Earliest edge; left=0/right=1/top=2/bottom=3/none=4 |
| 0x100A41A0 / 0x100A4250 | Segment calculations | Original speed/segment branches and float intermediates |
| 0x100A42D0 | MotionTimeline::initialize | Route distances and segment/reflection time construction |
| 0x100A3EA0 | MotionTimeline::advance | Per-tick timeline, segment transitions and pauses |
| 0x100A3E40 | effectiveVelocity | Current segment velocity and original boundary cases |
| 0x1015A240 | MotionPosition::integrate | Original position update and float storage |

A tick reads effective velocity and integrates position before advancing the timeline. Keep this order when connecting state updates. MotionRoute stores explicit points; [ROUTE](ROUTE.md) restores chart-driven construction and redirection.

## Differential validation

tools/verify_motion.py compares original x86 instructions and native output by float bit pattern:

| Coverage | Cases |
| --- | ---: |
| X selectors | 8 |
| Y selectors | 8 |
| Destinations, valid/invalid, wide and side combinations | 342 |
| Lanes | 27 |
| Regions | 24 |
| Boundary rays, corners, zero velocity and seeded directions | 581 |
| Velocity timelines and up to 200 integration ticks | 100 |
| Total | 1,090 |

The timeline cases cover 0-3 intermediate points, special reflection point 5, zero-length segments, pause countdowns and one-time position suppression. Initial velocities, segment times, reflection state and final per-tick values match. Each original call has a 10,000-instruction bound and must return. Report: .build/motion-parity.json. Eleven host checks also pass.

## Evidence boundary

The original integration fixture sets object +427=1 to skip an external offscreen callback. This differential suite does not validate that callback, note deletion, JR decisions, AFP effects or rendering. The default field is compared; arbitrary runtime-adjusted geometry is not fully covered. Nonfinite adapter input is rejected rather than compared with undefined original behavior.

StageSession now connects the separately restored routes and note states. That full-song audit remains distinct from these isolated motion comparisons and from device validation.
