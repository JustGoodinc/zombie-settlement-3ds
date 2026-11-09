// Handcrafted level layouts. One character per 16x16 tile.
// Tiles:   " grass  = asphalt  ^ parking line  : dirt  . lino  , wood  ~ carpet  _ concrete
//          # wall  W window  % cell bars  F fence  E extraction  g garden  X palisade
// Doors:   D closed  O open  K keycard-locked  C cell door (cell key)  G settlement gate
// Props:   s shelf  x crate  d desk  Q desk(keycard)  U desk(cell key)  f filing cabinet
//          l locker  m medical cabinet  v police car  u wrecked car  (containers)
//          T tree  b bench  k cot  c chair  p plant  w water cooler  y decor crate  L lamp
//          * campfire  r radio/map table  & workbench  n generator spot  1-6 building plots
// Spawns:  Z idle zombie  z wandering zombie  q lying zombie  R trapped survivor
//          P player start  V gate visitor spot

static const char* const POLICE[] = {
R"MAP(""T"""""""""""""""""""""""""""""""T""""""""""""""""""")MAP",
R"MAP("""""""::::::::::::""""""""""""""z"""""""""""""""""T"")MAP",
R"MAP("T""""":x::::y:::::""""""""""""""""""""""""""""""""""")MAP",
R"MAP("""""""::::::::::::"""""""""""""""""""T""""""""""""""")MAP",
R"MAP(""""""""""""""""""""""""""""""""""""""""""""""""""""""")MAP",
R"MAP(""#####W########W#########W#########W########W##W####"")MAP",
R"MAP(""#s..s.x..s#l.l..x.l#l.l.l.l.l#s,,,,,,,w#k..%..k%.k#"")MAP",
R"MAP(""#.........#........#.........#,,,,,,,,,#...%...%..#"")MAP",
R"MAP(""#x.......s#........#.b.....b.#,,c,,,c,,#.R.%.q.%..#"")MAP",
R"MAP(""#.........#..q.....#.........#,,d,,,d,,#%C%%%C%%C%#"")MAP",
R"MAP(""#.s..Z..s.#........#....Z....#,,,,z,,,,#..........#"")MAP",
R"MAP(""#.........#x......x#.........#,,,,,,,,,#U.........#"")MAP",
R"MAP(""#........f#........#.........#,,,,,,,,x#....Z.....#"")MAP",
R"MAP(""#####K########K#########O#########O#########O######"")MAP",
R"MAP(""#.................................................#"")MAP",
R"MAP(""W.......z..........p.............Z........p.......W"")MAP",
R"MAP(""#.................................................#"")MAP",
R"MAP(""######O##########O########O########O#########O#####"")MAP",
R"MAP(""#,d,,d,,,,d,f#f~~~~~~f#fff..fff#m.k..k.m#_________#"")MAP",
R"MAP(""#,c,,,c,,,c,,#~~~Qd~~~#........#........#___d_____#"")MAP",
R"MAP(""W,,,,,,,,,,,,#~~~c~~~~#f..ff..f#........#__c_c____W"")MAP",
R"MAP(""#,d,,d,,,,d,f#~~~~~~~~#........#x......m#_________#"")MAP",
R"MAP(""#,c,,,c,,,c,,#p~~~~~~p#ff..z.ff#........#____Z____#"")MAP",
R"MAP(""#,,,,,z,,,,,,#~~~~Z~~~#........#..q.....#_________#"")MAP",
R"MAP(""#p,,,,,,,,,,x#~~~~~~~~#........#........#________x#"")MAP",
R"MAP(""##########################O#########O##############"")MAP",
R"MAP(""#p.......................................w.......p#"")MAP",
R"MAP(""#....b.....b........................b.....b.......#"")MAP",
R"MAP(""W..............cddc...Z.........cddc..............W"")MAP",
R"MAP(""#....................z............................#"")MAP",
R"MAP(""#.................................................#"")MAP",
R"MAP(""#p...............................................p#"")MAP",
R"MAP(""#####W####W####W########DD########W####W####W######"")MAP",
R"MAP(""__________L_____________________________L__________"")MAP",
R"MAP(""___________________________________________________"")MAP",
R"MAP(""==^====^====^====^====^====^====^====^====^========"")MAP",
R"MAP(""===v=========z======u==========v========q=====u===="")MAP",
R"MAP(""==================================================="")MAP",
R"MAP(""==^====^====^====^====^====^====^====^====^========"")MAP",
R"MAP(""======================Z============================"")MAP",
R"MAP(""=====================P============================="")MAP",
R"MAP(""====================EEEE==========================="")MAP",
R"MAP(""====================EEEE==========================="")MAP",
};

static const char* const SETTLEMENT[] = {
R"MAP(""T""""""""""""""""""""""""""""""T"")MAP",
R"MAP("""XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X:1::::::2::::::3::::::5:::::X""")MAP",
R"MAP("""X:::::::::::&::::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X:::::n::::::::::::::::::::::X""")MAP",
R"MAP("""X::L::::::::::::::::::::::L::X""")MAP",
R"MAP("""X:::::::::::r::::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X:::::::::::::*::::::::::::::X""")MAP",
R"MAP("""X:4::::::::::::::::::::::yy::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""Xgggggggg::::::::::::::::::::X""")MAP",
R"MAP("""Xgggggggg::::::::::::::::::::X""")MAP",
R"MAP("""X:::::::::::::P::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::6:::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""X::::::::::::::::::::::::::::X""")MAP",
R"MAP("""XXXXXXXXXXXXXXGGXXXXXXXXXXXXXX""")MAP",
R"MAP("""""""""""""""""V"""""""""""""""""")MAP",
R"MAP(""""""""""""""""::::"""""""""""""""")MAP",
R"MAP(""T"""""""""""""::::"""""""""""""T"")MAP",
};

const char* const* MAP_DATA[] = {POLICE, SETTLEMENT};
extern const int MAP_HEIGHT[];
const int MAP_HEIGHT[] = {(int)(sizeof(POLICE) / sizeof(POLICE[0])), (int)(sizeof(SETTLEMENT) / sizeof(SETTLEMENT[0]))};
