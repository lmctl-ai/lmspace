# Shared SDK catalogs

The host registers the SDK string, collection and testing catalogs from the per-file SDK pin. The registry is the source for `scripts/gen-schema.py`; the generated schema therefore includes declarations supplied by headers as well as orbital methods. Native signature/compiler parity and embedded schema byte checks remain part of `test.sh`.

`assert_close` now uses the common absolute-tolerance contract: NaN and negative tolerances fail, equal infinities fail, and finite differences can pass against positive infinite tolerance. The stricter physics operation is retained as `space_assert_close`, which requires finite inputs and finite nonnegative tolerance before calling the shared SDK assertion helper. Existing finite physical calculations retain their previous meaning. Domain conversion, propagation and numerical validity checks remain separate.

`Host::collections` owns DSL-created lists/maps, with stable handles for one chunk and the shared 65,536-object cap. It is reset between chunks after previous handles expire. Physics object arenas remain unchanged. All five VM/catalog headers are pinned in `sdk.lock` and verified by `scripts/sync-sdk.sh --verify`.
