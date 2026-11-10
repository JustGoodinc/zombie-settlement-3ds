// Design handoff generator (included by zs_tool.cpp):  zs_tool --handoff OUT_DIR
// Drives the real game through every screen, exports every asset with transparency,
// and writes DESIGN_HANDOFF.md + index.html from the same data so docs never drift.
#include "../src/ui/ui.h"
#include <direct.h>

static void mkd(const char* p) { _mkdir(p); }

// RGBA PNG (stored deflate; the zip step recompresses) with nearest-neighbour scale.
static bool writePNGA(const char* path, const px* d, int w, int h, int scale, bool keyIsTransparent) {
    int W = w * scale, H = h * scale;
    static uint32_t tab[256]; if (!tab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t k = i; for (int j = 0; j < 8; j++) k = (k & 1) ? 0xEDB88320u ^ (k >> 1) : k >> 1; tab[i] = k; }
    auto crc = [&](const uint8_t* b, size_t n, uint32_t c) { for (size_t i = 0; i < n; i++) c = tab[(c ^ b[i]) & 255] ^ (c >> 8); return c; };
    auto be = [](uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; };
    FILE* f = fopen(path, "wb"); if (!f) return false;
    auto chunk = [&](const char* t, const uint8_t* dd, uint32_t n) { uint8_t hh[8]; be(hh, n); memcpy(hh + 4, t, 4); fwrite(hh, 1, 8, f); if (n) fwrite(dd, 1, n, f);
        uint32_t c = crc((const uint8_t*)t, 4, 0xFFFFFFFFu); c = crc(dd, n, c) ^ 0xFFFFFFFFu; uint8_t tt[4]; be(tt, c); fwrite(tt, 1, 4, f); };
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; fwrite(sig, 1, 8, f);
    uint8_t ih[13]; be(ih, W); be(ih + 4, H); ih[8] = 8; ih[9] = 6; ih[10] = ih[11] = ih[12] = 0; chunk("IHDR", ih, 13);
    size_t raw = (size_t)H * (W * 4 + 1); uint8_t* r = (uint8_t*)malloc(raw); size_t k = 0;
    for (int y = 0; y < H; y++) { r[k++] = 0; for (int x = 0; x < W; x++) { px c = d[(y / scale) * w + x / scale]; bool t = keyIsTransparent && c == KEYC;
        r[k++] = (uint8_t)(((c >> 11) * 255) / 31); r[k++] = (uint8_t)((((c >> 5) & 63) * 255) / 63); r[k++] = (uint8_t)(((c & 31) * 255) / 31); r[k++] = t ? 0 : 255; } }
    size_t blocks = (raw + 65534) / 65535; uint8_t* z = (uint8_t*)malloc(2 + raw + blocks * 5 + 4); size_t o = 0; z[o++] = 0x78; z[o++] = 1;
    uint32_t a = 1, b = 0; for (size_t i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    for (size_t p = 0; p < raw; p += 65535) { size_t n = raw - p > 65535 ? 65535 : raw - p; z[o++] = (p + n >= raw) ? 1 : 0; z[o++] = n & 255; z[o++] = n >> 8; z[o++] = ~n & 255; z[o++] = (~n >> 8) & 255; memcpy(z + o, r + p, n); o += n; }
    be(z + o, (b << 16) | a); o += 4; chunk("IDAT", z, (uint32_t)o); chunk("IEND", nullptr, 0);
    free(r); free(z); fclose(f); return true;
}
static void spritePNG(const char* dir, const char* name, const Sprite& s) {
    char p[512];
    snprintf(p, sizeof p, "%s/%s.png", dir, name); writePNGA(p, s.d, s.w, s.h, 1, true);
    snprintf(p, sizeof p, "%s/%s@4x.png", dir, name); writePNGA(p, s.d, s.w, s.h, 4, true);
}
static void slug(char* out, const char* in) { int j = 0; for (int i = 0; in[i] && j < 60; i++) { char c = in[i]; if (c >= 'A' && c <= 'Z') c += 32; out[j++] = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ? c : '_'; } out[j] = 0; }

// ---------------------------------------------------------------- screens
struct ShotInfo { char file[48]; char title[64]; char area[16]; char note[200]; };
static ShotInfo g_shots[96]; static int g_nshots = 0;
static const char* g_out;

static void cap(const char* file, const char* area, const char* title, const char* note) {
    char p[512];
    snprintf(p, sizeof p, "%s/screens/%s.png", g_out, file);   // composite like the PC window (top, 8 px gap, bottom)
    enum { W = TOP_W, H = TOP_H + 8 + BOT_H }; static px img[W * H];
    for (int i = 0; i < W * H; i++) img[i] = rgb(16, 16, 20);
    memcpy(img, topB, sizeof topB);
    for (int y = 0; y < BOT_H; y++) memcpy(img + (TOP_H + 8 + y) * W + 40, botB + y * BOT_W, BOT_W * sizeof(px));
    writePNGA(p, img, W, H, 2, false);
    snprintf(p, sizeof p, "%s/screens/native/%s_top.png", g_out, file); writePNGA(p, topB, TOP_W, TOP_H, 1, false);
    snprintf(p, sizeof p, "%s/screens/native/%s_bottom.png", g_out, file); writePNGA(p, botB, BOT_W, BOT_H, 1, false);
    ShotInfo& s = g_shots[g_nshots++];
    snprintf(s.file, sizeof s.file, "%s", file); snprintf(s.title, sizeof s.title, "%s", title);
    snprintf(s.area, sizeof s.area, "%s", area); snprintf(s.note, sizeof s.note, "%s", note);
}
static void tapC(int x, int y) { tap(x, y); }

static void captureScreens() {
    extern const char* g_keyName[12];
    static const char* PC[12] = {"E", "Shift", "R", "Space", "Q", "F", "Esc", "Tab", "^", "v", "<", ">"};
    for (int i = 0; i < 12; i++) g_keyName[i] = PC[i];
    g_inputStyle = 0;
    for (int s = 0; s < 3; s++) save_delete(s);
    G.set.tutorialDone = false; G.set.zoom = 200;
    // ---- front end
    G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); step(20);
    cap("01_title_main_menu", "Front end", "Title + Main Menu", "Animated title scene (top). Main menu (bottom): New Game, Continue, Load, Settings, Controls, Credits, Quit on PC. Continue/Load disable when no save exists.");
    tapC(31, 108); step(6);   // New Game (left rail)
    cap("02_new_game_slots", "Front end", "New Game: choose a slot", "Three save slots with day/time summary. Start/Delete per slot.");
    tapC(289, 36); step(6);   // Start (right rail)
    cap("03_tutorial_offer", "Front end", "Tutorial offer (modal)", "Confirm dialog shown on New Game while the tutorial has never been finished or skipped.");
    tapC(98, 151); step(30);   // Yes
    cap("04_tutorial_step1", "Tutorial", "Tutorial banner: step 1/10", "Top-screen banner: step counter, title, glyph of the control to use, wrapped hint, skip hint. Bottom HUD gains Skip Tutorial.");
    G.tut = 2; G.tutT = 1; step(4);
    cap("05_tutorial_highlight", "Tutorial", "Tutorial: pulsing target button", "Step 3 pulses the Expedition button gold until the world map is opened.");
    tut_skip(); G.toastT = 0;
    ui_open(PN_SETTINGS); step(8);
    cap("06_settings", "Front end", "Settings", "Volumes (segmented bars), day length, screen shake, FPS, camera zoom, window/fullscreen (PC), Controls, Replay Tutorial, Animation Test Room.");
    ui_open(PN_CONTROLS); step(6);
    cap("07_controls_3ds", "Controls", "Controls - 3DS buttons", "Glyph + action table. 3DS colours: A red, B yellow, X blue, Y green; pills for L/R/START/SELECT; D-pad and Circle Pad icons.");
    g_inputStyle = 1; step(2);
    cap("08_controls_keyboard", "Controls", "Controls - keyboard", "Same table with keycaps showing the player's bound keys (from controls.ini).");
    g_inputStyle = 2; step(2);
    cap("09_controls_xbox", "Controls", "Controls - Xbox pad", "Shown automatically once a gamepad is used.");
    g_inputStyle = 0;
    ui_open(PN_CREDITS); step(6);
    cap("10_credits", "Front end", "Credits", "");
    // ---- settlement
    ui_open(PN_HUD); G.clock = 10 * 60; step(30);
    cap("11_settlement_day", "Settlement", "Settlement HUD - day", "Top: player at the campfire, cabins, tents, watchtower, settlers at work. Bottom: resources, population/security, minimap with buildings, objective, Base/Survivors/Expedition/Journal/Inventory/Pause.");
    G.clock = 19 * 60; step(10);
    cap("12_settlement_dusk", "Settlement", "Settlement - dusk", "Amber grade as daylight fades.");
    G.clock = 23 * 60; step(20);
    cap("13_settlement_night", "Settlement", "Settlement - night", "Blue night grade; campfire and torches are light sources; settlers sleep on bedrolls, guards stay up.");
    G.clock = 10 * 60; step(4);
    g_inputStyle = 1; step(2); cap("14_hud_keyboard", "Controls", "HUD footer - keyboard", "Footer prompts follow the active device."); g_inputStyle = 2; step(2);
    cap("15_hud_xbox", "Controls", "HUD footer - Xbox", ""); g_inputStyle = 0;
    ui_open(PN_BASE); step(8);
    cap("16_base_buildings", "Settlement", "Base - Buildings tab", "Building list with mini building art and level; detail card with large art, description, Manage.");
    tapC(31, 140); step(6);
    cap("17_base_jobs", "Settlement", "Base - Jobs tab", "Per-survivor job cycling with skill hints; live security score.");
    tapC(31, 196); step(6);
    cap("18_base_resources", "Settlement", "Base - Resources tab", "Resources, food forecast, population, security bar, walls, generator/scanner, Journal and Stash shortcuts.");
    for (int t = 0; t < 3; t++) {
        g_buildingSel = t == 0 ? B_WORKSHOP : B_INFIRMARY;
        ui_open(PN_BUILDING); if (t) { tapC(31, 36 + t * 72); } step(6);
        static const char* N[3] = {"19_build_tab_build", "20_build_tab_upgrade", "21_build_tab_repair"};
        static const char* T[3] = {"Build menu - Build", "Build menu - Upgrade", "Build menu - Repair"};
        cap(N[t], "Settlement", T[t], "Six facility cards (art, level, damage), detail panel with cost icons (red when unaffordable), perk text, primary action, facility-specific action.");
    }
    g_buildingSel = B_QUARTERS; ui_open(PN_BUILDING); step(6);
    cap("22_build_quarters", "Settlement", "Living Quarters (sleep/rest)", "Facility actions: Sleep until morning, Rest 2h.");
    ui_open(PN_SURVIVORS); step(6);
    cap("23_survivors", "Survivors", "Survivors - roster", "Roster with portraits, detail with 2x portrait, level/job/trait/status, health and six skill bars, Set Leader/Backpack/Quarantine.");
    tapC(31, 84); step(6);
    cap("24_survivors_squad", "Survivors", "Survivors - Squad tab", "");
    G.st.surv[4].alive = false; G.st.surv[4].deathDay = 3; tapC(31, 196); step(6);
    cap("25_survivors_status_memorial", "Survivors", "Survivors - Status / memorial", "Deceased survivors stay listed with day of death.");
    G.st.surv[4].alive = true;
    G.st.surv[2].infection = INF_ADVANCED; G.st.surv[2].infKnown = true; G.st.surv[2].hp = 40;
    ui_open(PN_INFIRMARY); step(4); tapC(110, 45); step(4);
    cap("26_infirmary", "Settlement", "Infirmary - treatment", "Patient list with status colour, portrait, infection progress bar, treat cost, quarantine, examine.");
    G.st.surv[2].infection = INF_NONE; G.st.surv[2].hp = 100;
    ui_open(PN_WORKSHOP); step(6);
    cap("27_workshop", "Settlement", "Workshop - weapons", "Every weapon in packs and stash with durability bars; repair and upgrade costs.");
    G.st.stash.cap = 16; G.st.stash.n = 0; G.st.stash.push(makeItem(IT_SHOTGUN)); G.st.stash.push(makeItem(IT_CANNED, 3)); G.st.stash.push(makeItem(IT_MEDKIT));
    g_contProp = -2; g_contBag = -1; ui_open(PN_CONTAINER); step(6);
    cap("28_stash", "Settlement", "Storage stash", "Container screen in stash mode: slots, Take All/Take/Store/Donate (convert to resources), compact backpack grid.");
    { Rng r(3); G.st.visitor = make_survivor(r, true); } G.st.visitorPresent = true;
    ui_open(PN_RECRUIT); step(6);
    cap("29_recruit", "Settlement", "Recruit stranger", "Profile with portrait, trait, specialty, skill bars, screening status; Accept / Reject / Screen / Accept into Quarantine / Later.");
    G.st.visitorPresent = false;
    journal(JK_WARN, "Zombies broke through the eastern fence. 3 food lost, the Workshop was damaged.");
    journal(JK_DEATH, "Riley died: killed when the walls were breached.");
    ui_open(PN_JOURNAL); step(6);
    cap("30_journal", "Settlement", "Incident journal", "Paged history; colour by kind (info, good, warning, death with skull).");
    ui_open(PN_INVENTORY); step(6);
    cap("31_inventory_grid", "Inventory", "Inventory - grid", "6x4 starter backpack at 24 px cells; equipped badge, stack counts, durability strips; info panel with Use/Equip, Rotate, To Stash/Drop.");
    tapC(100, 17); step(4);
    cap("32_inventory_selected", "Inventory", "Inventory - item selected", "Selected item: gold frame, stats and actions.");
    tapC(31, 66); step(4); cap("33_inventory_equip", "Inventory", "Inventory - Equip tab", "");
    tapC(31, 110); step(4); cap("34_inventory_items", "Inventory", "Inventory - Items tab", "");
    ui_open(PN_PAUSE); step(6);
    cap("35_pause_settlement", "Front end", "Pause - settlement", "Save is only available at the settlement.");
    ui_open(PN_SAVELOAD); step(6);
    cap("36_save_load", "Front end", "Save / Load", "");
    ui_open(PN_WORLDMAP); step(6);
    cap("37_world_map", "Expedition", "World map - Police Station", "Location list with icons, map preview, loot, threat, restock timer, Deploy.");
    tapC(31, 48); step(4);
    cap("38_world_map_unscouted", "Expedition", "World map - unscouted location", "Unscouted locations are visibly disabled; Deploy greys out.");
    tapC(31, 16); step(2); tapC(289, 36); step(6);
    G.squad[1] = 4; step(2);
    cap("39_expedition_prep", "Expedition", "Expedition preparation", "Leader picker, equipment summary, companions (max 2), security-while-away warning, Deploy.");
    tapC(289, 36); step(40);
    // ---- expedition
    cap("40_police_arrival", "Expedition", "Police Station - arrival", "Parking lot, POLICE sign with strobes, cars, cones, extraction chevrons. Bottom HUD: explored-only minimap, objective, glyph hints.");
    Unit& u = G.u[G.ctrl];
    u.x = u.px = 26 * 16 + 8.0f; u.y = u.py = 29 * 16 + 8.0f; u.fy = -1; u.fx = 0; u.dir = 6;
    for (int i = 0; i < G.nu; i++) if (i != G.ctrl && !G.u[i].escort) { G.u[i].x = G.u[i].px = u.x - 14; G.u[i].y = G.u[i].py = u.y + 10; }
    G.nz = 0; step(20);
    cap("41_police_lobby", "Expedition", "Police Station - lobby interior", "Interior lighting, wall ambient occlusion, desks, benches, litter decals.");
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < G.map.nprop; i++) { const Prop& p = G.map.prop[i]; if (p.container < 0) continue; float dx = p.tx * 16 + 8 - u.x, dy = p.ty * 16 + 8 - u.y; if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; } }
    const Prop& cp = G.map.prop[best]; u.x = u.px = cp.tx * 16 + 8.0f; u.y = u.py = cp.ty * 16 + 30.0f; u.fx = 0; u.fy = -1; u.dir = 6; step(8);
    cap("42_container_highlight", "Expedition", "Container in reach", "Lime pulsing outline + A prompt plate (drawn after zoom, stays sharp).");
    press(BTN_A); step(16);
    cap("43_searching", "Expedition", "Searching", "Gold outline + progress bar; the survivor crouches; moving cancels.");
    step(40);
    cap("44_container_menu", "Inventory", "Container (loot) menu", "Slots row, Take All (pulses during tutorial), Take, Store, Inspect, compact backpack. World paused.");
    ui_open(PN_HUD); step(4);
    G.nz = 0;
    for (int k = 0; k < 4; k++) {
        Zombie& z = G.z[G.nz++]; memset(&z, 0, sizeof z);
        z.x = z.px = u.x + 40 + k * 14; z.y = z.py = u.y + 6 - k * 10; z.hp = 100; z.state = ZS_CHASE; z.spawnIdx = -1; z.speed = 1; z.variant = (uint8_t)k; z.hx = -1;
        z.anim = Animator(); z.anim.play(AnimState::ZWalk, true);
    }
    u.fx = 1; u.fy = 0; u.dir = 0;
    Survivor& s = G.st.surv[u.sv]; if (s.equip >= 0) { s.pack.it[s.equip].mag = 12; s.pack.it[s.equip].dur = 300; }
    press(BTN_Y); step(2);
    cap("45_combat_muzzle", "Combat", "Combat - firing", "Muzzle flash, recoil kick, ejected casing, tracer toward the aim-assisted target.");
    dbg_zombieHit(0, HR_HEAD, 1); step(3);
    cap("46_combat_headshot", "Combat", "Combat - headshot", "HEADSHOT label, blood burst, zombie death animation (instant kill).");
    G.z[1].x = u.x + 10; G.z[1].y = u.y; step(30);
    cap("47_combat_close", "Combat", "Combat - zombie attack", "Zombies lunge and bite on their impact frame; hurt flash and knockback; possible BITTEN! infection.");
    s.hp = 100;
    if (s.equip >= 0) s.pack.it[s.equip].mag = 0;
    press(BTN_X); step(8);
    cap("48_reload", "Combat", "Reloading", "RELOADING label and crouched reload pose; dodging cancels without losing ammo.");
    G.nz = 0; step(4);
    G.clock = 23 * 60; step(10);
    cap("49_night_flashlight", "Atmosphere", "Night with flashlight", "Flashlight cone (smoothly rotates), personal light radius, line-of-sight darkness; zombies outside light are hidden.");
    G.flashlight = false; step(10);
    cap("50_night_no_light", "Atmosphere", "Night without flashlight", "");
    G.flashlight = true; G.clock = 12 * 60;
    for (int i = 0; i < G.nu; i++) if (G.u[i].escort) {
        u.x = u.px = G.u[i].x; u.y = u.py = G.u[i].y + 36; u.fx = 0; u.fy = -1; u.dir = 6;
        for (int b = 0; b < G.map.nbar; b++) if (G.map.bar[b].kind == BK_CELLDOOR) G.map.bar[b].state = BS_OPEN;
        map_refresh(G.map);
    }
    step(20);
    cap("51_rescue", "Expedition", "Trapped survivor (rescue)", "HELP! bubble behind the opened cell door; A to rescue, then escort to extraction.");
    const uint8_t Z[3] = {100, 150, 200};
    static const char* ZN[3] = {"52_zoom_100", "53_zoom_150", "54_zoom_200"};
    for (int i = 0; i < 3; i++) { G.set.zoom = Z[i]; step(4); char t[32]; snprintf(t, sizeof t, "Camera zoom %d%%", Z[i]); cap(ZN[i], "Atmosphere", t, "Settings > Camera zoom. Default is 125%; HUD and text stay native resolution."); }
    G.set.zoom = 125;
    ui_open(PN_SQUAD); step(6);
    cap("55_squad_orders", "Expedition", "Squad orders", "Follow / Hold / Retreat per companion.");
    ui_open(PN_PAUSE); step(6);
    cap("56_pause_expedition", "Front end", "Pause - expedition", "PAUSED banner on the dimmed top screen; Save disabled away from the settlement.");
    ui_open(PN_HUD);
    u.x = u.px = G.map.exitX * 16 + 8.0f; u.y = u.py = G.map.exitY * 16 + 8.0f; step(4);
    cap("57_extraction_prompt", "Expedition", "Extraction zone", "Yellow chevrons blink green; A to extract.");
    press(BTN_A); step(10);
    cap("58_extraction_summary", "Expedition", "Extraction summary", "Results, deaths, delivered resources, rescued survivor offer, Return.");
    ui_open(PN_DEATH); step(6);
    cap("59_survivor_lost", "Front end", "Survivor lost", "Shown when the controlled survivor dies at the settlement.");
    G.debugAnim = 0; ui_open(PN_ANIMTEST); step(20);
    cap("60_anim_test_room", "Animation", "Animation test room", "All 8 directions + 4x zoom of the selected state, frame/event readout.");
    ui_open(PN_HUD); step(2);
}

