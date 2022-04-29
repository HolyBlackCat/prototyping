#pragma once

#include "game/xf.h"

constexpr int tile_size = 12;

enum class Tile
{
    air,
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

struct Grid
{
    // Maps from the unaligned grid space to the world space.
    Xf xf;

    Array2D<Cell> cells;

    [[nodiscard]] bool IsEmpty() const;

    [[nodiscard]] Xf GridToWorld() const;

    void LoadFromFile(Stream::ReadOnlyData data);

    // Removes empty tiles on the sides.
    void Trim();

    void Render(Xf camera) const;
};
