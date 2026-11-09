# Art Asset Catalog

All art is original and generated at startup by `src/core/art.cpp`, from hand-drawn palette-letter templates plus procedural drawing. Exported review sheets are in `assets/gallery/` (regenerate with `zs_tool --gallery assets/gallery`).

| Sheet | Contents |
|---|---|
| `characters.png` | 16×24 characters: 8 survivor + 4 zombie palette variants × 5 stored directions (W/SW/NW mirrored) × 14 poses |
| `tiles.png` | 26 tile types × 4 variants (grass, asphalt, line, dirt, lino, wood, carpet, concrete, wall top/front, window/broken, bars, fence, extraction, garden, palisade, doors closed/open/locked/broken, cell door, gate, cracks) |
| `props_icons.png` | tent, barrel, sandbags, log pile, torch, traffic cone, ground decals (tuft, flower, rock, paper, debris, oil, crack, blood smear, leaves), location icons, desk, filing cabinet, locker, crate, medical cabinet, shelf, police car, wreck, trees, bench, cot, chair, plant, cooler, campfire ×3, radio table, generator ×2, workbench, lamp, blood ×3, bag, bedroll, UI icons |
| `items.png` | 29 inventory items |
| `buildings_portraits.png` | 6 buildings × (plot + 3 levels), 8 portraits |
| `assets/icons/` | 3DS icon 48×48, HOME banner 256×128 + banner sound, Windows .ico |

Audio is original too: 29 synthesised effects plus procedural ambience and music (`src/core/audio.cpp`).
