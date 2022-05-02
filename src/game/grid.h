#pragma once

#include "game/xf.h"

constexpr int tile_size = 12;

enum class Tile
{
    empty,
    wall,
    wall_a,
    wall_b,
    wall_c,
    wall_d,
    _count,
};

struct TileInfo
{
    Tile tile{};
    int tex_index = -1; // -1 = invisible.
    bool solid = false;
    int corner = -1; // -1 = full tile, 0 = |/, 1 = \|, 2 = /|, 3 = |\.
};

[[nodiscard]] const TileInfo &GetTileInfo(Tile tile);

namespace TileHitboxes
{
    [[nodiscard]] const std::vector<ivec2> &GetHitboxPoints(int index);

    // `corner`: -1 = full tile, 0 = |/, 1 = \|, 2 = /|, 3 = |\.
    [[nodiscard]] int GetHitboxPointsMaskFull(int corner);
}

struct CellLayer
{
    Tile tile{};

    [[nodiscard]] const TileInfo &Info() const
    {
        return GetTileInfo(tile);
    }
};

struct Cell
{
    CellLayer mid;
    // CellLayer bg;

    [[nodiscard]] bool Empty() const {return mid.tile == Tile{};}
};

class Grid
{
    Array2D<Cell> cells;

    // Maps tile position to their hitbox points, if any.
    // The hitbox is represented as a bit mask. Pass individual bit numbers to `TileHitboxes::GetHitboxPoints(i)`.
    // The `..._min` map only contains a minimal set of points, enough to ensure movement without adding new collisions.
    // The `..._full` map contains enough points to detect any collisions.
    phmap::flat_hash_map<ivec2, int> hitbox_points_min, hitbox_points_full;

    // This can untrim the grid. Make sure to trim it after you're done adding tiles.
    void Resize(ivec2 offset, ivec2 new_size);

    // Removes empty tiles on the sides.
    // Returns the non-negative trim offset for the top-left corners.
    ivec2 Trim();

    // Update hitbox points for the specified rect.
    void RegenerateHitboxPointsInRect(ivec2 pos, ivec2 size);

  public:
    // Maps from the unaligned grid space (origin in the corner) to the world space.
    Xf xf;

    void LoadFromFile(Stream::ReadOnlyData data);

    [[nodiscard]] bool IsEmpty() const;

    [[nodiscard]] const Array2D<Cell> Cells() const {return cells;}

    // Resizes the array to include the specified rect.
    // Then calls `void func(auto &cell)`, where `cell` is `Cell &cell(ivec2 target)`, where `target` is relative so the `pos` parameter of the function itself.
    // `func` is only allowed to modify the specified rect, otherwise an assertion is triggered.
    // Then trims the grid.
    template <typename F>
    void ModifyRegion(ivec2 pos, ivec2 size, F &&func)
    {
        bool should_trim = pos(any) <= 0 || (pos + size)(any) >= size;
        ivec2 offset = clamp_min(-pos, 0);
        Resize(offset, max(clamp_min(pos, 0) + size, offset + cells.size()));
        func([&](ivec2 target) -> Cell &
        {
            ASSERT(target(all) >= 0 && target(all) < size);
            return cells.safe_nonthrowing_at(target + pos + offset);
        });
        if (should_trim)
            offset -= Trim();

        pos += offset;
        ivec2 clamped_pos = clamp_min(pos);
        size -= clamped_pos - pos;
        pos = clamped_pos;
        clamp_var_max(size, cells.size() - pos);

        if (size(all) > 0)
            RegenerateHitboxPointsInRect(pos, size);
    }

    // Uses `ModifyRegion` to remove the specified tile.
    // Does nothing if the tile is out of range.
    void RemoveTile(ivec2 pos);

    // Maps from the grid space (with the origin in the center, unlike `xf`) to the world space.
    [[nodiscard]] Xf GridToWorld() const;
    [[nodiscard]] Xf WorldToGrid() const {return GridToWorld().Inverse();}

    [[nodiscard]] ivec2 OtherToGrid(Xf other, ivec2 pos) const
    {
        return WorldToGrid() * (other * pos);
    }

    void Render(Xf camera) const;

    enum class DebugRenderFlags
    {
        none = 0,
        // The AABB for the cells.
        aabb = 1 << 0,
        // The coodinate axes, using the centered origin.
        coordinate_system = 1 << 1,
        // A dot at the tile origin (in the top-left corner).
        tile_origin = 1 << 2,
        // Hitbox points.
        hitbox_points = 1 << 3,

        all = aabb | coordinate_system | tile_origin | hitbox_points,
    };
    IMP_ENUM_FLAG_OPERATORS_IN_CLASS(DebugRenderFlags)

    void DebugRender(Xf camera, DebugRenderFlags flags) const;
};
