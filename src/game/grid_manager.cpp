#include "grid_manager.h"

#include "game/main.h"

static constexpr int aabb_tree_margin = tile_size;
// static constexpr int aabb_tree_margin = 0; // For testing only.

GridManager::GridManager()
    : aabb_tree(ivec2(aabb_tree_margin))
{}

GridManager::aabb_t GridManager::GetGridAabb(const Grid &grid, Xf offset)
{
    Xf xf = grid.GridToWorld() * offset;
    aabb_t ret;
    ret.a = xf * ivec2(0);
    ret.b = xf * (grid.Cells().size() * tile_size);
    sort_two_var(ret.a, ret.b);
    return ret;
}

GridId GridManager::AddGrid(GridObject obj) noexcept
{
    if (grid_ids.IsFull())
    {
        grid_ids.Reserve((grid_ids.Capacity() + 1) * 3 / 2);
        grids.resize(grid_ids.Capacity());
    }

    GridId ret;
    ret.index = grid_ids.InsertAny();
    grids[ret.index] = std::move(obj);
    grids[ret.index]->aabb_node_index = aabb_tree.AddNode(GetGridAabb(grids[ret.index].value().grid), TreeData{.grid_id = ret});

    return ret;
}

void GridManager::RemoveGrid(GridId id) noexcept
{
    auto &obj = grids.at(id.index);
    aabb_tree.RemoveNode(obj.value().aabb_node_index);
    obj.reset();
    grid_ids.EraseUnordered(id.index);
}

const GridObject &GridManager::GetGrid(GridId id) const
{
    return grids.at(id.index).value();
}

void GridManager::Render(Xf camera) const
{
    aabb_t aabb;
    aabb.a = camera * (-screen_size / 2);
    aabb.b = camera * ( screen_size / 2);
    CollideAabbApprox(aabb, [&](GridId id)
    {
        GetGrid(id).grid.Render(camera);
        return false;
    });
}

void GridManager::DebugRender(Xf camera, Grid::DebugRenderFlags flags) const
{
    aabb_t aabb;
    aabb.a = camera * (-screen_size / 2);
    aabb.b = camera * ( screen_size / 2);
    CollideAabbApprox(aabb, [&](GridId id)
    {
        GetGrid(id).grid.DebugRender(camera, flags);
        return false;
    });
}
