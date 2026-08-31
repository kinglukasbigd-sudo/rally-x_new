#!/usr/bin/env python3
"""Offline level generator for New Rally-X (clean-room reconstruction).

Produces the plain-text .lvl files consumed at runtime by world/LevelLoader.
Levels are DATA: the game never hard-codes a maze. This tool only exists so
the shipped data files can be regenerated / retuned.

Maze style: open rectangular wall blocks separated by one-tile corridors,
forming a heavily looped playfield (Rally-X mazes are loops, not perfect
mazes -- dead ends are rare and every corridor must reconnect).
"""
import random, sys
from collections import deque

W = H = 32

def build_maze(seed, min_w=2, max_w=5, attempts=4000):
    rnd = random.Random(seed)
    g = [['.'] * W for _ in range(H)]
    for x in range(W):
        g[0][x] = g[H - 1][x] = '#'
    for y in range(H):
        g[y][0] = g[y][W - 1] = '#'

    def can_place(x0, y0, w, h):
        # keep one road tile of padding on every side so corridors stay open
        for y in range(y0 - 1, y0 + h + 1):
            for x in range(x0 - 1, x0 + w + 1):
                if x < 1 or y < 1 or x >= W - 1 or y >= H - 1:
                    return False
                if g[y][x] == '#':
                    return False
        return True

    placed = 0
    for _ in range(attempts):
        w = rnd.randint(min_w, max_w)
        h = rnd.randint(min_w, max_w)
        x0 = rnd.randint(2, W - 3 - w)
        y0 = rnd.randint(2, H - 3 - h)
        if not can_place(x0, y0, w, h):
            continue
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                g[y][x] = '#'
        placed += 1
    return g, placed

def roads(g):
    return [(x, y) for y in range(H) for x in range(W) if g[y][x] == '.']

def connected(g):
    r = roads(g)
    seen = {r[0]}
    q = deque([r[0]])
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n not in seen and g[n[1]][n[0]] == '.':
                seen.add(n)
                q.append(n)
    return len(seen) == len(r)

def dist_field(g, src):
    d = {src: 0}
    q = deque([src])
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n not in d and g[n[1]][n[0]] == '.':
                d[n] = d[(x, y)] + 1
                q.append(n)
    return d

def find_pen(g, dist, length):
    """The straightest, farthest place to garage the pursuit cars.

    Looks for a run of `length` contiguous road tiles, horizontal or vertical,
    and returns the one whose nearest tile is furthest from the player spawn.
    That is what keeps a round from opening with the pack on top of you.
    """
    best = None
    for y in range(1, H - 1):
        for x in range(1, W - 1):
            for dx, dy in ((1, 0), (0, 1)):
                tiles = []
                for k in range(length):
                    cx, cy = x + dx * k, y + dy * k
                    if not (1 <= cx < W - 1 and 1 <= cy < H - 1):
                        break
                    if g[cy][cx] != '.' or (cx, cy) not in dist:
                        break
                    tiles.append((cx, cy))
                if len(tiles) < length:
                    continue
                score = min(dist[t] for t in tiles)
                if best is None or score > best[0]:
                    best = (score, tiles)
    return best


def spread_pick(cands, count, rnd, minsep):
    """Greedy farthest-point sampling so items never clump."""
    out = []
    pool = cands[:]
    rnd.shuffle(pool)
    for c in pool:
        if len(out) >= count:
            break
        if all(abs(c[0] - o[0]) + abs(c[1] - o[1]) >= minsep for o in out):
            out.append(c)
    if len(out) < count:                     # relax if the maze is tight
        for c in pool:
            if len(out) >= count:
                break
            if c not in out:
                out.append(c)
    return out[:count]

