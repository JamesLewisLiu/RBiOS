# Original AFP movie release order

## Call chain

Normal lifecycle recycling uses reset; destructor delete order is not a substitute.

CNoteBase vtable 0x1028DE34 +20 points to 0x101591D0. It resets ordinary-long at this+256, Switch-long at +260, then head at +264. Components also receive virtual+24 to become inactive. 0x10158D40 releases and clears the long components before calling its own virtual+20, preserving long-before-head ordering.

| Component | Original reset | Actual release order | Binding slots |
| --- | --- | --- | --- |
| CLongNoteGraphic | 0x1015BDE0 | actor+112 exp_loop, then actor+116 eff_long_tail | 0, 1 |
| CSwitchLongGraphic | 0x10156690 | All 16 bodies at +112..172, all caps at +176..236, all collection actors at +240..300, then tail background +304 | 0..15, 16..31, 32..47, 48; allocated entries only |
| CNoteGraphic | 0x10155890 | on +44, off +48, Bigbang +52, overlay +36, collection +40 | Current head on -1 then off -2; Bigbang is a separate single binding |

Actor reset is vtable 0x1028DC0C +20 = 0x100058D0. It calls pool release 0x10164BC0, then virtual+24. Pool release pushes the node onto the free-list head, hides the image, and clears the actor's pool/node references. The next acquire order is therefore the reverse of release order.

Ordinary-long creation at 0x1015C1A0 acquires tail +116 before exp_loop +112; reset reverses that order, although these use distinct pools. Switch creation at 0x101569E0 interleaves body/cap/collection per segment; reset groups complete arrays. Head is created first at 0x10158F20 and released after the long components.

## Presentation consequences

GamePresentation now follows component reset order, rather than binding creation order or a blanket reverse order. Local Switch caps share pools 23/24 with the head. Releasing caps first and head last makes the old head node the next acquired instance. Releasing in creation order would instead reuse the final cap's movie clock.

Bindings continue to record original creation order. Cross-note cleanup ordering within one tick needs a separate comparison against the original manager.

## Original-instruction validation

python -B tools/verify_afp_release_order.py passes 68 checks with 56 actual original pool releases.

The isolated fixture executes 0x101591D0, all three component resets, actor reset and pool release. Only renderer SetVisible is a recording stub; the stage actor list is empty and all optional components are populated. Final free lists equal the reverse observed release sequence. All actor node pointers clear and pool IDs become -1.

Report: .build/original-afp-release-order.json. No game process, Windows API, server or complete AFP renderer is invoked.
