# Settlement Specification

Six designated plots, each with a top-down building that visibly grows with level (annex at Lv 2, trim/lantern/antenna at Lv 3; damage shows cracks and smoke). Costs are in `content.cpp`.

| Building | Effect |
|---|---|
| Infirmary | treat injuries and infection, screening, quarantine; level gates infection stage |
| Workshop | repair (Lv 1), upgrades (Lv 2), cheaper repairs (Lv 3) |
| Storage | stash capacity 16 / 24 / 32 items |
| Kitchen/Garden | baseline food + per-farmer output; generator +50% |
| Living Quarters | population cap 8 / 12 / 16 / 20, sleep |
| Defenses | walls, watchtower, reinforcement (+security) |

**Resources:** Food, Medicine, Ammo, Materials, Fuel.

**Daily tick at 06:00:**
- Farmers produce food and the population eats.
- Workers add materials.
- Mechanics repair a damaged building and the walls.
- Guards use 1 ammo each.
- Job skills gain XP.
- A stranger may arrive (35%) if there is room.
- The game autosaves when you are at the settlement.

**Security (0–100):** defense level ×12, plus per guard (12 + 3×guarding), ±8/10 for ammo, minus damage, scaled by wall condition. Large camps and known unquarantined infections lower it. Ratings: Safe ≥ 70, Guarded ≥ 45, Risky ≥ 25, DANGEROUS. A warning fires at 18:00 and Prep shows security after the squad leaves.

**Incidents (02:00):** chance = 0.30 − 0.0035·security + 0.004·population, clamped to 3–45%.
- Security ≥ 45: wall scuffs only.
- Security 25–45: wall, building damage, food loss, an injury.
- Below 25: casualties are possible.

Deaths are impossible at security ≥ 25 (tested over 200 nights). Every incident is written to the journal with its side, casualties and damage.
