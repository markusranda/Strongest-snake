# Rigmor — The Atlas Walker

Rigmor is a CLI tool that scans, analyzes, and maintains metadata for sprite atlases.  
It’s designed for automation, predictability, and minimalism — no database, no external libs, no bloat.  
Just one binary file, a predictable layout, and a simple interface for pixel-precise automation.

> “No libs, no joins, no bullshit.”

---

## Commands

### 1. **scan**

Parses the entire png and updates the db

### 2. **list**

Lists all sprites from db

### 3. **find**

Finds specific sprite

### 4. **edit**

Edits specific sprite

### 5. **delete**

Deletes specific sprite

## 🧠 Concept

Rigmor’s job is to **walk through a PNG atlas**, detect which _cells_ contain pixel data,  
and record them in a persistent binary database (`.rigdb`).

This makes it trivial to extract structured metadata (names, positions, spans) from  
a packed sprite sheet, without relying on third-party tools like Aseprite or Unity importers.

---

## ⚙️ Atlas Layout

An atlas is treated as a fixed-size grid.

- **Atlas size:** 4096×4096 pixels (configurable later)
- **Cell size:** 32×32 pixels
- **Total cells:** 128×128 = 16,384 cells

Each cell is scanned to detect “content” (non-transparent pixels).

If content is found, Rigmor creates or updates a record in the `.rigdb` file.

---

## 💾 Database Design

Rigmor uses a **flat binary file** (`atlas.rigdb`) with a **fixed record size** for predictable random access.  
No CSV, no SQL, no compression.

### Record layout (40 bytes per entry)

| Offset | Field     | Type       | Bytes | Description                           |
| -----: | --------- | ---------- | ----- | ------------------------------------- |
|      0 | `id`      | `uint32`   | 4     | Unique identifier (never reused)      |
|      4 | `name`    | `char[32]` | 32    | Null-terminated or space-padded ASCII |
|     36 | `x`       | `uint16`   | 2     | Top-left X position (pixels)          |
|     38 | `y`       | `uint16`   | 2     | Top-left Y position (pixels)          |
|     40 | `width`   | `uint8`    | 1     | Width in pixels (≤255)                |
|     41 | `height`  | `uint8`    | 1     | Height in pixels (≤255)               |
|  42–43 | `padding` | `uint8[2]` | 2     | Reserved for flags or alignment       |

**Total:** 44 bytes (aligned for CPU-friendly access)

This file scales cleanly:

---

## 🧩 How It Works

### 1. **Scan**

Rigmor walks the atlas grid, cell by cell.  
If a cell (or region) contains any opaque pixels → it's considered “occupied.”

### 2. **Record**

If that region doesn’t exist in the database → it’s appended.  
If it does → it’s updated or skipped.

### 3. **Detect overlaps**

When a new cell overlaps an existing region (multi-cell sprite), Rigmor prompts the user:

### 4. **Persist**

Changes are committed directly to the `.rigdb` file in-place.  
Edits are atomic — no reserialization, no full rewrites.

---

## 📐 Multi-cell Sprites

Sprites can span multiple cells.  
Instead of breaking the fixed grid model, Rigmor uses the `width` and `height` fields  
to represent the full bounding box of the sprite.

The walker logic skips all cells covered by an existing region during scans.

This keeps:

- One record per sprite (not per cell)
- Random-access structure intact
- Ergonomic edits and safe merges

---

## ⚡ Editing and CLI Ergonomics

The CLI focuses on _safe mutation_ and _quick ergonomics_.

### Example commands

```bash
rigmor scan atlas.png
rigmor list
rigmor find 420 420
rigmor edit 12 name cyberhead_run
rigmor edit 12 move 480 420
rigmor edit 12 resize 64 32
rigmor delete 12
```

```bash
$ rigmor scan atlas.png
Found 3 new sprites:
(420,420) 32x32
(512,420) 32x32
(544,420) 64x64
Proceed? [Y/n]

> Y
> Name sprite #1 [420,420]: cyberhead_idle
> Name sprite #2 [512,420]: cyberhead_run
> Name sprite #3 [544,420]: cyberhead_hit
> ✅ Saved 3 new entries
```
