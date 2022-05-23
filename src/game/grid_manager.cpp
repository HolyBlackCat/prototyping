#include "grid_manager.h"

#include "game/main.h"
#include "utils/coroutines.h"

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
    std::vector<GridId> ids;
    CollideAabbApprox(aabb, [&](GridId id)
    {
        ids.push_back(id);
        return false;
    });
    std::sort(ids.begin(), ids.end());
    for (GridId id : ids)
        GetGrid(id).grid.Render(camera);
}

void GridManager::DebugRender(Xf camera, Grid::DebugRenderFlags flags) const
{
    aabb_t aabb;
    aabb.a = camera * (-screen_size / 2);
    aabb.b = camera * ( screen_size / 2);
    std::vector<GridId> ids;
    CollideAabbApprox(aabb, [&](GridId id)
    {
        ids.push_back(id);
        return false;
    });
    std::sort(ids.begin(), ids.end());
    for (GridId id : ids)
        GetGrid(id).grid.DebugRender(camera, flags);
}

void GridManager::TickPhysics()
{
    static const auto norm_dirs = []{
        std::array<fvec2, 8> ret;
        for (int i = 0; i < 8; i++)
        {
            ret[i] = fvec2::dir8(i);
            if (i % 2 != 0)
                ret[i] = ret[i].norm();
        }
        return ret;
    }();

    struct Entry
    {
        ivec2 remaining_vel;
        phmap::flat_hash_set<GridId> collision_candidates;


        // This is used by the circular obstruction avoidance algorithm below.
        // (0,0) means that the grid wasn't moved yet.
        ivec2 circular_dir;

        // If true, circular movement in that 8-direction has failed.
        std::array<bool, 8> failed_circular_dirs{};
    };

    struct ImpulseTransferEntry
    {
        phmap::flat_hash_set<GridId> collision_candidates;
    };

    std::vector<GridId> aabb_update_entries;
    phmap::flat_hash_map<GridId, Entry> entries;
    phmap::flat_hash_map<GridId, ImpulseTransferEntry> impulse_entries;

    // Populate entries.
    // Also move unobstructed objects early.
    for (int i = 0; i < GridCount(); i++)
    {
        GridId grid_id = GetGridId(i);

        GridObject &obj = grids[grid_id.index].value();

        Entry new_entry;
        new_entry.remaining_vel = Math::round_with_compensation(obj.vel, obj.vel_lag);
        obj.vel_lag *= 0.99f;

        ImpulseTransferEntry new_impulse_entry;

        // If this entry is going to move, queue it for AABB update.
        if (new_entry.remaining_vel != 0)
            aabb_update_entries.push_back(grid_id);

        // Find potentially colliding grids.
        // Note the final expand by 1 pixel, which lets us reuse the grid list for impulse transfer later.
        aabb_t expanded_aabb = GetGridAabb(obj.grid).ExpandInDir(new_entry.remaining_vel).Expand(ivec2(1));
        CollideAabbApprox(expanded_aabb, [&](GridId other_grid_id)
        {
            if (other_grid_id != grid_id)
                new_impulse_entry.collision_candidates.insert(other_grid_id);
            return false;
        });

        // If this grid can't hit any other grids, move it early.
        if (new_impulse_entry.collision_candidates.empty())
            obj.grid.xf.pos += new_entry.remaining_vel;
        else
            entries.try_emplace(grid_id, std::move(new_entry));

        // Queue for impulse transfer.
        // We add all grids here, even if they have zero velocity, because we store collision candidates in this list.
        impulse_entries.try_emplace(grid_id, std::move(new_impulse_entry));
    }
    // Extend `collision_candidates` to make it symmetric.
    // Yes, we extended the source object hitbox when looking for candidates,
    // but we couldn't extend the candidate hitboxes, so this is still needed.
    for (auto &[id, entry] : impulse_entries)
    {
        for (GridId candidate : entry.collision_candidates)
        {
            if (auto it = impulse_entries.find(candidate); it != impulse_entries.end())
                it->second.collision_candidates.insert(id);
        }
    }
    for (auto &[id, entry] : entries)
        entry.collision_candidates = impulse_entries.at(id).collision_candidates;

    // Advance the objects by a single pixel.
    // First, try both directions at the same time. On failure, try the directions separately.
    while (true)
    {
        bool any_progress = false;

        for (auto it = entries.begin(); it != entries.end();)
        {
            Entry &entry = it->second;
            ivec2 dir = sign(entry.remaining_vel);

            GridObject &obj = grids[it->first.index].value();

            bool can_move = std::none_of(entry.collision_candidates.begin(), entry.collision_candidates.end(), [&](GridId id)
            {
                const GridObject &other_obj = grids[id.index].value();
                return obj.grid.CollidesWithGridWithCustomXfDifference(other_obj.grid, other_obj.grid.WorldToGrid() * Xf::Pos(dir) * obj.grid.GridToWorld(), false);
            });

            if (can_move)
            {
                obj.grid.xf.pos += dir;
                entry.remaining_vel -= dir;
                any_progress = true;
            }
            else
            {
                for (bool axis_dir_index : initial_dir_for_physics_tick ? std::array{true, false} : std::array{false, true})
                {
                    ivec2 axis_dir;
                    axis_dir[axis_dir_index] = dir[axis_dir_index];

                    if (!axis_dir)
                        continue;

                    bool axis_can_move = std::none_of(entry.collision_candidates.begin(), entry.collision_candidates.end(), [&](GridId id)
                    {
                        const GridObject &other_obj = grids[id.index].value();
                        return obj.grid.CollidesWithGridWithCustomXfDifference(other_obj.grid, other_obj.grid.WorldToGrid() * Xf::Pos(axis_dir) * obj.grid.GridToWorld(), false);
                    });

                    if (axis_can_move)
                    {
                        obj.grid.xf.pos += axis_dir;
                        entry.remaining_vel -= axis_dir;
                        any_progress = true;
                    }
                }
            }

            if (entry.remaining_vel == 0)
                it = entries.erase(it);
            else
                it++;
        }

        if (!any_progress)
            break;
    }

    // Try to resolve any circular obstructions.
    while (true)
    {
        bool any_progress = false;

        // Coroutine. Pauses to show a working scenario. Stops when there's no more valid scenarios.
        auto ProcessObject = [&](auto &ProcessObject, GridId id, int proposed_dir) -> Coroutine<>
        {
            Entry &entry = entries.at(id);
            if (entry.circular_dir)
                co_return; // The object was already moved.
            if (entry.failed_circular_dirs[proposed_dir])
                co_return; // We already tried this direction, and it's obstructed.

            GridObject &obj = grids.at(id.index).value();

            // Temporarily add the offset to the position.
            entry.circular_dir = ivec2::dir8(proposed_dir);
            obj.grid.xf.pos += entry.circular_dir;
            entry.remaining_vel -= entry.circular_dir;


            struct Collision
            {
                GridId id;
                std::vector<int> possible_dirs;
            };

            std::vector<Collision> collisions;
            bool fully_stuck = false;

            for (GridId candidate_id : entry.collision_candidates)
            {
                auto candidate_iter = entries.find(candidate_id);
                // Note that we have to check `candidate_iter->second.remaining_vel`, because this algorithm removes velocity-less entries separately, later.
                bool candidate_is_movable = candidate_iter != entries.end() && candidate_iter->second.circular_dir == 0 && candidate_iter->second.remaining_vel;

                const GridObject &candidate = GetGrid(candidate_id);
                bool collides = obj.grid.CollidesWithGridWithCustomXfDifference(candidate.grid, candidate.grid.WorldToGrid() * obj.grid.GridToWorld(), false);

                if (collides)
                {
                    if (!candidate_is_movable)
                    {
                        fully_stuck = true;
                        break;
                    }
                    else
                    {
                        Collision new_collision;
                        new_collision.id = candidate_id;
                        new_collision.possible_dirs.reserve(5);
                        for (int delta : {0, 1, -1, 2, -2})
                        {
                            // If `remaining_vel` allows us to move in this direction...
                            if (sign(candidate_iter->second.remaining_vel) == sign(ivec2::dir8(proposed_dir + delta)))
                                new_collision.possible_dirs.push_back(mod_ex(proposed_dir + delta, 8));
                        }
                        collisions.push_back(std::move(new_collision));
                    }
                }
            }

            if (fully_stuck)
            {
                entry.failed_circular_dirs[proposed_dir] = true;
            }
            else if (!collisions.empty())
            {
                auto CollisionCoro = [&](std::size_t i) -> Coroutine<>
                {
                    for (int dir : collisions[i].possible_dirs)
                    {
                        auto coro = ProcessObject(ProcessObject, collisions[i].id, dir);
                        while (coro())
                            co_await std::suspend_always{}; // Propagate success.
                    }
                };

                std::vector<Coroutine<>> coroutines(collisions.size());
                coroutines.front() = CollisionCoro(0);

                std::size_t pos = 0;
                while (true)
                {
                    if (coroutines[pos]())
                    {
                        if (pos + 1 == coroutines.size())
                        {
                            // Success.
                            co_await std::suspend_always{};
                        }
                        else
                        {
                            // Success, but we need to resolve more collisions.
                            pos++;
                            coroutines[pos] = CollisionCoro(pos);
                        }
                    }
                    else
                    {
                        if (pos == 0)
                            break; // We're done.

                        // Failure, need to update the previous collision.
                        pos--;
                    }
                }
            }
            else
            {
                // Success!
                co_await std::suspend_always{};
            }

            // Undo the movement.
            obj.grid.xf.pos -= entry.circular_dir;
            entry.remaining_vel += entry.circular_dir;
            entry.circular_dir = ivec2();
        };

        for (auto &[id, entry] : entries)
        {
            auto TryOffset = [&, &id = id](ivec2 offset) -> bool
            {
                auto coro = ProcessObject(ProcessObject, id, offset.angle8_sign());
                bool success = coro();
                if (success)
                {
                    // Reset stuff. On failure it happens automatically.
                    for (auto &[id, entry] : entries)
                    {
                        entry.failed_circular_dirs = {};
                        entry.circular_dir = {};
                    }
                }
                return success;
            };

            if (entry.remaining_vel(all) != 0 && TryOffset(sign(entry.remaining_vel)))
            {
                // Moved diagonally.
                any_progress = true;
            }
            else
            {
                // Couldn't move diagnoally, try both directions separately.
                if (ivec2 offset = sign(entry.remaining_vel * ivec2::dir4(initial_dir_for_physics_tick)); offset && TryOffset(offset))
                    any_progress = true;
                if (ivec2 offset = sign(entry.remaining_vel * ivec2::dir4(!initial_dir_for_physics_tick)); offset && TryOffset(offset))
                    any_progress = true;
            }
        }

        if (any_progress)
        {
            // Erase entries with no remaining velocity. We couldn't do it earlier because it invalidates the iterators.
            for (auto it = entries.begin(); it != entries.end();)
            {
                if (it->second.remaining_vel == 0)
                    it = entries.erase(it);
                else
                    ++it;
            }
        }
        else
        {
            break;
        }
    }

    // Update AABBs.
    for (const auto &id : aabb_update_entries)
        ModifyGrid(id, [](GridObject &){});

    // Perform impulse transfer.
    // First, sort entries by speed.
    std::vector<decltype(impulse_entries)::value_type *> impulse_entries_sorted;
    impulse_entries_sorted.reserve(impulse_entries.size());
    for (auto &entry : impulse_entries)
        impulse_entries_sorted.push_back(&entry);
    std::sort(impulse_entries_sorted.begin(), impulse_entries_sorted.end(), [&](const auto *a, const auto *b)
    {
        return grids.at(a->first.index).value().vel.len_sqr() > grids.at(b->first.index).value().vel.len_sqr();
    });
    for (auto *entry_pair : impulse_entries_sorted)
    {
        GridId id = entry_pair->first;
        ImpulseTransferEntry &entry = entry_pair->second;

        GridObject &obj = grids.at(id.index).value();

        for (GridId other_id : entry.collision_candidates)
        {
            GridObject &other_obj = grids.at(other_id.index).value();

            if (obj.infinite_mass && other_obj.infinite_mass)
                continue;

            if (fvec2 vel_delta = obj.vel - other_obj.vel)
            {
                int dir_index_0 = vel_delta.angle8_floor() - 1;

                // Whether `vel_delta` is one of the 8 main directions.
                bool dir_is_8_aligned = vel_delta(any) == 0 || abs(vel_delta.x) == abs(vel_delta.y);

                auto CollidesWithDir = [&](int dir)
                {
                    return obj.grid.CollidesWithGridWithCustomXfDifference(other_obj.grid, other_obj.grid.WorldToGrid() * Xf::Pos(ivec2::dir8(dir)) * obj.grid.GridToWorld(), false);
                };

                bool hit_1 = CollidesWithDir(dir_index_0 + 1);
                bool hit_2 = CollidesWithDir(dir_index_0 + 2);

                // If we have a collision, perform impulse transfer.
                if (hit_1 || (!dir_is_8_aligned && hit_2))
                {
                    // Determine the best movement direction.
                    // If null, the objects can't move relative to each other.
                    std::optional<int> best_dir;

                    if (dir_is_8_aligned)
                    {
                        if (!hit_1)
                            best_dir = dir_index_0 + 1;
                        else if (hit_2 != CollidesWithDir(dir_index_0))
                            best_dir = dir_index_0 + (hit_2 ? 0 : 2);
                    }
                    else
                    {
                        if (hit_1 != hit_2)
                        {
                            best_dir = dir_index_0 + (hit_2 ? 1 : 2);
                        }
                        else
                        {
                            // Check which of the two dirs is closer to our velocity.
                            bool prefer_dir_2 = vel_delta /dot/ norm_dirs[mod_ex(dir_index_0 + 2, 8)] > vel_delta /dot/ norm_dirs[mod_ex(dir_index_0 + 1, 8)];

                            int preferred_dir = dir_index_0 + (prefer_dir_2 ? 3 : 0);
                            int backup_dir = dir_index_0 + (prefer_dir_2 ? 0 : 3);

                            if (!CollidesWithDir(preferred_dir))
                                best_dir = preferred_dir;
                            else if (!CollidesWithDir(backup_dir))
                                best_dir = backup_dir;
                        }
                    }

                    // Which body changes velocity: 0 = self, 1 = other.
                    float mass_factor = other_obj.infinite_mass ? 0 : obj.infinite_mass ? 1 : obj.grid.Mass() / float(obj.grid.Mass() + other_obj.grid.Mass());

                    if (!best_dir)
                    {
                        obj.vel = other_obj.vel = other_obj.vel + vel_delta * mass_factor;
                        // obj.vel_lag = other_obj.vel_lag = (obj.vel_lag + other_obj.vel_lag) / 2;
                    }
                    else
                    {
                        fvec2 normal = norm_dirs[mod_ex(*best_dir + 2, 8)];
                        fvec2 vel_delta_proj = Math::project_onto_line_norm(vel_delta, normal);

                        obj.vel -= vel_delta_proj * (1 - mass_factor);
                        other_obj.vel += vel_delta_proj * mass_factor;

                        // // Try to sync the velocity lag.
                        // fvec2 vel_lag_delta_proj = Math::project_onto_line_norm(obj.vel_lag - other_obj.vel_lag, normal);
                        // obj.vel_lag -= vel_lag_delta_proj * (1 - mass_factor);
                        // other_obj.vel_lag += vel_lag_delta_proj * mass_factor;
                    }
                }
            }

            // Erase this ID from the candidates of the other object, to avoid checking it twice.
            impulse_entries.at(other_id).collision_candidates.erase(id);
        }
    }

    // Update the preferred movement direction for the next tick.
    initial_dir_for_physics_tick = !initial_dir_for_physics_tick;
}
