<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Kuba's Slipper Workshop

A physics-merge shop game for the Coopa card. Kuba crafts slippers for the whole family: drop blanks, merge matching pairs into bigger and pricier slippers, stash the finished ones in your backpack, sell them for materials, and buy fancier blanks to scale up production. Wealth brings risk: once the bin holds valuable slippers, the rim line and the bin capacity are real threats — one overflow and every slipper in the bin AND the backpack is spilled. The workshop floor (bin, purchase queue, hammers) is saved with your save: leave, power off, come back — and carry on where you left off. Milestone stars, once lit, are permanent — even a fresh workshop can't take them away.

- **Merge up**: ▲▼ move the hanger, ● drops the blank; two matching slippers fuse into the next tier — from an insole all the way to Grandma's legendary Spinning Slipper, with every tier made for a family member (the cat's cloth slipper, Dahuang's paw slipper, Dad's leather slipper…)
- **Bag it**: hold ▼ to sweep every finished slipper (cloth tier and up) into your backpack. Keep them in the bin to merge higher, or bank them before the pile overflows — that's the whole dilemma
- **Sell for materials**: in the backpack, ● sells one and holding ● sells the whole row; prices scale ×2.5 per tier (insole 1 → Spinning Slipper 4000)
- **Slipper upgrades**: pick a slipper type in the backpack and hold ▼ to spend materials on an upgrade (max level 3, +30% sell price per level, doubled at max) — upgrade what you sell most
- **Blank shop**: open the function menu and pick the shop; bought cloth/paw/Kuba blanks queue up in order — drop your current one first, the 下一只 preview always shows the front of the queue; or restock hammers. Also the **basket upgrade** (max level 5, prices 50/150/500/1200): a bigger basket unlocks higher merge tiers — Lv1 merges up to the Kuba slipper only (two Kubas won't fuse), Lv5 unlocks all ten tiers. ● buys, the bottom rows go to the backpack or close the shop
- **Overflow (fail)**: if the pile holds the rim for a cumulative 1 second (flickering doesn't reset it, leaving the line pauses it), it topples — every slipper in the bin and the backpack is spilled; materials, lifetime income and milestones survive. The rim flashes red as soon as the pile nears it (faster as danger grows) with a big countdown. The bin holds at most 26 pieces — once full, collect or merge to make room
- **Compacting hammer**: once the pile crosses the rim, ● becomes a hammer tap that knocks the pile down and packs it tight (the only rescue for a tower of scraps); 10 materials each in the shop
- **Combos**: merges within 400ms chain up for bonus materials; fusing two Spinning Slippers is a grand union — a **slipper rain**: Grandma hollers 「拖鞋会拐弯!」, a 1000-material bonus is paid, and 20-30 Kuba-to-Grandpa sized slippers pour from the sky — rain slippers can be merged further on the spot (two Grandpas make a Grandma); no topple calls during the rain
- **Spicy levels**: pick 辣度 on the title screen — mild widens the bin and pays ×130%, hot narrows it and pays ×80%
- **Story & tutorial**: 「新的工坊」opens with a dialog cutscene (typewriter text + granny/Kuba voices) — Kuba finally opens his own slipper workshop; in the workshop Grandma teaches dropping and merging, and only after your first finished slipper does she teach collecting, then the menu; milestones, basket upgrades and the slipper rain all trigger her praise lines. **Light up all eight stars** and re-enter the workshop for the finale: a touched Grandma passes her own slipper down to Kuba — it lands in your backpack for real
- **Cross-game link**: veterans of Kuba's Great Adventure wear a badge on the title screen
- **8 milestones** (= store stars, permanent): first sale, craft Dad's slipper, keep a Grandma slipper in the bag, craft the golden slipper, craft the Spinning Slipper, max the basket, max a slipper type, fuse two Spinning Slippers into a slipper rain

## Layout

| File | What it is |
|---|---|
| `coopa.toml` | id, name, version, SDK, entry, milestones |
| `slipperworks_texts.h` | every string on screen (font glyphs are collected from here) |
| `src/slipperworks_art.*` | original pixel art: the 10-tier slipper chain (10→44 px), material spool, hammer, sparkle |
| `src/slipperworks_logic.*` | pure gameplay: verlet physics bin, merging, combos, collecting, price tables |
| `src/slipperworks_save.*` | save layout (176 bytes, legacy saves auto-migrate), validation, milestones |
| `src/slipperworks_ui.*` | screens: title, workshop, shop overlay, backpack |
| `src/slipperworks_app.c` | the `kit_app_t` table the card shell calls into |
| `tools/artcheck.py` | validates pixel art rows: count, width, palette |
| `tools/simtest.sh` | rebuilds the ASan simulator and runs a key script (headless) |
| `tools/devlink.py` | USB tool: screenshot / key injection on a real card |

## Controls

| Screen | Keys |
|---|---|
| Title | ▲▼ select, ● confirm (workshop / backpack / spicy / menu) |
| Workshop | ▲▼ move hanger, ● tap to drop; past the rim ● = compacting hammer |
| Workshop (hold) | hold ▼ collect to bag; hold ● open the **function menu** (physics pauses; a blank dropped on the way counts as normal) |
| Menu | continue / shop / backpack / collect / hammer / title / firmware menu |
| Shop | ▲▼ select, ● buy; bottom rows: backpack / close (close returns to the function menu) |
| Fail | ● open a fresh bin (spilled slippers are gone; materials stay) |
| Backpack | ▲▼ pick a row, ● sell one, hold ● sell all, hold ▼ upgrade that slipper; last row returns to the function menu |

## Development

In Codespaces or a local Dev Container: `coopa run` to play (↑↓ space = ▲▼●, hold = long press; `--seed` replays the same blank sequence), `coopa check` for the store checks. Physics and economy tuning lives in the macro block at the top of `slipperworks_logic.c` plus the `slipperworks_sell_price` / `slipperworks_shop_*` tables.
