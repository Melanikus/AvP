# How alien ways work

*Written 2026-11-08 by Melanikus for the AvP port (#109, #118, #119, #138).*

This explains, in plain words, how the aliens in this port of *Aliens versus Predator* (1999) plan their way across walls and ceilings:
what each part does, why, every number it uses, how to turn it on and test it, and what isn't done yet. It's written so that someone who
has never seen the code, and can't ask anyone, can follow it. Each file named here also explains itself at its top.

## 1. What it is, and why

In the 1999 game an alien can run on walls and ceilings, but nothing plans a way over them. It climbs only when it happens to run into a
wall while it's allowed to. Most of the time it runs along the floor, like a dog.

The user asked for aliens that keep off the floor where they can: **"floor is lava"** (issue #109). Alien ways gives each alien a *way*:
a list of points across the level's surfaces, from where the alien is to where it's going, that goes up the walls and along the ceilings
when that isn't much longer than the floor. The alien follows it, and the game's own 1999 code still does everything else: its
attacks, its senses, its animations and its physics.

Later parts, each from a ruling of the user's:
- **Different routes** (issue #138): aliens hunting together take different ways.
- **Doors:** a way never goes through a door an alien can't get through (#118).
- **Smooth curves:** an alien aims a little ahead along its way, rather than at each corner.

## 2. Turning it on

While it's being made and filmed, alien ways is a developer feature, on only with switches on the game's command line:

| Switch | What it does |
|---|---|
| `-dev -alienways` | Builds the map at each level's start and guides the aliens. Without it, none of this runs. |
| `-dev -alienways -alienwaysview` | Also draws the developer view (section 12), for films. Its own switch, so no player ever sees the lines once the ways are on for everyone. |
| `-dev -alienwaysaim=N` | How far ahead along its way an alien aims, in mm, 0 to 10000. The default is 1500. 0 aims at the next point of the way, as before the look-ahead was added, so a film can compare the two. |
| `-dev -alienwayswatch=N` | One line in the log every frame for the alien in the guide's place N (0 to 63): what its state did, whether it was steered or kept driving, its grip and speed, its target and the marine scent near it. Off by default, and it stops after 6,000 lines a level. |

A number outside its range is ignored, and the log says so. The command line comes from outside the game, so it's checked (AvP's third
rule: input is hostile).

**When it ships**, the user's ruling is that it's always on, in every mode, with no setting for players. That's one line in
`AlienWays_On` (alien_ways.cpp).

**Where it runs:** only on the machine that runs the aliens. That's single player, or the host of a network game (`AvP.NetworkAIServer`).
A machine that joined a network game only draws the aliens the host sends it, so it has no map.

## 3. With the switch off

With `-alienways` off, the game plays exactly as in 1999:
- Nothing is listed or built, and no memory is set aside.
- Every call from the aliens' code into alien ways gives back "no way", and the 1999 code steers the alien.
- The avoidance tests that alien ways holds back while a way steers an alien act as they always did.
- The developer view draws nothing.

Several tests check that the game's calls do nothing with the switch off.

## 4. The parts

Most parts know nothing about the game: they work on plain numbers, so a test can make up a small world and check every answer. Only
alien_ways.cpp and alien_ways_level.cpp read the game's own structures.

| File | What it does | Knows the game? |
|---|---|---|
| alien_ways.h / .cpp | The game's side: starts and ends each level's map, gives the guide what it needs each frame, steers the aliens, and writes the log. | yes |
| alien_ways_level.h / .cpp | Lists the loaded level's polygons and doors for the map. Reads each door's state every frame. | yes |
| alien_ways_build.h / .cpp | Builds the map on a thread of its own, so the game doesn't stop while it's made. | no |
| alien_ways_map.h / .cpp | The map of surfaces, and the search for a way across it. | no |
| alien_ways_guide.h / .cpp | Which aliens are following a way, what each way is, when to search again, and when an alien is stuck. | no |
| alien_ways_claims.h | Each alien's claim on the polygons its way uses, so the others pay a little more to use them (different routes). | no |
| alien_ways_lookahead.h | Where along its way an alien aims: a little ahead, so it curves. | no |
| alien_ways_stuck.h | The words of the log's lines about an alien found stuck, or let go. | no |
| alien_ways_view.h / .cpp | Turns the ways into coloured lines for the developer view. d3d_render.cpp draws them. | no |

Where the game calls in:
- game.cpp: `AlienWays_LevelStart` at a level's start, and `AlienWays_Frame` each frame, before any alien moves.
- system.cpp: `AlienWays_LevelRestarts` at a restart, and `AlienWays_LevelEnd` when a level is let go. (A lift into another
  environment used to build its map here too; main took that path out, #135, as it could never run.)
- bh_near.cpp, the aliens' close-range behaviour: `AlienWays_Steer` in Approach, Hunt and Wander, `AlienWays_KeepDriving` after each
  frame's state, and `AlienWays_Following` to make an alien on a way crawl.

## 5. A level's life

```
level start      AlienWays_LevelStart: list the level's polygons and doors (on the game's thread), then start the build on its own thread.
  |              Until the map is ready, aliens move as they always have.
each frame       AlienWays_Frame, before any alien moves:
  |                - once, the log says how the build went (ready, its size and time, or why there's no map);
  |                - each door's state is read, so a search keeps off shut doors;
  |                - aliens no longer guided get their usual gravity back;
  |                - the searches carry on, within the frame's allowance.
each alien       in Approach, Hunt or Wander: AlienWays_Steer gives the direction of its way, if it has one, and it climbs.
  |              If not, the 1999 code steers it.
  |              Then AlienWays_KeepDriving keeps it going along its way on a frame its state didn't ask.
restart          AlienWays_LevelRestarts: every alien, its way and its claims are forgotten. The map and the doors stay, because a restart
  |              doesn't change the level's shape.
level end        AlienWays_LevelEnd: stop the build if it's still going, wait for its thread, and let go of the map and every way.
```

## 6. The map

**Places.** Every polygon an alien could cling to is a place on the map. Each one is a **floor**, a **wall** or a **ceiling**, by which way
it faces. The test is the game's own floor angle (FLOOR_THRESHOLD in dynamics.cpp): a polygon facing up by at least 30000 of 65536 is a
floor, one facing down that much is a ceiling, and anything between is a wall. The game's numbers are millimetres, and y goes *down*, so
"up" is a smaller y.

**Joins.** Two polygons are joined where they touch: where a corner of one lies within 10 mm (`kAlienWaysTouch`) of a corner or an edge
of the other. The edge part matters, because a long wall's bottom edge often runs past the corners of several floor tiles. Each join keeps
the stretch along which the two touch, so a way can cross at the point of it nearest to where the alien is.

**What counts:**
- a room's polygons, and a door's (a door is in its open shape in the level, and its frame is a real wall);
- an object that stands still (a crate, a lamp, a switch, a screen), where it stands and turned the way it's turned.

**What's left out:**
- a lift's car, a platform and anything on a track, which won't be where the level has them;
- anything the game says nothing collides with;
- things picked up;
- scenery that's far too big to matter: a polygon over 5,000 m² seen from above (a sky), a wall over 50 m tall (the fake view outside a
  space station), and a corner more than 64 km from the middle of the world.

**Limits.** A level file comes from outside the game, so every count has a limit, and memory has two ceilings. Over any of them, the map
isn't built at all, and the log says why in words. The stock levels' largest need is in brackets:

| Limit | Most | (stock's largest) |
|---|---|---|
| polygons | 200,000 | 39,354 |
| corners in one 1 m box | 4,096 | 1,004 |
| edge steps while joining | 10,000,000 | 742,611 |
| corner-against-edge tests | 100,000,000 | 10,634,886 |
| joins | 1,000,000 | 189,256 |
| memory kept | 32 MB | 8 MB |
| memory while building | 64 MB | |
| doors | 4,096 | a few dozen |

**Size and time.** On the 32 stock levels the map keeps 4.6 MB on average, and 8 MB at most. A grid of every surface would have been
251 MB on average. Joining a big level up takes up to about a second, so it's done on its own thread.

**The build's thread** only ever touches its own copy of the polygons and the map, until it says it's finished. The game's thread asks
the state, and uses the map only once the state says it's ready. The state is the one thing both read, and it's an atomic. If no thread
can be started, the map is built at once on the game's thread, and the log says so.

## 7. The search: floor is lava

A way is a search over the places, from the polygon under the alien to the polygon under its goal. Each polygon is reached at a point, the
start or where the way came onto it. Going on from there costs the distance to where the way leaves, times that polygon's **price**:

- a wall or a ceiling: 1;
- a floor: `kAlienWaysFloorPrice`, which is 10.

With a price of 10, a metre of floor costs as much as ten metres of wall, so a way goes up the wall wherever that's less than ten times
as far.

```
cheap floor (price 1):   A ------------------------- B          all floor
floor is lava (10):      A                           B
                          \_________________________/            up the wall, along it, and back down
```

**Why 10:** measured with no game running on the 32 stock levels, over 1,636 legs of a route. A price of 10 left 79% less floor than
price 1, with ways 41% longer. Past 10, almost no more floor was left behind.

**Rules that shape a way:**
- **Head height** (`kAlienWaysClimbHeight`, 1.5 m): on every wall it crosses, a way climbs to 1.5 m above the wall's lowest point (or to
  the wall's top, if that's lower), goes along at that height, and comes down only where it leaves. Without it, ways ran along a wall's
  very foot, where the alien is as good as on the floor.
- **A step down costs at least 0.5 m of floor** (`kAlienWaysLeastFloorStep`). Otherwise a way could drop to the floor for no length at
  all where a floor's corner touches two walls, and climb straight back.
- **Round an outside corner** (`kAlienWaysStandOff`, 0.8 m): where a way goes from one wall to another round a sharp outside corner (a turn
  of 60 to 120 degrees, such as the edge of a pillar), it gets one more point, stood off the corner along each wall's facing. Heading
  straight for a point round the corner, the alien would push into its own wall.
- **Doors:** see section 10.
- **Claims:** see section 9.
- **At most 256 points** (`kAlienWaysMostWayPoints`). A longer way is cut short and says so, and it's searched for again as the alien
  goes.

**The allowance.** All the searches together may look at 500 polygons a frame (`kAlienWaysLooksPerFrame`). One search runs at a time,
first asked, first served, so no frame is held up however many aliens ask.

**Good, not always the cheapest.** Each polygon is settled by the first way that reaches it, but what crossing a polygon costs depends
on where the way came onto it. So a dearer way that reaches a polygon first can win over a cheaper one that comes later. The ways it
finds always join up; they're just not always the cheapest (section 15).

**Where the search starts and ends.** An alien starts on the nearest polygon of the kind it's standing on, by its body's down, within 2 m
(`kAlienWaysNearEnough`), looked for first in its own module. If its modules have none that near, every polygon of the map is looked
through. The same goes for the goal.

**What the map doesn't know:** the alien's width, jumps across air, and lifts. A way means the surfaces join up, not that an alien fits.
The game's physics decides the rest.

## 8. The guide: following a way

**Who is guided:** a standard alien that can climb, in Approach (coming for its target), Hunt or Wander. At most 64 at once
(`kAlienWaysMostFollowers`); one more is steered by the 1999 code.

**Where it heads:**
- When the target's module can be seen from the alien's, the goal is the target itself, so the way can go round by the walls and ceilings
  all the way to it.
- Otherwise the goal is the point the 1999 code picked: in Approach, the target or the way into the next module towards it; in Hunt and
  Wander, the way into the next module. So the way changes only how the alien gets there, never where it goes.

**A way all on the floor** is handed back to the 1999 code, which is as good on one floor: it curves at its target and climbs any wall it
touches. The exception is a way to the target itself, which is followed even all on the floor, for the last metres. Near its target the
1999 hunting can flip between Hunt and Wander every frame and never close in.

**Following.** The alien heads along its way's points:
- A point counts as **reached** when the alien is within 0.6 m of it across the surface it's on, and within 1.5 m of it above or below
  its own middle.
- **A step onto another surface** counts as made only once the alien is on the new surface. Until then it keeps heading for the corner
  where the two surfaces meet, so that the game's own climbing turns it onto the new one.
  - Two polygons are **one surface** (no step) when, with both facings known, they face within 30 degrees of each other, or, with a facing
    not known, when they're the same kind (`AlienWays_SameSurface`, alien_ways_map.h).
  - Otherwise the step is made once the alien's body's down is within 55 degrees of the new polygon's facing turned round.
  - A bevel, the strip between a wall and the floor that faces half up, is about 45 degrees from the wall, so the step onto the wall is
    made at once.
- **Gravity:** while an alien is on a way, its usual gravity is off, so it sticks to whatever it's touching. It gets its usual gravity
  back the moment it stops being guided, so a dying alien doesn't hang on a ceiling.
- **Crawling:** an alien on a way crawls (`AlienWays_Following`).
- **Avoidance held back:** while its way really steers it this frame, the 1999 tests that would send an alien off to avoid something
  don't act, because running into a wall's foot is how it climbs onto it. The log says when one would have acted. A breakable thing in
  the way is still broken.

**The aim** (alien_ways_lookahead.h). The alien doesn't aim at the next point itself, or it would run corner to corner. It aims at a point
1.5 m further along the way, from its nearest place on it (`-alienwaysaim`). The aim stops short:
- at the goal;
- at a step onto another surface that the alien hasn't made yet (the same test as the guide's step);
- at the bend where the way has turned more than 30 degrees, all its bends added together. An outside corner can't be cut.

It never looks more than 10 m ahead. With one bend of just under 30 degrees half-way, the straight line to the aim passes 0.19 m inside
the corner.

**Asking again.** A new search is asked for:
- when the way is 2 s old;
- when the goal has moved 3 m;
- when the alien is 3 m off its way.

The alien follows its old way until the new one comes. **A way that's being followed is never dropped because a search failed**, so an
alien on a ceiling doesn't let go because its way's renewal found nothing.

**Stuck.** An alien that gets less than 0.1 m nearer to its next point in 1.5 s is stuck. Its way is dropped, and the 1999 code steers it
for 3 s before it asks again. The log says why it might be stuck, from the game, at that moment (alien_ways_stuck.h).

**Keeping going.** The 1999 Hunt and Wander send an alien back and forth between the two states, one frame each, and some frames don't
ask for the way at all. On such a frame, an alien the guide still keeps is driven along the way it already has (`AlienWays_KeepDriving`).
Driving never counts as asking, so the rule below still lets it go on time.

**Let go.** An alien stops being guided, and gets its gravity back:
- when Steer says there's no way;
- when nothing has asked about it for half a second (it's gone far, or gone, or it's in a state that doesn't ask). A frame or a few with
  no asking don't let it go.
- at once when the game lets it go, at the first frame it's in a state that never asks for a way, such as attacking or dying;
- at a restart or the level's end.

## 9. Claims: different routes

So that aliens hunting together come from different directions where the level allows it, each alien **claims** the polygons its way uses
(alien_ways_claims.h):

- a polygon the way is on gets 2 claims;
- a polygon one join away from the way gets 1 claim, so a way right beside another counts as using it a little;
- another alien's search pays 1 + 0.125 times a polygon's price for each claim on it, but never more than 1.5 times. So a crowd can't wall
  a corridor off: an alien that has no other way still takes it;
- an alien's own claims don't count against it.

```
alien A's way:     [2] [2] [2] [2]            the polygons next to it get [1]
               [1] [1] [1] [1] [1] [1]
alien B, searching, pays 1.25 x on A's way, 1.125 x beside it, and 1 x anywhere else
```

Each alien's claims are kept on their own, so taking them off takes off exactly what that alien put on. They're taken off whenever its way
goes: it's let go, its way is dropped, or a new way replaces it. All of them go at a restart and at the level's end. All their memory, about
2 MB, is made once a level, when the map is ready, and never grows after that. Every number handed in is checked first.

A price never below 1 keeps the search's estimate of what's left a lower bound, as a search of this kind needs.

**Flankers** (the user's rule: "1 in 3, up to 2.5x", and "the 4th and 5th are long route sneaking bastards"). The guide counts the
aliens coming for each target. Each alien takes its place among them at its first search for that target, and keeps it while that's its
target:
- the first 3 (`kAlienWaysDirectAliens`) search as above: 0.125 a claim, at most 1.5 times;
- every one after them is a **flanker**: the others' claims weigh three times as much for it (0.375 a claim), and a polygon can cost it
  up to 2.5 times. Where the level has another way round, it takes it, and no way it takes costs more than 2.5 times the cheapest.

Each way found says in the log what it cost, whether it's a flanker's, and which direction it comes into its goal from, seen from above.

## 10. Doors

A way never goes through a door an alien can't get through:
- The lister finds each door module: a proximity door, a lift's door, or a door worked by a switch. It marks the door's polygons with its
  number, counted from 1, so that 0 means "no door".
- Each frame, before the searches carry on, each door is read from its own module's block. The block is checked to be that kind of door,
  with its data, before it's read. A door that can't be read counts as shut.
- The rule is the game's own for aliens' routes (AIModuleAdmitsPheromones, pheromone.cpp):
  - a proximity door opens by itself for an alien that comes near, so it lets one through unless it's locked;
  - a lift's door, or a switch's door, only while it's open, because an alien can't open either.
- A search keeps off a shut door's polygons, except the one it starts on and the goal's own, so an alien in a doorway can always try to
  leave. A search sees the doors as they were when it started.
- Doors are kept by their place in the level's list of modules, not as pointers, and read again every frame. So nothing stale is left
  after a restart.

## 11. The log

Alien ways writes plain lines to the game's log (avp_log.txt, which is overwritten at each launch, so copy it before running again):
- how the build went: the map's size and time, what was left out and why, or why there's no map;
- each search that ends: found (with its length on the floor, walls and ceilings), cut short, no way, or refused, and which end was refused;
- an alien found stuck, with the game's facts at that moment; an alien let go, with the state it left for;
- a 1999 avoidance test that would have sent a guided alien off;
- the watched alien's line every frame, with `-alienwayswatch`.

Each kind of line has a limit a level, so a long level can't fill the log.

## 12. The developer view

With `-dev -alienways -alienwaysview`, each alien's way is drawn in the level as lines, coloured by what each stretch is on: gold for the floor, blue for a
wall, purple for a ceiling. A small cross marks each point. The stretch the alien is on and the point it's heading for are white. The lines go
through the same sums as the level's own polygons, so a wall hides a way behind it. The view is never drawn in a network game, where seeing an
alien's way through a wall would be cheating.

## 13. Tests

Every test is a small program in `tests/`, built and run by `tests/build.ps1` with the settings in its `.args` file. Each has a take-out list
in `tests/takeouts/`: every row breaks one thing in a copy of the code, and the test has to notice. That proves each check can fail.

| Test | What it tries |
|---|---|
| alien_ways_map_test | the map and the search on made-up worlds: kinds, joins, limits, prices, head height, stand-offs, doors, claims in a search |
| alien_ways_worlds_test | nine made-up worlds, each with its way worked out by hand at floor prices 1 and 10 |
| alien_ways_build_test | the thread: start, finish, stop, building without a thread, and the restart's call (by reading the game's source) |
| alien_ways_guide_test | following, reaching, steps, renewing, stuck, let-go, keep-driving and the aim, with made-up aliens; and the game's calls, by reading its source |
| alien_ways_hunt_test | that Hunt and Wander call the alien ways as they should, by reading bh_near.cpp |
| alien_ways_claims_test | the claims on a made-up map of 15 polygons, with hostile numbers and the memory limit; the flankers' weights are tried in the map and guide tests |
| alien_ways_lookahead_test | the aim on made-up ways: straight, bends, steps, seams between walls, stretches of no length, refusals, the map's farthest corners |
| alien_ways_stuck_test | every word of the stuck and let-go lines |
| alien_ways_view_test | the developer view's lines and colours |

The tests that "read the game's source" check that the game calls alien ways where and how it should. The game itself can't run in a test.

## 14. The numbers are first guesses

Every rule above is a first guess, to be tuned from the log and from films of the game. The main ones and their files:

- floor price 10, head height 1.5 m, least floor step 0.5 m, stand-off 0.8 m (60 to 120 degrees): alien_ways_map.h;
- reached 0.6 m and 1.5 m, renew after 2 s, 3 m and 3 m, stuck at 0.1 m in 1.5 s, rest 3 s, let go after 0.5 s, 500 polygons a frame:
  alien_ways_guide.h;
- one surface within 30 degrees (alien_ways_map.h), and a step made within 55 degrees (alien_ways_guide.h);
- look 1.5 m ahead, at most 10 m, stop past 30 degrees of bend: alien_ways_lookahead.h;
- claims 2 and 1, 0.125 a claim, at most 1.5 times: alien_ways_claims.h; flankers after the 3rd, 0.375 a claim, at most 2.5 times:
  alien_ways_guide.h.

## 15. Not done yet

Gathered from every commit's own notes, as of 2026-11-08:

**The search**
- It isn't exact (section 7): a polygon is settled by the first way that reaches it, though what crossing it costs depends on where the
  way came onto it (its entry point, and the 0.5 m least floor step after a wall). In a JavaScript copy of the search, from one start on
  Genshd1, the way found cost 10% more than one it missed. The claims multiply the difference: a flanker in one film paid 2.44 times its
  route's unclaimed price where a way at 1.59 times was there. A test is owed that shows it in the C++: two ways into one room, the
  cheaper one reaching the room second. The exact search is planned with the grid (#145, #149), so this version won't be tuned for it.

**How the alien moves**
- Whether a point is reached, and whether the alien is stuck, still go by the way's next point, not by the aim.
- The aim doesn't know the alien's width, and doesn't check that the straight line to it stays on or clear of a surface.
- Gentler corners (under 60 degrees) and knife edges (over 120) get no stand-off. A crossing where four or five polygons meet isn't stood
  off yet. Whether the game turns the alien onto the new wall once it's round an edge is for the film to show.
- Hunt asks only while it's more than 2 m from its doorway point, even when the goal is the target, which may drop the way at each doorway.
- The target's module is the game's own pick, and a big open-space module's box can take in a room's floor.

**Claims and flankers**
- Nothing logs how many claims a search met.
- An alien keeps its place among a target's aliens when one ahead of it is let go, so a 4th stays a flanker even if one of the first three
  dies. The log reader doesn't sum the arrival directions yet.
- Whether "1 in 3" means every alien after the 3rd (as written) or every third alien is a question for the user. The first film had
  flankers arriving from the same directions as direct aliens, so whether they really take other ways is still to be counted from the log.
- Adding an alien's claims looks through what it holds one by one: fine for the few hundred polygons of a way, slow near its most of 4,096.
- The claims can change while a search runs over several frames, so a way may be a little dearer than the cheapest. It still joins up.

**Tests and tools**
- No test drives the game side's "let go by state" with real game states; that's checked only by reading the source.
- The log reader doesn't sum the newest lines yet.
- The numbers measured offline on the stock levels (the floor price, the limits, the sizes) are given where they're used, but the
  scripts that measured them aren't in the repository yet. Which way round a polygon's corners go (alien_ways_map.cpp,
  AlienWays_HowMuchItFacesUp) was checked against the normals the stock levels store, but how many polygons that covered wasn't recorded.

**Shipping**
- It's still behind `-dev -alienways`. Shipping means turning it on in `AlienWays_On`, with the user's word.

**What a map can't know**
- The alien's width, jumps across air, and lifts.
