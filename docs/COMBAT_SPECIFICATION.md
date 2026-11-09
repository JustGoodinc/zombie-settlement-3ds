# Combat Specification

## Hit table (`src/gameplay/combat.cpp`)
Weights are per mille and always sum to exactly 1000 (the rounding remainder goes to Body). Baseline (skill 3, no bonus, ≤ 64 px): **Miss 300, Body 400, Arm 150, Leg 100, Head 50.**

Modifiers:
- Miss factor = 1 − 0.07·(skill−3) − acc/100 + 0.9·farFraction + 0.25 (moving) + 0.30 (injured, HP < 50) − 0.10 (target idle or lying), clamped to 0.25–2.6.
- Head factor = (1 + 0.12·(skill−3) + acc/200) × distance falloff (min 0.3) × 0.7 if moving.

Results:
- Miss: 0 damage.
- Body: full damage.
- Arm: 55% damage.
- Leg: 60% damage plus a 2.5 s stumble/slow.
- Head: instant kill.

## Aim assist
The survivor fires where they face. The nearest zombie inside the weapon's cone (9–18°) with line of sight is the candidate. The roll happens at the trigger. A hit fires a projectile that homes onto that target and applies exactly the rolled result on arrival. A miss fires a projectile visibly 5–9° wide that cannot hurt anything. With no candidate, the bullet flies straight; if it strikes a zombie, the roll is made with Miss excluded. **A hit effect is never shown for a rolled miss.** If the target dies first, the bullet flies on harmlessly.

## Weapons (`content.cpp`)
| Weapon | Dmg | Mag | Ammo | Durability | Notes |
|---|---|---|---|---|---|
| Pistol | 40 | 12 | 9mm | 400 | |
| Revolver | 60 | 6 | .38 | 500 | |
| Shotgun | 22×6 | 5 | shells | 300 | each pellet rolls separately |
| Hunting Rifle | 95 | 5 | rifle | 350 | |
| SMG | 28 | 30 | 9mm | 600 | automatic |
| Knife | 34 | – | – | ∞ | 8 stamina |
| Bat / Crowbar / Machete | 48 / 44 / 60 | – | – | 60 / 120 / 90 | 12–14 stamina |

Weapons never jam. At 0 durability a weapon cannot be used until it is repaired at the Workshop. Workshop upgrades (+8 accuracy, max 2) need Workshop Lv 2. Melee resolves on the swing's impact animation event within a 60° arc.

## Zombie attacks
Within 13 px, an attack lands on the animation's impact frame: 8–14 damage and a 12% chance of infection. A dodge in progress negates the hit ("DODGED").
