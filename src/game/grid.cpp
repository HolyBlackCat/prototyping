#include "grid.h"

#include "game/main.h"

static const auto tile_info = []{
    TileInfo ret[] = {
        TileInfo{ .tile = Tile::empty,  .mass =   0 },
        TileInfo{ .tile = Tile::wall,   .mass = 100, .render = TileRenderFlavor::quarter, .tex_index = 0, .corner = -1, .merge_group = 1 },
        TileInfo{ .tile = Tile::wall_a, .mass =  50, .render = TileRenderFlavor::quarter, .tex_index = 0, .corner =  0, .merge_group = 1 },
        TileInfo{ .tile = Tile::wall_b, .mass =  50, .render = TileRenderFlavor::quarter, .tex_index = 0, .corner =  1, .merge_group = 1 },
        TileInfo{ .tile = Tile::wall_c, .mass =  50, .render = TileRenderFlavor::quarter, .tex_index = 0, .corner =  2, .merge_group = 1 },
        TileInfo{ .tile = Tile::wall_d, .mass =  50, .render = TileRenderFlavor::quarter, .tex_index = 0, .corner =  3, .merge_group = 1 },
    };
    if (std::size(ret) != std::to_underlying(Tile::_count))
        throw std::runtime_error(FMT("Wrong size of the tile info array: {}, but expected {}.", std::size(ret), std::to_underlying(Tile::_count)));
    for (int i = 0; i < std::to_underlying(Tile::_count); i++)
    {
        if (ret[i].tile != Tile(i))
            throw std::runtime_error(FMT("Wrong tile enum in the tile info array: at index {}.", i));
    }
    return std::to_array(ret);
}();

const TileInfo &GetTileInfo(Tile tile)
{
    if (tile < Tile{} || tile >= Tile::_count)
        throw std::runtime_error(FMT("Tile index {} is out of range.", std::to_underlying(tile)));
    return tile_info[int(tile)];
}

namespace TileHitboxes
{
    static constexpr int highres_tile_size = tile_size * highres_factor;

    // On the diagnoals, we create points sparsely (with `highres_factor` as the step).
    // This looks safe, but if something goes wrong, there's non-sparse version in the git history.
    static const std::vector<std::vector<ivec2>> hitbox_point_patterns_HighRes = {
        {ivec2(0,0)},                                     //  0 = ['   ]
        {ivec2(highres_tile_size-1,0)},                   //  1 = [   ']
        {ivec2(highres_tile_size-1)},                     //  2 = [   .]
        {ivec2(0,highres_tile_size-1)},                   //  3 = [.   ]
        {ivec2(1,0)},                                     //  4 = ['>  ]
        {ivec2(highres_tile_size-1,1)},                   //  5 = [  v']
        {ivec2(highres_tile_size-2,highres_tile_size-1)}, //  6 = [  <.]
        {ivec2(0,highres_tile_size-2)},                   //  7 = [.^  ]
        {ivec2(0,1)},                                     //  8 = ['v  ]
        {ivec2(highres_tile_size-2,0)},                   //  9 = [  <']
        {ivec2(highres_tile_size-1,highres_tile_size-2)}, // 10 = [  ^.]
        {ivec2(1,highres_tile_size-1)},                   // 11 = [.>  ]
        []{                                               // 12 = [ /  ]
            std::vector<ivec2> ret;
            for (int i = 1 + highres_factor/2; i < highres_tile_size - 2; i += highres_factor)
                ret.push_back(ivec2(highres_tile_size - i - 2, i));
            return ret;
        }(),
        []{                                               // 13 = [  \ ]
            std::vector<ivec2> ret;
            for (int i = 1 + highres_factor/2; i < highres_tile_size - 2; i += highres_factor)
                ret.push_back(ivec2(i+1,i));
            return ret;
        }(),
        []{                                               // 14 = [ /  ]
            std::vector<ivec2> ret;
            for (int i = 1 + highres_factor/2; i < highres_tile_size - 2; i += highres_factor)
                ret.push_back(ivec2(highres_tile_size - i - 1, i + 1));
            return ret;
        }(),
        []{                                               // 15 = [  \ ]
            std::vector<ivec2> ret;
            for (int i = 1 + highres_factor/2; i < highres_tile_size - 2; i += highres_factor)
                ret.push_back(ivec2(i,i+1));
            return ret;
        }(),
    };

