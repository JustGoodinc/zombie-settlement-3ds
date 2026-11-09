# Inventory Specification

- Backpacks: Small 6×4 (start), Medium 8×5, Large 10×6, upgraded by *using* the backpack item. At 16 px per cell the Large grid is 160×96 px, which fits beside a 140 px info panel on the 320×240 screen, so no scrolling or zoom is needed.
- Items occupy w×h cells and can rotate 90°. Overlap and out-of-bounds placement are rejected (`Grid::fits`).
- Stylus: press an item to pick it up, drag (the ghost follows immediately, green or red preview), release to place. Dropping onto one other item stacks it (same stackable type) or swaps the two if both fit.
- Buttons: D-pad cursor, A pick up/place, Y rotate, X use/equip, B cancel/back.
- Tabs: Inventory (grid), Equip, Items, Key (list views); Sort repacks largest-first and is left unchanged if it fails.
- Containers: Take, Take All (only what fits; the rest stays put and is never deleted), Store (backpack to container), Inspect. The settlement stash adds Donate (convert to resources).
- Drop: in the field, items go into a dropped bag you can pick up again. At the settlement they go to the stash.
- On extraction, food, materials, fuel and equipment are delivered automatically. Generator and Medical Scanner are installed. Ammo and medicine stay in the pack (donate at the stash if you want them as resources).
- Opening the inventory pauses everything (verified by an automated test).
