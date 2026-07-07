/**
 * Smoke test del substrate: garantiza que `npm test` (el --verify-cmd del harness)
 * está verde desde el commit 0. Cada fase añade sus propios tests junto a este.
 */
import { strict as assert } from "node:assert";
import { test } from "node:test";

test("el substrate del laboratorio está operativo", () => {
  assert.equal(1 + 1, 2);
});