    // Prints the hitbox points as an array.
    [[maybe_unused]] static void DebugPrintHitboxPoints()
    {
        Array2D<int> arr(ivec2(tile_size * TileHitboxes::highres_factor), -1);
        for (int i = 0; i < 16; i++)
        {
            for (ivec2 pos : TileHitboxes::GetHitboxPointsHighRes(i))
                arr.safe_throwing_at(pos) = i;
        }
        for (ivec2 pos : vector_range(arr.size()))
        {
            int x = arr.safe_throwing_at(pos);
            std::cout << (x == -1 ? "  ." : FMT("{:{}d}", x, TileHitboxes::highres_factor) );
            if (pos.x == tile_size * TileHitboxes::highres_factor - 1)
                std::cout << '\n';
        }
    }

    const std::vector<ivec2> &GetHitboxPointsHighRes(int index)
    {
        if (index < 0 || std::size_t(index) >= std::size(hitbox_point_patterns_HighRes))
            throw std::runtime_error(FMT("Point-hitbox index is out of range: {}", index));
        return hitbox_point_patterns_HighRes[index];
    }

    int GetHitboxPointsMaskFull(int corner)
    {
        // -1 = full tile, 0 = |/, 1 = \|, 2 = /|, 3 = |\.
        switch (corner)
        {
          case -2:
            return 0;
          case -1:
            return 0b0000'0000'0000'1111;
          case 0:
            return 0b0001'0010'1000'0001;
          case 1:
            return 0b0010'0100'0001'0010;
          case 2:
            return 0b0100'1000'0010'0100;
          case 3:
            return 0b1000'0001'0100'1000;
        }
        throw std::runtime_error("Invalid corner id.");
    }

    int GetHitboxPointsMaskPossibleMin(int corner)
    {
        return GetHitboxPointsMaskFull(corner) & 0b0000'1111'1111'1111;
    }

    bool TileCollidesWithPointHighRes(int corner, ivec2 point)
    {
        // Diagonals are exclusive, to agree with the point hitboxes.

        switch (corner)
        {
          case -2:
            return false;
          case -1:
            return true;
          case 0:
            return point.sum() < highres_tile_size - 1;
          case 1:
            return point.x > point.y;
          case 2:
            return point.sum() > highres_tile_size - 1;
          case 3:
            return point.x < point.y;
        }
        throw std::runtime_error("Invalid corner id.");
    }

    bool CornerHasEdge(int corner, int dir)
    {
        dir = mod_ex(dir, 4);
        if (corner == -2)
            return false;
        if (corner == -1)
            return true;
        corner = mod_ex(corner, 4);
        if (dir == 0)
            return corner == 1 || corner == 2;
        if (dir == 1)
            return corner == 2 || corner == 3;
        if (dir == 2)
            return corner == 3 || corner == 0;
        if (dir == 3)
            return corner == 0 || corner == 1;
        ASSERT(false, "Invalid corner and/or direction.");
        return false;
    }
}

void Grid::Resize(ivec2 offset, ivec2 new_size)
{
    if (new_size(any) == 0)
        new_size = {};

    // Realign position to avoid visual movement.
    xf.pos += xf.Matrix() * (-offset * tile_size + new_size * tile_size / 2 - cells.size() * tile_size / 2);

    cells.resize(new_size, offset);

    { // Trim hitbox points outside of the new rect, and offset the remaining points.
        // If this is true, we copy the map to a new location to offset the points, ignoring the out-of-range points.
        // Otherwise we remove the out-of-range points in place.
        bool need_offset = offset != 0;

        for (auto *map : {&hitbox_points_full, &hitbox_points_min})
        {
            phmap::flat_hash_map<ivec2, int> new_map;
            if (need_offset)
                new_map.reserve(map->size());

            for (auto it = map->begin(); it != map->end();)
            {
                ivec2 rel_pos = it->first + offset;
                if (rel_pos(any) < 0 || rel_pos(any) >= new_size)
                {
                    if (need_offset)
                        it++;
                    else
                        it = map->erase(it);
                }
                else
                {
                    if (need_offset)
                        new_map.try_emplace(rel_pos, it->second);

                    ++it;
                }
            }

            if (need_offset)
                *map = std::move(new_map);
        }
    }
}

