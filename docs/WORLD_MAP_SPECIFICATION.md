# World Map Specification

Locations: Police Station (**playable**), Hospital, Grocery Store, Train Yard, Scrapyard, Woods, Residential (listed, marked UNSCOUTED, Deploy disabled until their maps are built).

## Police Station (`src/world/maps.cpp`, 55×43 tiles)
Back lot with crates, evidence storage and armory behind keycard doors, locker room, break room, and holding cells with a trapped survivor behind cell doors. Then the main corridor, offices, chief's office (keycard desk), records, first aid, interrogation, reception lobby, sidewalk, parking lot with police cars and wrecks, and the road extraction zone.
- 67 containers: desks, filing cabinets, lockers, crates, medical cabinets, evidence shelves, car trunks.
- 19 zombies: idle, wandering and lying.
- 18 windows, 12 doors (2 keycard-locked), 3 cell doors.
- The Station Keycard is guaranteed in the chief's desk; the Cell Key in the cell-block guard desk (first generation).

## Loot refresh rule
The 15-minute timer starts the first time you **leave** a location after visiting it. It only counts active, unpaused play: never time with the game closed, never time spent in menus, and never while you are inside that location. When it expires, every container is restocked with new random contents (loot generation +1) and killed zombies return. Doors, windows, the rescue outcome and dropped bags persist. The timer then waits for your next departure.

## Extraction
Stand on the yellow-chevron tiles and press A. All living squad members return, and a following rescued survivor can be recruited. If the controlled survivor dies, control passes to a living companion. If everyone dies, the expedition fails.