// ---------------------------------------------------------------- assets
static const char* SPR_NAMES[S_COUNT] = {
    "desk", "filing_cabinet", "locker", "crate", "medical_cabinet", "shelf", "police_car", "tree_a", "tree_b", "bench", "cot",
    "chair", "plant", "water_cooler", "campfire_f0", "campfire_f1", "campfire_f2", "radio_table", "generator_f0", "generator_f1",
    "workbench", "lamp_post", "blood_0", "blood_1", "blood_2", "dropped_bag", "bedroll", "wrecked_car",
    "icon_food", "icon_medicine", "icon_ammo", "icon_materials", "icon_fuel", "icon_shield", "icon_skull",
    "icon_biohazard", "icon_people", "icon_clock", "logo_zombie",
    "tent", "barrel", "sandbags", "log_pile", "torch_f0", "torch_f1", "traffic_cone",
    "decal_tuft", "decal_flower", "decal_rock", "decal_paper", "decal_debris", "decal_oil", "decal_crack", "decal_blood_smear", "decal_leaves",
    "icon_house", "icon_tree", "icon_train"};
static const char* TILE_NAMES[TA_COUNT] = {"grass", "asphalt", "parking_line", "dirt", "lino", "wood", "carpet", "concrete", "wall_top", "wall_front",
    "window", "window_broken", "cell_bars", "fence", "extraction", "garden", "palisade", "door_closed", "door_open", "door_locked", "door_broken",
    "cell_door", "gate_closed", "gate_open", "crack_overlay_1", "crack_overlay_2"};
