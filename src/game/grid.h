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
    int mass = 100; // This is integral to avoid rounding errors.
};

[[nodiscard]] const TileInfo &GetTileInfo(Tile tile);

namespace TileHitboxes
{
    // Note that the collision code runs in triple resolution, because otherwise we're unable to achieve zero separation between adjacent triangle tiles: |\ \|
    // You could think that an asymmetric collision algorithm could work (points of diagonals misaliged relative to the true hitbox by 1 pixel), but that actually doesn't work:
    // it causes minor disagreements between minimal and full hitbox here and there. Yes, I tried both cases (1. points offset inwards, 2. triangle tile hitboxes offset inwards).
    // A 2x resolution would kinda work, but the conversion to and from 1x resolution would be asymmetrical, which isn't good.

    constexpr int highres_factor = 3;

    // Converts a point from 3x to normal resolution.
    [[nodiscard]] inline ivec2 ToNormalRes(ivec2 point) {return div_ex(point, highres_factor);}
    // Converts a point from normal to highres_factorx resolution.
    [[nodiscard]] inline ivec2 ToHighRes(ivec2 point) {return point * highres_factor + 1;}

    // Outputs four high-res corners of a normal-resolution point.
    // `func` is `bool func(ivec2)`. If it returns true, the function stops.
    // Both the point and the corners are pixel-centered.
    [[nodiscard]] bool ToHighResCorners(ivec2 point, auto &&func)
    {
        return
            func(point * highres_factor) ||
            func(point * highres_factor + ivec2(highres_factor - 1, 0)) ||
            func(point * highres_factor + highres_factor - 1) ||
            func(point * highres_factor + ivec2(0, highres_factor - 1));
    }

    // The numbering of points in hitbox point masks is as follows:
    //   [ 0][ 4]------------------------[ 9][ 1]
    //   [ 8]    [13]                [12]    [ 5]
    //    |  [15]    [13]        [12]    [14]  |
    //    |      [15]    [13][12]    [14]      |
    //    |          [15][12][13][14]          |
    //    |          [12][15][14][13]          |
    //    |      [12]    [14][15]    [13]      |
    //    |  [12]    [14]        [15]    [13]  |
    //   [ 7]    [14]                [15]    [10]
    //   [ 3][11]------------------------[ 6][ 2]

    // Given a mask bit index, returns a list of points for it (see the picture above).
    [[nodiscard]] const std::vector<ivec2> &GetHitboxPointsHighRes(int index);

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

        // Corner points are removed when neighbor tiles have POSSIBLE points in certain locations.
        //   #  #                #  #   ·
        //    \ |\              /| /    ·
        //     \| \            / |/     ·
        //   #--#··#··········#··#--#   ·
        //    \ :                : /    ·
        //     \:                :/     ·
        //      #                #      ·
        //      :                :      ·
        //      :                :      ·
        //      #                #      ·
        //     /:                :\     ·
        //    / :                : \    ·
        //   #--#··#··········#··#--#   ·
        //     /| /            \ |\     ·
        //    / |/              \| \    ·
        //   #  #                #  #   ·

        // Check diagonal neighbors. This works for all tile types.
        if ((mask & 0b0001) && (possible_min_points_at_offset(ivec2(-1,-1)) & 0b0100)) mask &= ~0b0001;
        if ((mask & 0b0010) && (possible_min_points_at_offset(ivec2( 1,-1)) & 0b1000)) mask &= ~0b0010;
        if ((mask & 0b0100) && (possible_min_points_at_offset(ivec2( 1, 1)) & 0b0001)) mask &= ~0b0100;
        if ((mask & 0b1000) && (possible_min_points_at_offset(ivec2(-1, 1)) & 0b0010)) mask &= ~0b1000;

