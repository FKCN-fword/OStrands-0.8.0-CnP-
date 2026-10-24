# P+ counter path and hybrid behavior

## P+ counter path
- Do not gate alias counter polling on `SLSO.esp`.
- Use `sslThreadController.GetOrgasmCount(actor)` when available.
- Otherwise use `sslActorAlias.GetOrgasmCount()` with no arguments on the scene's bound actor aliases.
- Do not overwrite any PEX file or add an ESP/ESL.

## Counter policy
- First successful sample establishes a baseline to avoid replaying P+ restored counts.
- Increase deltas are calculated. A multi-count jump is coalesced into at most one effect request per observed poll to avoid a burst and preserve the existing 10-second actor repeat guard.
- A decreasing counter resynchronizes the baseline without triggering an effect.
- A jump greater than 16 is resynchronized and logged without triggering a burst.
- Invalid Papyrus return values cause a retry backoff; timed-out alias rounds are ignored.
- No delayed queue is added. The existing FluidManager emitter check and 10-second repeat limit remain in effect.

Source contract only. This does not claim Windows compilation or in-game verification.
