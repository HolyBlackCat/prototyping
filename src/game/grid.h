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
    int corner = -2; // -2 = empty, -1 = full tile, 0 = |/, 1 = \|, 2 = /|, 3 = |\.
};

[[nodiscard]] const TileInfo &GetTileInfo(Tile tile);

namespace TileHitboxes
{
    //   0----1
    //   |    |   \4    5/
    //   |    |    \    /
    //   2----3
    [[nodiscard]] const std::vector<ivec2> &GetHitboxPoints(int index);

    // Returns the points of the full hitbox for the specified corner.
    // `corner`: -2 = empty, -1 = full tile, 0 = |/, 1 = \|, 2 = /|, 3 = |\.
    [[nodiscard]] int GetHitboxPointsMaskFull(int corner);
    // Returns the possible points of the minial hitbox for the specified corner.
    [[nodiscard]] int GetHitboxPointsMaskPossibleMin(int corner);

    // Returns the true minimal hitbox for a tile, a subset of `GetHitboxPointsMaskPossibleMin()`.
    // `possible_min_points_at_offset` is `int possible_min_points_at_offset(ivec2 offset)`.
    // It should return the result of `GetHitboxPointsMaskPossibleMin` on an adjacent tile, at `offset`.
    [[nodiscard]] int GetHitboxPointsMaskPartial(int corner, auto &&possible_min_points_at_offset)
    {
        int mask = GetHitboxPointsMaskPossibleMin(corner);
        if (mask == 0)
            return 0;

        int original_mask = mask;

        // Corner points are removed when neighbor tiles have POSSIBLE points in certain locations.
        //  \|    |/       \|
        // --#::::#--       #.
        //   ::::::         :::.
        //   ::::::         :::::.
        // --#::::#--     --#::::#--
        //  /|    |\        |     \    <-- Bars show the tested directions.

        // Check diagonal neighbors. This works for all tile types.
        if ((mask & 0b0001) && (possible_min_points_at_offset(ivec2(-1,-1)) & 0b0100)) mask &= ~0b0001;
        if ((mask & 0b0010) && (possible_min_points_at_offset(ivec2( 1,-1)) & 0b1000)) mask &= ~0b0010;
        if ((mask & 0b0100) && (possible_min_points_at_offset(ivec2( 1, 1)) & 0b0001)) mask &= ~0b0100;
        if ((mask & 0b1000) && (possible_min_points_at_offset(ivec2(-1, 1)) & 0b0010)) mask &= ~0b1000;

        // Check non-diagnoal neighbors.
        if (original_mask & 0b0001)
        {
            if ((mask & 0b0010) && (possible_min_points_at_offset(ivec2( 1, 0)) & 0b0001)) mask &= ~0b0010;
            if ((mask & 0b1000) && (possible_min_points_at_offset(ivec2( 0, 1)) & 0b0001)) mask &= ~0b1000;
        }
        if (original_mask & 0b0010)
        {
            if ((mask & 0b0001) && (possible_min_points_at_offset(ivec2(-1, 0)) & 0b0010)) mask &= ~0b0001;
            if ((mask & 0b0100) && (possible_min_points_at_offset(ivec2( 0, 1)) & 0b0010)) mask &= ~0b0100;
        }
        if (original_mask & 0b0100)
        {
            if ((mask & 0b0010) && (possible_min_points_at_offset(ivec2( 0,-1)) & 0b0100)) mask &= ~0b0010;
            if ((mask & 0b1000) && (possible_min_points_at_offset(ivec2(-1, 0)) & 0b0100)) mask &= ~0b1000;
        }
        if (original_mask & 0b1000)
        {
            if ((mask & 0b0001) && (possible_min_points_at_offset(ivec2( 0,-1)) & 0b1000)) mask &= ~0b0001;
            if ((mask & 0b0100) && (possible_min_points_at_offset(ivec2( 1, 0)) & 0b1000)) mask &= ~0b0100;
        }

        return mask;
    }
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
    // This will also partially update a 1-tile area around the rect, even if the rect is empty.
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
        if (size(any) <= 0)
            return; // Empty rect.

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

        if (size(all) >= 0) // Sic. Since it updates a 1-tile border around the rect, empty rects are workable too.
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
