# HUD_SPEC_v1 — bottom strip rects (B25)

Reference 1920×1080, origin top-left. The canvas scales these by the shortest-side DPI curve and fits them in the 90% title-safe frame. One contiguous bottom bar: the minimap bay and the command bay angle up at the outer corners; the centre well is the lower recess. Frame metal is gunmetal and steel with brass trim. No teal. No cyan on frames.

Left to right:

| Widget | x | y | w | h | Notes |
|---|---:|---:|---:|---:|---|
| W_Clock | 18 | 748 | 108 | 28 | Sits just above the minimap's top-left |
| W_Minimap | 18 | 780 | 250 | 250 | Fog, allies, and enemies |
| W_MapChip column | 276 | 780 | 36 | 36 | Ping, terrain, alliances. Step 44. Right of the map |
| W_IdleWorker | 136 | 748 | 40 | 28 | Hidden at 0 idle |
| W_CtrlGroups | 332 | 786 | 696 | 26 | 10 tabs, 66×26, step 70. Top edge of the centre panel. Order 1–9 then 0 |
| W_SelRail | 332 | 820 | 28 | 222 | Page tabs on the **left** of the grid. Up to 6 pages, tab 28×32, step 37 |
| W_SelGrid | 368 | 820 | 682 | 222 | 8×3, cell 80×70, step 86×76. 24 units a page |
| W_Wireframe | 368 | 820 | 200 | 222 | Single-unit silhouette. Grid hides |
| W_UnitInfo | 584 | 820 | 466 | 112 | Single-unit name, HP, order |
| W_Queue | 584 | 948 | 466 | 80 | Slot 1 72×72 at x584; slots 2–5 64×64 at 668, 740, 812, 884 |
| W_Portrait | 1068 | 792 | 220 | 220 | Right of the centre panel, left of the command card |
| W_HP | 1068 | 1012 | 220 | 14 | Flush to the portrait bottom |
| W_Energy | 1068 | 1028 | 220 | 10 | Slot kept when the fill is hidden |
| W_CmdCard | 1310 | 848 | 344 | 204 | 5×3, cell 64×64, gap 6. Far right |
| W_Chat | 1490 | 804 | 44 | 36 | Above the card |
| W_Menu | 1542 | 804 | 112 | 36 | Above the card's top-right. Right edge lines up with the card |
| W_Resources | 1500 | 16 | 404 | 40 | Top right. Chips 128×40 |
| W_Power | 1500 | 60 | 404 | 6 | Under the resources |

Raised bar tops, design Y: outer corners 736, centre well 776. Seams at x=328 and x=1064.
