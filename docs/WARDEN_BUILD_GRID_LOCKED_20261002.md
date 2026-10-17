# Warden build grid — locked

**Grid:** 200 uu cells. One cell is one build cell.

**Superseded 2026-10-17, 8:08 AM PT.** The 2026-10-02 note listed the Command Center as 4×4 / 800 uu. That size is retired. Footprints below are `FootprintX` / `FootprintY` on the building data (balance row and `UKodBuildingDefinition`). They are not hardcoded per class in the snap.

| Building | Cells | Span |
|---|---|---|
| Command Center | 5×5 | 1000 uu |
| Barracks | 3×3 | 600 uu |
| Bunker | 3×3 | 600 uu |
| Supply Depot | 2×2 | 400 uu |

## Pivot parity

Snap is per axis.

- **Even N** snaps the pivot to a grid line. The pivot is the centre of the footprint and the shared corner of the inner cells.
- **Odd N** snaps the pivot to a cell centre, 100 uu off the grid line.

The occupied cells are the half-open box centred on that pivot: exactly N cells on that axis, and not the cell that merely touches the far edge. `floor` of an even pivot is one cell too far, because that pivot sits on the boundary.

Adjacent buildings still cover full cells, so they wall off with no gap. A lowered Supply Depot stays walkable. Landing snaps back onto this pivot and restores the authored facing.

## Meshes

The actor origin is the pivot.

- Command Center v5k: pivot at the mesh centre, UCX ±500. That matches the 5×5 (odd) snap. Do not move the pivot to a corner.
- Barracks and Bunker (3×3): pivot at the mesh centre, collision ±300 from that origin.
- Supply Depot (2×2): pivot on the footprint centre, which lands on a grid line, collision ±200 from that origin.