        // Check non-diagnoal neighbors.
        if ((mask & 0b0001'0001) && (possible_min_points_at_offset(ivec2( 0,-1)) & 0b1000)) mask &= ~0b0001'0001;
        if ((mask & 0b0010'0010) && (possible_min_points_at_offset(ivec2( 1, 0)) & 0b0001)) mask &= ~0b0010'0010;
        if ((mask & 0b0100'0100) && (possible_min_points_at_offset(ivec2( 0, 1)) & 0b0010)) mask &= ~0b0100'0100;
        if ((mask & 0b1000'1000) && (possible_min_points_at_offset(ivec2(-1, 0)) & 0b0100)) mask &= ~0b1000'1000;

        if ((mask & 0b0001'0000'0001) && (possible_min_points_at_offset(ivec2(-1, 0)) & 0b0010)) mask &= ~0b0001'0000'0001;
        if ((mask & 0b0010'0000'0010) && (possible_min_points_at_offset(ivec2( 0,-1)) & 0b0100)) mask &= ~0b0010'0000'0010;
        if ((mask & 0b0100'0000'0100) && (possible_min_points_at_offset(ivec2( 1, 0)) & 0b1000)) mask &= ~0b0100'0000'0100;
        if ((mask & 0b1000'0000'1000) && (possible_min_points_at_offset(ivec2( 0, 1)) & 0b0001)) mask &= ~0b1000'0000'1000;

        return mask;
    }

    // Checks collision of `point` against a `corner`-shaped tile.
    // `point` is assumed to be in the tile AABB, otherwise the result is meaningless.
    // `point` is in double-resolution, and is pixel-centered.
    [[nodiscard]] bool TileCollidesWithPointHighRes(int corner, ivec2 point);
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

    [[nodiscard]] int Mass() const {return mid.Info().mass;}
};

class Grid
{
    Array2D<Cell> cells;

    // Maps tile position to their hitbox points, if any.
    // The hitbox is represented as a bit mask. Pass individual bit numbers to `TileHitboxes::GetHitboxPoints(i)`.
    // The `..._min` map only contains a minimal set of points, enough to ensure movement without adding new collisions.
    // The `..._full` map contains enough points to detect any collisions.
    phmap::flat_hash_map<ivec2, int> hitbox_points_min, hitbox_points_full;

    // The total mass of the grid.
    int mass = 0;

    // This can untrim the grid. Make sure to trim it after you're done adding tiles.
    void Resize(ivec2 offset, ivec2 new_size);

    // Removes empty tiles on the sides.
    // Returns the non-negative trim offset for the top-left corners.
    ivec2 Trim();

    // Update hitbox points for the specified rect.
    // This will also partially update a 1-tile area around the rect, even if the rect is empty.
    void RegenerateHitboxPointsInRect(ivec2 pos, ivec2 size);

  public:
    // Maps from the unaligned grid space (origin in the center) to the world space.
    Xf xf;

    void LoadFromFile(Stream::ReadOnlyData data);

    [[nodiscard]] bool IsEmpty() const;

    [[nodiscard]] const Array2D<Cell> Cells() const {return cells;}

    [[nodiscard]] int Mass() const {return mass;}

    // Resizes the array to include the specified rect.
    // Then calls `void func(auto &&cell)`, where `cell` is `Cell &cell(ivec2 target)`, where `target` is relative so the `pos` parameter of the function itself.
    // `func` is only allowed to modify the specified rect, otherwise an assertion is triggered.
    // Then trims the grid.
    template <typename F>
    void ModifyRegion(ivec2 pos, ivec2 size, F &&func)
    {
        if (size(any) <= 0)
            return; // Empty rect.

        // Determine the starting mass of the region.
        int starting_mass = 0;
        for (ivec2 pos : clamp_min(pos, 0) <= vector_range < clamp_max(pos + size, cells.size()))
            starting_mass += cells.safe_throwing_at(pos).Mass();

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

        // Update the hitbox points.
        if (size(all) >= 0) // Sic. Since it updates a 1-tile border around the rect, empty rects are workable too.
            RegenerateHitboxPointsInRect(pos, size);

        // Update the mass.
        mass -= starting_mass;
        for (ivec2 pos : pos <= vector_range < size)
            mass += cells.safe_throwing_at(pos).Mass();
    }

    // Uses `ModifyRegion` to remove the specified tile.
    // Does nothing if the tile is out of range.
    void RemoveTile(ivec2 pos);

    // Maps from the grid space (with the origin in the corner, unlike `xf`) to the world space.
    [[nodiscard]] Xf GridToWorld() const;
    [[nodiscard]] Xf WorldToGrid() const {return GridToWorld().Inverse();}

    [[nodiscard]] ivec2 OtherToGrid(Xf other, ivec2 pos) const
    {
        return WorldToGrid() * other * pos;
    }
    [[nodiscard]] ivec2 OtherToGridPixelCentered(Xf other, ivec2 pos) const
    {
        return (WorldToGrid() * other).TransformPixelCenteredPoint(pos);
    }

    // `point` is in high resolution, pixel-centered, in grid space (with the origin in the corner).
    // Use `WorldToGrid()`, not `xf.inverse()`.
    [[nodiscard]] bool CollidesWithPointInGridSpaceHighRes(ivec2 point) const;
    // Same, but the `point` is in normal resolution. Performs multiple samples to make sure connections like []\| are airtight.
    [[nodiscard]] bool CollidesWithPointInGridSpace(ivec2 point) const;
    // Same, but in world space.
    [[nodiscard]] bool CollidesWithPointInWorldSpace(ivec2 point) const
    {
        return CollidesWithPointInGridSpace(WorldToGrid().TransformPixelCenteredPoint(point));
    }

    // Checks collisiton between two grids.
    // Ignores grid XFs completely, only respects `this_to_other`.
    // If `full` is false, does an incomplete test that only checks the borders.
    // Experiments show that in some case the border is 1 pixel thick, but the diagonal 1-pixel movement is safe.
    [[nodiscard]] bool CollidesWithGridWithCustomXfDifference(const Grid &other, Xf this_to_other, bool full) const;
    // Same, but respects our XF and their XF.
    [[nodiscard]] bool CollidesWithGrid(const Grid &other, bool full) const
    {
        return CollidesWithGridWithCustomXfDifference(other, other.WorldToGrid() * GridToWorld(), full);
    }
    // Same, but also lets you add custom offsets.
    [[nodiscard]] bool CollidesWithGridWithOffsets(Xf our_offset, const Grid &other, Xf other_offset, bool full) const
    {
        return CollidesWithGridWithCustomXfDifference(other, other_offset.Inverse() * other.WorldToGrid() * GridToWorld() * our_offset, full);
    }

    void Render(Xf camera, std::optional<fvec3> color = {}) const;

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
        hitbox_points_full = 1 << 3,
        hitbox_points_min  = 1 << 4,
        hitbox_points = hitbox_points_full | hitbox_points_min,

        all = aabb | coordinate_system | tile_origin | hitbox_points,
    };
    IMP_ENUM_FLAG_OPERATORS_IN_CLASS(DebugRenderFlags)

    void DebugRender(Xf camera, DebugRenderFlags flags) const;
};