static const char* POSE_NAMES[P_COUNT] = {"stand", "breathe", "walk_a", "walk_b", "run_a", "run_b", "crouch", "reach", "lean_back", "lean_fwd", "hurt", "fall", "dead", "look", "aim"};
static const char* DIR_NAMES[8] = {"e", "se", "s", "sw", "w", "nw", "n", "ne"};
static const char* BLD_NAMES[BLD_TYPES] = {"infirmary", "workshop", "storage", "kitchen_garden", "living_quarters", "defenses"};

static int exportAssets() {
    char d[512], p[512], nm[80]; int n = 0;
    snprintf(d, sizeof d, "%s/assets", g_out); mkd(d);
    const char* sub[] = {"props_icons", "tiles", "items", "characters", "portraits", "buildings", "ui", "audio", "app_icons", "sheets"};
    for (const char* s : sub) { snprintf(p, sizeof p, "%s/%s", d, s); mkd(p); }
    for (int i = 0; i < S_COUNT; i++) if (g_spr[i].d) { snprintf(p, sizeof p, "%s/props_icons", d); spritePNG(p, SPR_NAMES[i], g_spr[i]); n++; }
    for (int t = 0; t < TA_COUNT; t++) for (int v = 0; v < 4; v++) { snprintf(p, sizeof p, "%s/tiles", d); snprintf(nm, sizeof nm, "%s_v%d", TILE_NAMES[t], v); spritePNG(p, nm, g_tile[t][v]); n++; }
    for (int i = 1; i < IT_COUNT; i++) { char s2[64]; slug(s2, idef(i).name); snprintf(p, sizeof p, "%s/items", d); snprintf(nm, sizeof nm, "%02d_%s", i, s2); spritePNG(p, nm, g_item[i]); n++; }
    for (int z = 0; z < 2; z++) for (int v = 0; v < (z ? ZOMB_VARIANTS : SURV_VARIANTS); v++) {
        snprintf(p, sizeof p, "%s/characters/%s_%d", d, z ? "zombie" : "survivor", v); mkd(p);
        for (int dir = 0; dir < 8; dir++) for (int ps = 0; ps < P_COUNT; ps++) {
            bool flip; const Sprite& s = art_char(z != 0, v, dir, (Pose)ps, flip);
            Sprite f = s;
            if (flip) { f = spr_new(s.w, s.h); for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++) f.d[y * s.w + x] = s.d[y * s.w + (s.w - 1 - x)]; }
            snprintf(nm, sizeof nm, "%s_%s", DIR_NAMES[dir], POSE_NAMES[ps]); spritePNG(p, nm, f); n++;
            if (flip) free(f.d);
        }
    }
    for (int i = 0; i < PORTRAITS; i++) { snprintf(p, sizeof p, "%s/portraits", d); snprintf(nm, sizeof nm, "portrait_%d", i); spritePNG(p, nm, g_portrait[i]); n++; }
    for (int b = 0; b < BLD_TYPES; b++) for (int l = 0; l < BLD_LEVELS; l++) { snprintf(p, sizeof p, "%s/buildings", d); snprintf(nm, sizeof nm, "%s_%s", BLD_NAMES[b], l ? (l == 1 ? "lv1" : (l == 2 ? "lv2" : "lv3")) : "plot"); spritePNG(p, nm, g_building[b][l]); n++; }
    // UI components rendered by the real widget code
    {
        static px buf[320 * 360]; Canvas c; c.init(buf, 320, 360);
        gfx_clear(c, rgb(26, 26, 30));   // opaque backdrop: panel shadows are translucent
        ui_beginFrame(Input()); g_ui.focus = 99;
        ui_panel(c, 4, 4, 150, 70, "Panel Title"); ui_panel(c, 164, 4, 150, 70);
        ui_button(c, 8, 84, 96, 20, "Normal"); g_ui.focus = g_ui.count; ui_button(c, 112, 84, 96, 20, "Focused"); g_ui.focus = 99; ui_button(c, 216, 84, 96, 20, "Selected", UB_SELECTED);
        ui_button(c, 8, 110, 96, 20, "Gold", UB_GOLD); ui_button(c, 112, 110, 96, 20, "Disabled", 0, false); ui_button(c, 216, 110, 96, 20, "Left aligned", UB_LEFT);
        ui_tab(c, 8, 140, 74, "Active", true); ui_tab(c, 86, 140, 74, "Inactive", false);
        ui_bar(c, 8, 164, 140, 8, 0.7f, pal::green); ui_bar(c, 8, 176, 140, 6, 0.45f, pal::cyan); ui_bar(c, 8, 186, 140, 6, 0.25f, pal::red); ui_bar(c, 8, 196, 140, 8, 0.6f, pal::gold);
        const uint32_t B[] = {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_L, BTN_R, BTN_START, BTN_SELECT, BTN_UP, BTN_DPAD_LR, BTN_STICK, BTN_TOUCH};
        for (int style = 0; style < 3; style++) { g_inputStyle = style; int x = 8; for (uint32_t b : B) x += ui_btn(c, x, 214 + style * 16, b) + 4; }
        g_inputStyle = 0;
        ui_prompt(c, 8, 266, 'A', "Interact"); ui_prompt(c, 90, 266, 'Y', "Fire");
        ui_endFrame();
        snprintf(p, sizeof p, "%s/ui/components.png", d); writePNGA(p, buf, 320, 360, 1, false);
        snprintf(p, sizeof p, "%s/ui/components@3x.png", d); writePNGA(p, buf, 320, 360, 3, false); n += 2;
        static px bg[BOT_W * BOT_H]; Canvas cb; cb.init(bg, BOT_W, BOT_H); ui_bg(cb);
        snprintf(p, sizeof p, "%s/ui/bottom_screen_background.png", d); writePNGA(p, bg, BOT_W, BOT_H, 1, false); n++;
    }
    // font sheets (regular + bold) and palette swatches
    {
        static px buf[256 * 80]; Canvas c; c.init(buf, 256, 80); gfx_clear(c, pal::panel);
        char line[33];
        for (int r = 0; r < 3; r++) { for (int i = 0; i < 32; i++) { int ch = 32 + r * 32 + i; line[i] = (char)(ch > 126 ? ' ' : ch); } line[32] = 0; gfx_text(c, 2, 2 + r * 12, line, pal::cream, 1, false); }
        gfx_textB(c, 2, 42, "BOLD: Staying Alive 0123", pal::cream); gfx_text(c, 2, 56, "Regular with shadow: Scavenge.", pal::cream); gfx_text(c, 2, 68, "x2", pal::gold, 2);
        snprintf(p, sizeof p, "%s/ui/font_sheet@3x.png", d); writePNGA(p, buf, 256, 80, 3, false); n++;
    }
    {
        static px buf[320 * 72]; Canvas c; c.init(buf, 320, 72); gfx_clear(c, pal::black);
        const px P[] = {pal::black, pal::panel, pal::panel2, pal::panelHi, pal::border, pal::cream, pal::dim, pal::grey, pal::gold, pal::goldDk, pal::goldBg, pal::red, pal::redDk, pal::blood, pal::green, pal::lime, pal::cyan, pal::olive, pal::brown, pal::tan};
        for (int i = 0; i < 20; i++) gfx_fill(c, (i % 8) * 40, (i / 8) * 24, 40, 24, P[i]);
        snprintf(p, sizeof p, "%s/ui/palette_strip.png", d); writePNGA(p, buf, 320, 72, 2, false); n++;
    }
    snprintf(p, sizeof p, "%s/audio", d); n += audio_exportWavs(p);
    snprintf(p, sizeof p, "%s/sheets", d); gallery(p); n += 5;
    // copy app icon + banner
    const char* ico[] = {"icon.png", "banner.png", "banner.wav", "icon.ico"};
    for (const char* f : ico) {
        char src[256], dst[512]; snprintf(src, sizeof src, "assets/icons/%s", f); snprintf(dst, sizeof dst, "%s/app_icons/%s", d, f);
        FILE* a = fopen(src, "rb"); if (!a) continue; FILE* b = fopen(dst, "wb"); char buf2[4096]; size_t k; while ((k = fread(buf2, 1, sizeof buf2, a)) > 0) fwrite(buf2, 1, k, b); fclose(a); fclose(b); n++;
    }
    return n;
}