def generate(seed, name, rtype, difficulty, fuel, drain, pspeed, espeed,
             enemies, rocks, path, min_blocks=22, block=(2, 5),
             pen_len=7, min_pen_distance=16):
    for attempt in range(400):
        g, placed = build_maze(seed + attempt * 977, block[0], block[1])
        if not (placed >= min_blocks and connected(g)):
            continue
        # The maze also has to be able to hold a pen far from the spawn.
        probe_roads = roads(g)
        probe_spawn = min(probe_roads,
                          key=lambda c: abs(c[0] - W // 2) + abs(c[1] - H // 2))
        probe = find_pen(g, dist_field(g, probe_spawn), pen_len)
        if probe and probe[0] >= min_pen_distance:
            break
    else:
        raise SystemExit("maze generation failed for " + path)

    rnd = random.Random(seed ^ 0x5EED)
    r = roads(g)

    # player spawns near the middle of the map, like the original
    spawn = min(r, key=lambda c: abs(c[0] - W // 2) + abs(c[1] - H // 2))
    d = dist_field(g, spawn)

    # The pen goes down first: it needs the best straight run on the map, and
    # nothing else is allowed to sit in it.
    pen = None
    for length in (pen_len, pen_len - 1, pen_len - 2):
        if length < 3:
            break
        pen = find_pen(g, d, length)
        if pen and pen[0] >= min_pen_distance:
            break
    if not pen:
        raise SystemExit("no room for an enemy pen in " + path)
    pen_score, pen_tiles = pen

    used = {spawn} | set(pen_tiles)
    # Keep a ring of clear road around the pen so the pack can actually get out.
    pen_halo = set()
    for (px, py) in pen_tiles:
        for ax, ay in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            pen_halo.add((px + ax, py + ay))

    far = [c for c in r if d.get(c, 0) >= 6 and c not in used and c not in pen_halo]
    flags = spread_pick(far, 10, rnd, 7)
    used |= set(flags)

    rest = [c for c in far if c not in used]
    extra = spread_pick(rest, 2, rnd, 8)
    special, lucky = extra[0], extra[1]
    used |= {special, lucky}

    # rocks sit in corridors, never adjacent to the player spawn or the pen
    rpool = [c for c in r if d.get(c, 0) >= 8 and c not in used and c not in pen_halo]
    rk = spread_pick(rpool, rocks, rnd, 5)

    for c in flags:     g[c[1]][c[0]] = 'F'
    g[special[1]][special[0]] = 'S'
    g[lucky[1]][lucky[0]] = 'L'
    for c in pen_tiles: g[c[1]][c[0]] = 'E'
    for c in rk:        g[c[1]][c[0]] = 'R'
    g[spawn[1]][spawn[0]] = 'P'

    with open(path, 'w') as f:
        f.write("# New Rally-X level data -- generated by tools/genlevel.py\n")
        f.write("# . road  # wall  R rock  F flag  S special flag  L lucky flag\n")
        f.write("# P player spawn  E enemy pen (all cars launch from here together)\n")
        f.write("name %s\n" % name)
        f.write("type %s\n" % rtype)
        f.write("difficulty %d\n" % difficulty)
        f.write("fuel %d\n" % fuel)
        f.write("fuelDrain %.2f\n" % drain)
        f.write("playerSpeed %.2f\n" % pspeed)
        f.write("enemySpeed %.2f\n" % espeed)
        f.write("enemies %d\n" % enemies)
        f.write("# enemy pen: %d tiles, %d tiles from the player spawn\n"
                % (len(pen_tiles), pen_score))
        f.write("maze\n")
        for row in g:
            f.write(''.join(row) + "\n")
    print("wrote %s (%d wall blocks)" % (path, placed))

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'levels'
    # Round table.  Every round gets its own maze seed, so no two layouts
    # repeat, and the ramp tightens steadily: more cars, faster cars, more
    # rocks, a faster-burning tank, and wall blocks broken into smaller
    # pieces (the "blocks" and "block size" columns).  Wall *coverage* stays
    # near 40% throughout -- what changes is how finely the maze is chopped up,
    # not how much of it is solid.
    #
    #   The tank is always 100: difficulty comes from how fast it burns, not
    #   from how much you start with.  The last column is the resulting round
    #   length in seconds (100 / drain), which is the number worth balancing.
    #
    #   Challenging stages drive a faster car, and their "cars" column is not
    #   a pursuit that starts with the round -- it is how many come out of the
    #   pen at double speed once the tank runs dry.
    #
    #   seed  name       type        diff fuel drain pSpd  eSpd  cars rocks blocks  block size
    rounds = [
        (1001, "ROUND 1",  "NORMAL",    1, 100, 1.00, 1.35, 1.05,  3,   5,   22, (2, 5)),  # 100s
        (2002, "ROUND 2",  "NORMAL",    2, 100, 1.05, 1.35, 1.10,  4,   7,   24, (2, 5)),  #  95s
        (3003, "ROUND 3",  "CHALLENGE", 3, 100, 2.00, 1.65, 1.10,  5,   6,   24, (2, 5)),  #  50s
        (4004, "ROUND 4",  "NORMAL",    4, 100, 1.12, 1.35, 1.15,  4,   9,   26, (2, 4)),  #  89s
        (5005, "ROUND 5",  "NORMAL",    5, 100, 1.20, 1.35, 1.18,  5,  11,   28, (2, 4)),  #  83s
        (6006, "ROUND 6",  "NORMAL",    6, 100, 1.28, 1.35, 1.20,  5,  12,   30, (2, 4)),  #  78s
        (7007, "ROUND 7",  "CHALLENGE", 7, 100, 2.27, 1.65, 1.20,  6,   9,   30, (2, 4)),  #  44s
        (8008, "ROUND 8",  "NORMAL",    8, 100, 1.36, 1.35, 1.22,  6,  13,   32, (2, 4)),  #  74s
        (9009, "ROUND 9",  "NORMAL",    9, 100, 1.45, 1.35, 1.24,  6,  14,   34, (2, 3)),  #  69s
        (1010, "ROUND 10", "NORMAL",   10, 100, 1.53, 1.35, 1.26,  7,  15,   36, (2, 3)),  #  65s
        (1111, "ROUND 11", "CHALLENGE",11, 100, 2.58, 1.65, 1.26,  7,  12,   36, (2, 3)),  #  39s
        (1212, "ROUND 12", "NORMAL",   12, 100, 1.63, 1.35, 1.28,  7,  16,   38, (2, 3)),  #  61s
    ]
    for i, (seed, name, rtype, diff, fuel, drain, pspd, espd, cars, rocks,
            blocks, block) in enumerate(rounds, start=1):
        generate(seed, name, rtype, diff, fuel, drain, pspd, espd, cars, rocks,
                 "%s/level%02d.lvl" % (out, i), min_blocks=blocks, block=block)
