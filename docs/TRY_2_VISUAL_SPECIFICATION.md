# Try 2 Visual Specification

Try 2 is the approved look. The 3DS shell in the mockup is not rendered: the game draws only to the two native screens.

## Screens
- Top 400×240: gameplay world (3/4 top-down, 16 px tiles) plus the HUD: survivor plate with portrait, HP and stamina; location plate; weapon and ammo `12/36`; clock.
- Bottom 320×240: menus and inventory. Tabs on top, content panel, button-prompt footer at y=226.

## Palette (`src/core/gfx.h`, namespace `pal`)
| Role | RGB |
|---|---|
| Panel / panel2 / border | 38,38,43 · 48,48,54 · 92,92,100 |
| Text cream / dim | 226,216,196 · 150,144,132 |
| Selection gold / gold background | 232,192,64 · 64,56,34 |
| Danger red / blood | 176,48,48 · 120,20,20 |
| Container outline lime | 154,228,64 (searching: gold, searched: grey) |
| Health green / stamina cyan | 98,168,64 · 80,170,200 |

World: olive grass, dusty asphalt, lino checker, wood, carpet and concrete, all desaturated. Night tint 6,10,26; dusk tint 40,20,16.

## Grid and metrics
- Tiles 16×16. Characters 16×16 with the pivot at the feet (drawn at x−8, y−14).
- Inventory art uses 8 px per grid cell: drawn at 2× (16 px cells) in the backpack view and 1× in the compact container view (12 px cells).
- Portraits 24×24 (1× in the HUD, 2× in menus).
- Font: 5×8 bitmap, 6 px advance, 10 px line, 1 px drop shadow.
- Buttons 18–22 px tall; gold border plus an inner dark-gold line when focused.
- Nearest-neighbour scaling only.

## Comparison with the mockup
| Mockup panel | Implementation | Screenshot (`screenshots/auto/`) |
|---|---|---|
| Exploration & Combat | HUD matches (portrait, bars, location plate, ammo) | `15_combat_muzzle.png` |
| Inventory (grid) | Inventory/Equip/Items/Key/Sort tabs, grid + info, footer prompts, PAUSED banner | `14_inventory.png` |
| Looting (container) | slot row, Take All / Take, compact backpack | `12_container_menu.png` |
| Settlement overview | Base/Survivors/Jobs/Resources tabs, Lv list, building art | `03_settlement_base.png` |
| World map | location list + preview, loot, threat, cooldown, Deploy/Back | `06_world_map.png` |
| Survivor management | roster + portrait, Lv, job, health, skills | `05_survivors.png` |
| Build / upgrade | Build/Upgrade/Repair tabs, facility cards, costs | `04_build_menu.png` |
| Infirmary | patient, infection bar, Treat / Quarantine / Examine / Back | `23_infirmary.png` |

Known difference: mockup characters are larger and more detailed than 16 px allows. The sprites are deliberately simpler so they stay readable at native resolution.