// ---------------------------------------------------------------- documents
struct Swatch { const char* name; px c; const char* use; };
static const Swatch SW[] = {
    {"black", pal::black, "World clear colour"}, {"ink", pal::ink, "Outlines, text shadow, panel edge"},
    {"panel", pal::panel, "Panel fill"}, {"panel2", pal::panel2, "Title bar / raised fill"}, {"border", pal::border, "Neutral border"},
    {"cream", pal::cream, "Primary text"}, {"dim", pal::dim, "Secondary text"}, {"grey", pal::grey, "Disabled text"},
    {"gold", pal::gold, "Selection, focus, active tab"}, {"goldDk", pal::goldDk, "Focus inner line"}, {"goldBg", pal::goldBg, "Selected fill"},
    {"red", pal::red, "Danger, damage, low HP"}, {"redDk", pal::redDk, "PAUSED banner"}, {"blood", pal::blood, "Blood"},
    {"green", pal::green, "Health"}, {"lime", pal::lime, "Container outline, extraction"}, {"cyan", pal::cyan, "Stamina, night clock"},
    {"olive", pal::olive, "Foliage"}, {"brown", pal::brown, "Wood"}, {"tan", pal::tan, "Crates, planks"}};
static void hexOf(px c, char* out) { snprintf(out, 8, "#%02X%02X%02X", ((c >> 11) * 255) / 31, (((c >> 5) & 63) * 255) / 63, ((c & 31) * 255) / 31); }