ivec2 Grid::Trim()
{
    if (IsEmpty())
        return {}; // No cells.

    int top = 0;
    for (; top < cells.size().y; top++)
    {
        bool found = false;
        for (int x = 0; x < cells.size().x; x++)
        {
            if (!cells.safe_nonthrowing_at(vec(x, top)).Empty())
            {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    if (top >= cells.size().y)
    {
        // All cells are empty.
        cells = {};
        return {};
    }

    int bottom = cells.size().y - 1;
    for (;; bottom--)
    {
        bool found = false;
        for (int x = 0; x < cells.size().x; x++)
        {
            if (!cells.safe_nonthrowing_at(vec(x, bottom)).Empty())
            {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    int left = 0;
    for (;; left++)
    {
        bool found = false;
        for (int y = top; y <= bottom; y++)
        {
            if (!cells.safe_nonthrowing_at(vec(left, y)).Empty())
            {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    int right = cells.size().x - 1;
    for (;; right--)
    {
        bool found = false;
        for (int y = top; y <= bottom; y++)
        {
            if (!cells.safe_nonthrowing_at(vec(right, y)).Empty())
            {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    if (ivec2(left, top) == ivec2() && ivec2(right, bottom) == cells.size() - 1)
        return {}; // No changes needed.

    Resize(-ivec2(left, top), ivec2(right - left, bottom - top) + 1);

    return ivec2(left, top);
}

void Grid::RegenerateHitboxPointsInRect(ivec2 pos, ivec2 size)
{
    // Update the full hitbox.
    for (ivec2 tile_pos : vector_range(size) + pos)
    {
        int mask = TileHitboxes::GetHitboxPointsMaskFull(cells.safe_throwing_at(tile_pos).mid.Info().corner);
        if (mask == 0)
            hitbox_points_full.erase(tile_pos);
        else
            hitbox_points_full.insert_or_assign(tile_pos, mask);
    }

    // Update the minimal hitbox.
    for (ivec2 tile_pos : clamp_min(pos - 1) <= vector_range < clamp_max(pos + size + 1, cells.size()))
    {
        int mask = TileHitboxes::GetHitboxPointsMaskPartial(cells.safe_throwing_at(tile_pos).mid.Info().corner,
            [&](ivec2 offset)
            {
                ivec2 this_pos = tile_pos + offset;
                if (!cells.pos_in_range(this_pos))
                    return 0;
                return TileHitboxes::GetHitboxPointsMaskPossibleMin(cells.safe_throwing_at(this_pos).mid.Info().corner);
            }
        );
        if (mask == 0)
            hitbox_points_min.erase(tile_pos);
        else
            hitbox_points_min.insert_or_assign(tile_pos, mask);
    }
}

void Grid::LoadFromFile(Stream::ReadOnlyData data)
{
    Json json(data.string(), 32);

    try
    {
        auto input_layer = Tiled::LoadTileLayer(Tiled::FindLayer(json.GetView(), "mid"));

        // Validate the tiles in the file.
        for (ivec2 pos : vector_range(cells.size()))
        {
            int tile_index = input_layer.safe_throwing_at(pos);
            if (tile_index < 0 || tile_index >= int(Tile::_count))
                throw std::runtime_error(FMT("Tile {} at {} is out of range.", tile_index, pos));
        }

        // Remove the existing grid contents.
        Resize(ivec2(0), ivec2(0));

        // Copy the tiles into the grid.
        ModifyRegion(ivec2(0), input_layer.size(), [&](auto &&cell)
        {
            for (ivec2 pos : vector_range(cells.size()))
            cell(pos).mid.tile = Tile(input_layer.safe_throwing_at(pos));
        });
    }
    catch (std::exception &e)
    {
        throw std::runtime_error(FMT("While loading map `{}`:\n{}", data.name(), e.what()));
    }
}

bool Grid::IsEmpty() const
{
    return (cells.size() <= 0).any();
}

void Grid::RemoveTile(ivec2 pos)
{
    if (!cells.pos_in_range(pos))
        return; // Out of range.

    if (Cells().safe_nonthrowing_at(pos).mid.tile == Tile::empty)
        return; // No tile.

    ModifyRegion(pos, ivec2(1), [](auto &&cell){cell(ivec2(0)).mid.tile = Tile::empty;});
}

Xf Grid::GridToWorld() const
{
    Xf ret = xf;
    ret.pos -= ret.Matrix() * (cells.size() * tile_size / 2);
    return ret;
}

bool Grid::CollidesWithPointInGridSpaceHighRes(ivec2 point) const
{
    ivec2 tile_pos = div_ex(point, TileHitboxes::highres_tile_size);
    if (!cells.pos_in_range(tile_pos))
        return false;
    return TileHitboxes::TileCollidesWithPointHighRes(cells.safe_throwing_at(tile_pos).mid.Info().corner, mod_ex(point, TileHitboxes::highres_tile_size));
}

bool Grid::CollidesWithPointInGridSpace(ivec2 point) const
{
    return TileHitboxes::ToHighResCorners(point, [&](ivec2 corner){return CollidesWithPointInGridSpaceHighRes(corner);});
}

bool Grid::CollidesWithGridWithCustomXfDifference(const Grid &other, Xf this_to_other, bool full) const
{
    struct Job
    {
        const Grid *source = nullptr;
        const Grid *target = nullptr;
        Xf xf;
    };

    this_to_other.pos *= TileHitboxes::highres_factor;

    Job jobs[2] = {
        {this, &other, this_to_other},
        {&other, this, this_to_other.Inverse()},
    };

    for (const Job &job : jobs)
    {
        for (const auto &[tile, mask] : full ? job.source->hitbox_points_full : job.source->hitbox_points_min)
        {
            for (int i = 0, cur_mask = mask; cur_mask; cur_mask >>= 1, i++)
            {
                if ((cur_mask & 1) == 0)
                    continue;

                for (ivec2 point : TileHitboxes::GetHitboxPointsHighRes(i))
                {
                    if (job.target->CollidesWithPointInGridSpaceHighRes(job.xf.TransformPixelCenteredPoint(point + tile * TileHitboxes::highres_tile_size)))
                        return true;
                }
            }
        }
    }
    return false;
}

void Grid::Render(Xf camera, std::optional<fvec3> color) const
{
    if (IsEmpty())
        return; // Empty grid.

    static const Graphics::TextureAtlas::Region &region = texture_atlas.Get("tiles.png");

    // Maps the grid space to the camera space.
    Xf render_xf = camera.Inverse() * GridToWorld();
    // Maps the camera space to the grid space.
    Xf inv_render_xf = render_xf.Inverse();

    auto [corner_a, corner_b] = sort_two(
        div_ex(inv_render_xf * (-screen_size/2), tile_size),
        div_ex(inv_render_xf * ( screen_size/2), tile_size)
    );
    clamp_var_min(corner_a, 0);
    clamp_var_max(corner_b, cells.size() - 1);

    for (ivec2 tile_pos : corner_a <= vector_range <= corner_b)
    {
        const CellLayer &la = cells.safe_throwing_at(tile_pos).mid;
        const TileInfo &info = la.Info();

        if (info.render == TileRenderFlavor::quarter)
        {
            ivec2 tile_pix_pos = render_xf * (tile_pos * tile_size + tile_size/2);

            // Returns true if this tile should be drawn merged with its neighbor at `offset`.
            auto MergeToOffset = [&](ivec2 offset) -> bool
            {
                ivec2 other_pos = tile_pos + offset;
                if (!cells.pos_in_range(other_pos))
                    return false;
                const TileInfo &other_info = cells.safe_nonthrowing_at(other_pos).mid.Info();
                if (offset.x == 0 || offset.y == 0)
                {
                    int dir = offset.angle4_floor();
                    if (!TileHitboxes::CornerHasEdge(info.corner, dir) || !TileHitboxes::CornerHasEdge(other_info.corner, dir + 2))
                        return false;
                }

                return other_info.merge_group == info.merge_group;
            };

            if (int corner = info.corner; corner >= 0)
            {
                ivec2 dir_a = ivec2::dir4(info.corner + 2);
                ivec2 dir_b = ivec2::dir4(info.corner + 3);

                bool merge_a = MergeToOffset(dir_a);
                bool merge_b = MergeToOffset(dir_b);
                bool merge_ab = merge_a && merge_b && MergeToOffset(dir_a + dir_b);

                int variant;
                bool flip = false;
                if (!merge_a && !merge_b)
                    variant = 3;
                else if (merge_a && !merge_b)
                    variant = 2, flip = true;
                else if (!merge_a && merge_b)
                    variant = 2;
                else if (merge_ab)
                    variant = 1;
                else
                    variant = 4;

                ivec2 matrix_dir = ivec2::dir4(mod_ex(corner + render_xf.rot - flip, 4));
                auto quad = r.iquad(tile_pix_pos, region.region(ivec2(variant, info.tex_index) * tile_size, ivec2(tile_size))).center().matrix(imat2(matrix_dir, matrix_dir.rot90())).flip_x(flip);
                if (color)
                    quad.color(*color).mix(0);
            }
            else
            {
                bool merge[4];
                bool merge_diag[4];
                for (int i = 0; i < 4; i++)
                    merge[i] = MergeToOffset(ivec2::dir4(i - xf.rot));
                for (int i = 0; i < 4; i++)
                    merge_diag[i] = merge[i] && merge[mod_ex(i + 1, 4)] && MergeToOffset(ivec2::dir8(1 + i * 2));

                for (int i = 0; i < 4; i++)
                {

                    ivec2 sector_dir = ivec2::dir4_diag(i);
                    ivec2 sector = clamp_min(sector_dir, 0);

                    ivec2 variant;

                    int next_i = mod_ex(i + 1, 4);

                    bool flip_diag = false;

                    if (merge_diag[i])
                        variant = ivec2(0, 1);
                    else if (!merge[i] && !merge[next_i])
                        variant = ivec2(0, 0);
                    else if (merge[i] != merge[next_i])
                        variant = ivec2(1, 0), flip_diag = (merge[i] == (i % 2));
                    else if (merge[i] && merge[next_i])
                        variant = ivec2(1, 1);
                    else
                        ASSERT(false, "Unsure what tile variant to use.");

                    ivec2 dir = ivec2::dir4(flip_diag);
                    auto quad = r.fquad(tile_pix_pos + tile_size/2 * (sector - 1) + tile_size/4.f, region.region(ivec2(0, info.tex_index) * tile_size + variant * tile_size/2, ivec2(tile_size/2))).center().matrix(imat2(dir, dir.rot90()));

                    if (flip_diag ? !sector.y : sector.x)
                        quad.flip_x();
                    if (flip_diag ? !sector.x : sector.y)
                        quad.flip_y();
                    if (color)
                        quad.color(*color).mix(0);
                }
            }


        }
    }
}

void Grid::DebugRender(Xf camera, DebugRenderFlags flags) const
{
    if (flags == DebugRenderFlags::none)
        return;

    Xf render_xf = camera.Inverse() * GridToWorld();

    if (bool(flags & DebugRenderFlags::aabb))
    {
        fvec3 color(0,0.8,0.8);
        float alpha = 1;

        ivec2 a = render_xf * ivec2(0);
        ivec2 b = render_xf * (cells.size() * tile_size);
        sort_two_var(a, b);


        // Top.
        r.iquad(a-1, ivec2(b.x - a.x + 2, 1)).color(color).alpha(alpha);
        // Bottom
        r.iquad(ivec2(a.x - 1, b.y), ivec2(b.x - a.x + 2, 1)).color(color).alpha(alpha);
        // Left.
        r.iquad(a with(x -= 1), ivec2(1, b.y - a.y)).color(color).alpha(alpha);
        // Right.
        r.iquad(ivec2(b.x, a.y), ivec2(1, b.y - a.y)).color(color).alpha(alpha);
    }

    if (bool(flags & DebugRenderFlags::coordinate_system))
    {
        int len = 32;
        float alpha = 1;

        Xf centered_xf = camera.Inverse() * xf;

        ivec2 center = centered_xf * ivec2(0);
        ivec2 a = centered_xf.Matrix() * ivec2(1,0);
        ivec2 b = centered_xf.Matrix() * ivec2(0,1);

        r.iquad(center, a * len + a.rot90().abs()).color(fvec3(1,0,0)).alpha(alpha);
        r.iquad(center, b * len + b.rot90().abs()).color(fvec3(0,1,0)).alpha(alpha);
        r.iquad(center, ivec2(1)).color(fvec3(1,1,0)).alpha(alpha);
    }

    if (bool(flags & DebugRenderFlags::tile_origin))
    {
        float alpha = 1;
        ivec2 pos = render_xf * ivec2(0,0);
        r.iquad(pos - 2, ivec2(4)).color(fvec3(0,0.8,0.8)).alpha(alpha);
        r.iquad(pos - 1, ivec2(2)).color(fvec3(0)).alpha(alpha);
    }

    if (bool(flags & DebugRenderFlags::hitbox_points_full))
    {
        fvec3 color(1,0,1);
        float alpha = 0.6;

        for (const auto &[tile, mask] : hitbox_points_full)
        {
            for (int i = 0, cur_mask = mask; cur_mask; cur_mask >>= 1, i++)
            {
                if ((cur_mask & 1) == 0)
                    continue;

                for (ivec2 point : TileHitboxes::GetHitboxPointsHighRes(i))
                    r.iquad(render_xf.TransformPixelCenteredPoint(TileHitboxes::ToNormalRes(point) + tile * tile_size), ivec2(1)).color(color).alpha(alpha);
            }
        }
    }

    if (bool(flags & DebugRenderFlags::hitbox_points_min))
    {
        fvec3 color(0,0.5,1);
        float alpha = 1;

        for (const auto &[tile, mask] : hitbox_points_min)
        {
            for (int i = 0, cur_mask = mask; cur_mask; cur_mask >>= 1, i++)
            {
                if ((cur_mask & 1) == 0)
                    continue;

                for (ivec2 point : TileHitboxes::GetHitboxPointsHighRes(i))
                    r.iquad(render_xf.TransformPixelCenteredPoint(TileHitboxes::ToNormalRes(point) + tile * tile_size), ivec2(1)).color(color).alpha(alpha);
            }
        }
    }
}
