#pragma once

#include "game/grid.h"
#include "utils/aabb_tree.h"


// Those should only exist inside of a `GridManager`.
struct GridObject
{
    Grid grid;
    fvec2 vel;
    fvec2 vel_lag;

    // Don't modify. This is set automatically by the grid manager.
    int aabb_node_index = -1;
};

struct GridId
{
    int index = -1;

    [[nodiscard]] friend bool operator<=>(const GridId &, const GridId &) = default;
};
template <>
struct std::hash<GridId>
{
    std::size_t operator()(const GridId &id) const
    {
        return std::hash<int>{}(id.index);
    }
};

class GridManager
{
public:
    struct TreeData
    {
        GridId grid_id;
    };

    using aabb_tree_t = AabbTree<ivec2, TreeData>;
    using aabb_t = aabb_tree_t::Aabb;

private:
    aabb_tree_t aabb_tree;

    SparseSet<int> grid_ids;

    std::vector<std::optional<GridObject>> grids;

    // This oscillates between 0 and 1 every time you call `TickPhysics()`.
    // It represents the initial axis (X or Y) that the physics tick uses.
    bool initial_dir_for_physics_tick = 0;

public:
    GridManager();

    GridManager(const GridManager &) = delete;
    GridManager &operator=(const GridManager &) = delete;

    [[nodiscard]] static aabb_t GetGridAabb(const Grid &grid, Xf offset = {});

    GridId AddGrid(GridObject obj) noexcept;
    void RemoveGrid(GridId id) noexcept;

    [[nodiscard]] const GridObject &GetGrid(GridId id) const;

    [[nodiscard]] int GridCount() const {return grid_ids.ElemCount();}
    [[nodiscard]] GridId GetGridId(int index) const {return {.index = grid_ids.GetElem(index)};}

    // Temporarily gives you a non-const reference to a grid to modify it.
    // `func` is `void func(GridObject &obj)`.
    template <typename F>
    void ModifyGrid(GridId id, F &&func)
    {
        GridObject &obj = grids[id.index].value();
        func(obj);
        aabb_tree.ModifyNode(obj.aabb_node_index, GetGridAabb(obj.grid), round_maxabs(obj.vel));
    }

    [[nodiscard]] const aabb_tree_t &AabbTree() const {return aabb_tree;}

    // Finds all other grids colliding with this one.
    // `func` is `bool func(GridId id)`. If it returns true, the function stops immediately and also returns true.
    template <typename F>
    bool CollideGrid(GridId id, Xf offset, bool full, F &&func) const
    {
        const Grid &grid = GetGrid(id).grid;
        return aabb_tree.CollideAabb(GetGridAabb(grid, offset), [&](int node_id)
        {
            GridId grid_id = aabb_tree.GetNodeUserData(node_id).grid_id;
            if (grid_id == id)
                return false; // Skip this grid.
            return grid.CollidesWithGridWithOffsets(offset, GetGrid(grid_id).grid, {}, full) && bool(func(std::as_const(grid_id)));
        });
    }

    // Finds all other grids colliding with this one.
    // `func` is `bool func(GridId id)`. If it returns true, the function stops immediately and also returns true.
    template <typename F>
    bool CollideExternalGrid(const Grid &grid, Xf offset, bool full, F &&func) const
    {
        return aabb_tree.CollideAabb(GetGridAabb(grid, offset), [&](int id)
        {
            GridId grid_id = aabb_tree.GetNodeUserData(id).grid_id;
            return grid.CollidesWithGridWithOffsets(offset, GetGrid(grid_id).grid, {}, full) && bool(func(std::as_const(grid_id)));
        });
    }

    // Finds all grids approximately colliding with an AABB.
    // `func` is `bool func(GridId id)`. If it returns true, the function stops immediately and also returns true.
    template <typename F>
    bool CollideAabbApprox(aabb_t aabb, F &&func) const
    {
        return aabb_tree.CollideAabb(aabb, [&](int id)
        {
            return func(aabb_tree.GetNodeUserData(id).grid_id);
        });
    }

    // Finds all grids approximately colliding with a point.
    // `func` is `bool func(GridId id)`. If it returns true, the function stops immediately and also returns true.
    template <typename F>
    bool CollidePointApprox(ivec2 point, F &&func) const
    {
        return aabb_tree.CollidePoint(point, [&](int id)
        {
            return func(aabb_tree.GetNodeUserData(id).grid_id);
        });
    }

    void Render(Xf camera) const;
    void DebugRender(Xf camera, Grid::DebugRenderFlags flags) const;

    void TickPhysics();
};
