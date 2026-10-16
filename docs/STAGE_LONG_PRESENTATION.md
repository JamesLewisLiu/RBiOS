# StageSession long-note integration

Updated 2026-11-05. StageSession owns ordinary hold and Switch presentation state and updates geometry at each 60Hz frame end. snapshot never advances motion or animation. Original instruction/asset evidence is in [LONG_NOTE_GRAPHIC](LONG_NOTE_GRAPHIC.md).

## API and order

SessionNoteView.longGraphic contains bitmap/movie draw descriptions and explicit special-effect requests. movieBindings lists every movie acquired at creation, including hidden instances. Acquiring only visible draws would break original lifetimes and clocks.

SessionOptions callbacks:

1. onNoteCreated(sourceId, bindings) runs after all state/bindings are initialized.
2. onNoteReleased(sourceId) runs at original frame-end expiry, before unlinking and inactive marking.
3. onPresentationFrame(snapshot) runs after every full logic tick, including every intermediate tick in a batched advance.

snapshot is read-only. StageSession destruction does not emit per-note release callbacks; its owner destroys the whole GamePresentation pool. Bindings retain creation order. Cleanup uses the separately recovered [AFP_RELEASE_ORDER](AFP_RELEASE_ORDER.md), not simple forward/reverse binding order.

## Complete bindings

Head slots are -1/-2. Long slots follow LongGraphicDraw.slot.

| Type | Original creation/binding order |
| --- | --- |
| Switch head | Pools 23/24 local or 25/26 opponent; slots-1/-2; both swich_on/off always exist |
| Bigbang head | Pool 31/32, slot-1 |
| Ordinary hold | Tail-effect pool 15/16 slot 1, then exp_loop pool 30 slot 0 |
| Switch segment i | Body pool 27/28 slot i, cap pool 24/23 slot 16+i, collection pool 33 slot 32+i |
| Switch final background | Pool 29, slot 48 |

0x10158F20 creates body components only for local side 0. No opponent body is invented. Switch retains the 16-segment bound. Ordinary body is hidden at reset 1015BDE0, then enabled by first holdEntry 1015A5E0; original tail/background initialization is retained.

Original unnormalized judgment reasons control hold effects through 10159E10: reason 3 with grade!=4 or reason 7 enables BF30; reason 8 disables BEE0. Score normalization does not turn reasons 7/8 into generic hold ticks. Effects expire with their owning bindings.

Ordinary endpoints use frame.endpointX/Y; Switch uses frame.trailX/Y and cumulative timers. Presentation updates before MotionTimeline::advance and finishNoteFrame. Graphic duration uses separately truncated extra time/duration values; judgment switchDurations remains unchanged.

The offline stage has no replay association, so collection stays explicitly false. Original collection bindings are still allocated. special/secondarySpecial retain extra.value bits 0/1.

## Catalog audit

StageLongPresentationTests with no arguments verifies the 16-segment fixture. Arguments are resource-root, limit, chart-side and local-mode.

tools/verify_stage_long_presentation.py covers 2,309 original charts x both sides x auto/no-contact:

| Metric | Count |
| --- | ---: |
| Complete runs | 9,236 |
| 60Hz snapshots | 62,563,101 |
| Long-note views | 101,938,733 |
| Long draws | 162,026,891 |
| Bindings | 955,576 |
| Create callbacks | 7,252,200 |
| Release callbacks | 7,252,200 |

Every visible movie resolves to its resource/package/slot binding. Hidden segment bindings exist, IDs pair correctly and all pools drain. Angles/scales/alpha stay finite; ordinary anchorY=0 and Switch anchorY=1 are checked.

Original charts include red/blue ordinary, Top holds, Switch and both player-side categories. No Top Switch occurs in these catalog configurations. Replay collection is absent and is not counted as shown.

The original maximum is 14 segments. An additional 16-segment fixture verifies 51 bindings, visible final body slot 15,1,151 batched callbacks and unchanged state when snapshot is called inside a callback. Independent original geometry also covers 1-16 segments. Catalog integer draw bounds are X=31..737,Y=204..1127 without forced safe-area clipping.

Reports: .build/stage-long-presentation-audit.json and four long-presentation-side*-mode*.json files. Post-integration numeric logs exactly match the prior baseline for both sides,4,618 rows each; .build/stage-long-numeric-parity.json records the comparison.

This stage-description audit does not execute the original AFP free list. Within-note cleanup has separate instruction evidence. Cross-note same-tick ordering, exhaustion, effect consumers, original pixels, ARM64 behavior and device synchronization remain separate work. No server, original game process, resource conversion or score persistence is involved.