static void writeDocs(int assetCount) {
    char p[512]; snprintf(p, sizeof p, "%s/DESIGN_HANDOFF.md", g_out);
    FILE* m = fopen(p, "w");
    fprintf(m, "# Staying Alive - Design Handoff\n\nVersion 0.2.0. Every image in this package was rendered by the shipping game code (`zs_tool --handoff`), so it matches the build exactly.\n\n");
    fprintf(m, "Open `index.html` for the visual version of this document.\n\n");
    fprintf(m, "## 1. Platform and canvas\n\n| Screen | Size | Role |\n|---|---|---|\n| Top | 400 x 240 | Gameplay world + HUD (3/4 top-down) |\n| Bottom | 320 x 240 | Touch menus, inventory, maps |\n\n");
    fprintf(m, "- PC shows both screens stacked (8 px gap, bottom centred), integer-scaled, mouse = stylus.\n- Pixel art is 1:1 on device; nearest-neighbour scaling only. Camera zoom (100/125/150/200%%, default 125%%) scales the world layer only; HUD and text render after scaling at native resolution.\n- Screenshots in `screens/` are composites at 2x; `screens/native/` has each screen at 1x.\n\n");
    fprintf(m, "## 2. Colour\n\nRGB565 on device (5/6/5 bits), so values are quantised. Hex below is the exact rendered value.\n\n| Token | Hex | Use |\n|---|---|---|\n");
    for (auto& s : SW) { char h[8]; hexOf(s.c, h); fprintf(m, "| `%s` | `%s` | %s |\n", s.name, h, s.use); }
    fprintf(m, "\nAtmosphere grade (mixed toward by the light pass): day `#1E160E` warm, dusk `#2C160E` amber, night `#060A1A` blue; plus a screen-edge vignette.\n\n");
    fprintf(m, "## 3. Typography\n\n- Bitmap font 5x8 glyphs, 6 px advance, 10 px line height, 1 px `ink` drop shadow (`assets/ui/font_sheet@3x.png`).\n- Bold variant: glyph drawn twice, 7 px advance. Used for panel titles, tabs and buttons (falls back to regular when the label would not fit).\n- Large titles: same font at 2x-5x.\n- Minimum: never below 1x on device. Wrap at word boundaries; avoid lines over ~50 characters on the bottom screen.\n\n");
    fprintf(m, "## 4. Grid, spacing, components\n\n- World tiles 16 px. Characters 16x24 (feet pivot). Props 16x16; cars 32x16; tents 32x24; trees 32x32; buildings 48x48 on 3x3-tile plots.\n- Bottom screen: 4 px outer margin; panels have 3 px bevel + rivets; panel title bar 13 px (content starts at y+20); footer strip y 224-240.\n- Buttons: 18-22 px tall, 1 px ink edge, vertical gradient, light top-left / dark bottom-right bevel. States: normal, focused (gold border + dark-gold inner line), selected (gold fill), gold (call to action), disabled (flat, grey text), pressed (1 px down, inverted gradient).\n- Tabs: 15 px tall; active = gold gradient + gold top highlight + bright label.\n- Bars: 1 px ink frame, highlight row, 6 px segment ticks.\n- Inventory: 8 px of art per grid cell, drawn at 3x (24 px cells) for the 6x4 pack, 2x (16 px) for larger packs, 1x (12 px cells) in the loot screen.\n- See `assets/ui/components@3x.png`.\n\n");
    fprintf(m, "## 5. Input glyphs\n\nPrompts follow the active device: 3DS buttons (A red, B yellow, X blue, Y green; L/R/START/SELECT pills; D-pad + Circle Pad icons), keyboard keycaps using the player's bindings, Xbox labels when a pad is used. Rows 1-3 of `assets/ui/components@3x.png` show all three.\n\n");
    fprintf(m, "## 6. Characters and animation\n\n- 8 survivor palettes, 4 zombie palettes; 5 stored directions (W/SW/NW mirrored); 14 poses each: %s", "stand, breathe, walk A/B, run A/B, crouch, reach, lean back/forward, hurt, fall, dead, look.\n");
    fprintf(m, "- Animation states and timings: `docs/ANIMATION_BIBLE.md` in the repo. Exported frames: `assets/characters/<set>/<dir>_<pose>.png` (+ `@4x`).\n\n");
    fprintf(m, "## 7. Screens (%d)\n\n", g_nshots);
    const char* lastArea = "";
    for (int i = 0; i < g_nshots; i++) {
        const ShotInfo& s = g_shots[i];
        if (strcmp(lastArea, s.area)) { fprintf(m, "\n### %s\n\n", s.area); lastArea = s.area; }
        fprintf(m, "**%s** - `screens/%s.png`  \n%s\n\n", s.title, s.file, s.note[0] ? s.note : "");
    }
    fprintf(m, "## 8. Asset package (%d files)\n\n| Folder | Contents |\n|---|---|\n", assetCount);
    fprintf(m, "| `assets/props_icons/` | props, decals, UI icons (transparent PNG, 1x and @4x) |\n| `assets/tiles/` | 26 tile types x 4 variants |\n| `assets/items/` | inventory item art (8 px per grid cell) |\n");
    fprintf(m, "| `assets/characters/` | every survivor/zombie palette x 8 directions x 14 poses |\n| `assets/portraits/` | 24x24 portraits |\n| `assets/buildings/` | 6 buildings x plot + 3 levels |\n");
    fprintf(m, "| `assets/ui/` | components sheet, bottom-screen background, font sheet, palette strip |\n| `assets/audio/` | 29 effects + 4 ambience/music beds (WAV 22.05 kHz mono) |\n| `assets/sheets/` | review sheets (characters, tiles, props, items, buildings) |\n| `assets/app_icons/` | 3DS icon/banner, Windows .ico |\n\n");
    fprintf(m, "## 9. Open design work\n\n- Six world-map locations still need layouts and environment art (Hospital, Grocery, Train Yard, Scrapyard, Woods, Residential).\n- The Police Station facade is one 16 px wall row with a sign; a taller facade treatment would match the mockup better.\n- 45-degree character directions reuse front/back art with a turned head.\n- Night lighting uses 8 px cells (stepped edges).\n- All art is procedural/original; replacing any sprite only needs a same-size PNG mapped back into `src/core/art.cpp`.\n");
    fclose(m);

    // index.html: same content, visual
    snprintf(p, sizeof p, "%s/index.html", g_out);
    FILE* h = fopen(p, "w");
    fprintf(h, "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>Staying Alive - Design Handoff</title><style>"
               "body{margin:0;background:#141416;color:#e2d8c4;font:15px/1.5 system-ui,sans-serif}main{max-width:1180px;margin:0 auto;padding:32px}"
               "h1{color:#e8c040;font-size:28px;margin:0 0 4px}h2{border-bottom:1px solid #5c5c64;padding-bottom:6px;margin-top:40px;color:#e8c040}h3{color:#b9b09f;margin-top:28px}"
               "img{image-rendering:pixelated}code{background:#26262b;padding:1px 5px;border-radius:3px}table{border-collapse:collapse}td,th{border:1px solid #3a3a40;padding:4px 10px;text-align:left}"
               ".sw{display:inline-block;width:28px;height:18px;vertical-align:middle;border:1px solid #000;margin-right:8px}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(360px,1fr));gap:22px}"
               ".card{background:#1e1e22;border:1px solid #3a3a40;border-radius:6px;padding:10px}.card img{width:100%%;display:block;border-radius:3px}.card b{display:block;margin-top:8px;color:#e8c040}.card p{margin:4px 0 0;color:#b9b09f;font-size:13px}"
               ".big{max-width:100%%;border:1px solid #3a3a40}</style></head><body><main>");
    fprintf(h, "<h1>Staying Alive - Design Handoff</h1><p>Version 0.2.0 &middot; Windows + Nintendo 3DS &middot; %d screens &middot; %d asset files. Every image was rendered by the shipping game code.</p>", g_nshots, assetCount);
    fprintf(h, "<h2>Platform</h2><table><tr><th>Screen</th><th>Size</th><th>Role</th></tr><tr><td>Top</td><td>400&times;240</td><td>Gameplay world + HUD</td></tr><tr><td>Bottom</td><td>320&times;240</td><td>Touch menus, inventory, maps</td></tr></table>"
               "<p>Pixel art 1:1 on device, nearest-neighbour only. Camera zoom 100/125/150/200%% (default 125%%) scales the world layer; HUD and text stay native.</p>");
    fprintf(h, "<h2>Colour</h2><p>RGB565 on device; hex values are the exact rendered colours.</p><table><tr><th>Token</th><th>Hex</th><th>Use</th></tr>");
    for (auto& s : SW) { char hx[8]; hexOf(s.c, hx); fprintf(h, "<tr><td><span class=\"sw\" style=\"background:%s\"></span><code>%s</code></td><td><code>%s</code></td><td>%s</td></tr>", hx, s.name, hx, s.use); }
    fprintf(h, "</table><p>Atmosphere grade: day <code>#1E160E</code>, dusk <code>#2C160E</code>, night <code>#060A1A</code>, plus vignette.</p>");
    fprintf(h, "<h2>Typography</h2><p>5&times;8 bitmap font, 6 px advance, 10 px line, 1 px ink shadow. Bold (7 px advance) for titles, tabs, buttons.</p><img class=\"big\" src=\"assets/ui/font_sheet@3x.png\" alt=\"font sheet\">");
    fprintf(h, "<h2>Components &amp; input glyphs</h2><p>Panels, button states (normal, focused, selected, gold, disabled, left-aligned), tabs, bars, and input glyphs for 3DS / keyboard / Xbox.</p><img class=\"big\" src=\"assets/ui/components@3x.png\" alt=\"components\">");
    fprintf(h, "<h3>Spacing</h3><ul><li>World tiles 16 px; characters 16&times;24 (feet pivot); buildings 48&times;48 on 3&times;3 plots.</li><li>Bottom screen margin 4 px; panel bevel 3 px; title bar 13 px; footer strip y 224&ndash;240.</li><li>Buttons 18&ndash;22 px tall; tabs 15 px.</li><li>Inventory cells 24 px (6&times;4 pack), 16 px (larger), 12 px (loot screen).</li></ul>");
    fprintf(h, "<h2>Art</h2><div class=\"grid\">");
    const char* sheets[][2] = {{"assets/sheets/characters.png", "Characters (all variants, directions, poses)"}, {"assets/sheets/tiles.png", "Tiles"}, {"assets/sheets/props_icons.png", "Props, decals, icons"}, {"assets/sheets/items.png", "Items"}, {"assets/sheets/buildings_portraits.png", "Buildings + portraits"}};
    for (auto& s : sheets) fprintf(h, "<div class=\"card\"><img src=\"%s\" alt=\"%s\"><b>%s</b></div>", s[0], s[1], s[1]);
    fprintf(h, "</div>");
    lastArea = "";
    fprintf(h, "<h2>Screens (%d)</h2>", g_nshots);
    for (int i = 0; i < g_nshots; i++) {
        const ShotInfo& s = g_shots[i];
        if (strcmp(lastArea, s.area)) { if (i) fprintf(h, "</div>"); fprintf(h, "<h3>%s</h3><div class=\"grid\">", s.area); lastArea = s.area; }
        fprintf(h, "<div class=\"card\"><a href=\"screens/%s.png\"><img src=\"screens/%s.png\" alt=\"%s\" loading=\"lazy\"></a><b>%s</b><p>%s</p><p><code>%s</code></p></div>", s.file, s.file, s.title, s.title, s.note, s.file);
    }
    if (g_nshots) fprintf(h, "</div>");
    fprintf(h, "<h2>Asset package</h2><p>%d files under <code>assets/</code>: transparent PNGs at 1&times; and @4&times;, UI sheets, 29 sound effects + 4 ambience beds (WAV). Full table in <code>DESIGN_HANDOFF.md</code>.</p>", assetCount);
    fprintf(h, "<h2>Open design work</h2><ul><li>Six locations need layouts and environment art.</li><li>Taller Police Station facade.</li><li>Dedicated 45&deg; character art.</li><li>Smoother night light edges.</li></ul></main></body></html>");
    fclose(h);
}

static void handoff(const char* out) {
    g_out = out;
    char p[512];
    mkd(out); snprintf(p, sizeof p, "%s/screens", out); mkd(p); snprintf(p, sizeof p, "%s/screens/native", out); mkd(p);
    captureScreens();
    int n = exportAssets();
    writeDocs(n);
    printf("handoff: %d screens, %d asset files -> %s\n", g_nshots, n, out);
}
