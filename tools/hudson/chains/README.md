# What ends each level

One file for each of the 32 levels of the three campaigns. Each lists, in order, what has to happen for that level to end. The walking test reads these. A step that can't happen in a network game is written down as missing, so that a run says so and doesn't quietly fail.

**These were worked out from the level files, and nobody has walked them.** A run shows whether a level finishes. These files don't.

## The lines

A line starting with `#` is a note. The first says how the level's end starts. Everything after the first word of a line is the object's name, as the level file has it, and a name can have spaces in it.

| Line | What it means |
|---|---|
| `press <name>` | A switch a player presses |
| `walk <name>` | A switch a player walks into |
| `destroy <name>` | Something a player destroys |
| `kill <name>` | An NPC a player kills. One on its own line is an alien kind, which a network game only has on the host, in Co-op |
| `pickup <name>` | Something a player picks up |
| `self-running <name>` | Something that nothing starts, which may run by itself |
| `missing <name> \| <why>` | A step the level needs, whose object a network game doesn't make |
| `where <room> \| <room>` | The rooms the next step's object is in. Any one of them will do. It belongs to the next line, and isn't a step |
| `near <room> \| <room>` | Rooms the next step's object is only within 2 m of. They count if no `where` room can be reached, and the answer says so |
| `end <name>` | The mission step the chain should reach by itself. It's there to be checked, never fired: firing it would end the level whatever the chain had done. The game gives mission steps no name (`object_setup.cpp`, `AddToBehaviourList(0, ...)`), so it can't be found by this name either |

The name in a `missing` line ends at the ` | `.

A step with no `where` line can't be checked, and a run should count it as that.

Hangar has no `end` line. It has no mission step, because it ends through its queen, and a note says so.

## The room lists

Each level also has a `<level>.rooms`. The walking test uses it to check, before a step counts, that a route exists from where the players have been to the step's room, through doors open at that moment on that machine. `3dc/avp/coop_reach.h` does that, and its comments say how. It belongs in the same folder as the level's `.txt` file when a run reads them.

**The words for what this shows are "a route exists through open doors", and never "a player walked here".** It only knows which rooms join. A ledge, a jump, a door shutting too fast or a gap too narrow are for a real walk.

A line starting with `#` is a note. A name can have spaces in it, and it ends at the ` | `. Case matters, because two rooms in a level can differ only by case.

| Line | What it means |
|---|---|
| `room <room>` | A room that nothing joins, so that every room a chain names is on its list |
| `lockedlift <name>` | A platform lift that starts locked. It isn't a room in the level file, so a route can't see it. A level with one of these **depends on a lift**: a run's log says so, and the level doesn't count as ended by a walk that fires its chain, only by someone really walking it |
| `door <room> \| <kind>` | This room is a door, of the kind `proximity`, `switch`, `lift` or `platform`. A run says in its log if the game disagrees |
| `join <room> \| <room> \| <how>` | You can go between these two rooms, either way. `<how>` is how we know: `file`, the level file says so. `lift`, a lift links them. `touching`, their boxes touch. `sees`, one is on the list of what the other can see |
| `liftdoor <number> \| <room>` | This door belongs to this lift |
| `liftswitch <number> \| <room>` | Someone who reaches this room can use this lift, so all its doors count as open. It's the room of a switch, or one of the lift's own rooms |
| `liftswitch <number> \| <room> \| near` | The same, with a switch that's only within 2 m of this room |

`touching`, `sees` and `near` are guesses from where things are, because a window lets you see too. So an answer that needs one says which, and the report has to say so too. A level only has the guesses it needed to be worked out at all.

A file with anything wrong in it is refused whole, and a run's log says which line and why.

## How far to trust them

A file with a note saying the level couldn't be walked has its steps from the chains, and from what opens the doors on the way there. Their order is one the chains allow, and doesn't have to be one a player could walk. A note names any door that nothing in the file opens, or whose opener is behind it. That's a gap in how these were worked out, not something wrong with the level, and a person has to say how a player gets past those.

In any other file, the steps are in an order that the room list says can be walked. The steps are the fewest found. There may be others, and they aren't always the way a designer meant it.
