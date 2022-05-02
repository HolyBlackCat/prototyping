#include "grid.h"

#include "game/main.h"

static const auto tile_info = []{
    TileInfo ret[] = {
        TileInfo{ .tile = Tile::empty },
        TileInfo{ .tile = Tile::wall,   .tex_index = 0, .solid = true },
        TileInfo{ .tile = Tile::wall_a, .tex_index = 0, .solid = true, .corner = 0 },
        TileInfo{ .tile = Tile::wall_b, .tex_index = 0, .solid = true, .corner = 1 },
        TileInfo{ .tile = Tile::wall_c, .tex_index = 0, .solid = true, .corner = 2 },
        TileInfo{ .tile = Tile::wall_d, .tex_index = 0, .solid = true, .corner = 3 },
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

void Grid::Resize(ivec2 offset, ivec2 new_size)
{
    if (new_size(any) == 0)
        new_size = {};

    // Realign position to avoid visual movement.
    xf.pos += xf.Matrix() * (-offset * tile_size + new_size * tile_size / 2 - cells.size() * tile_size / 2);

    cells.resize(new_size, offset);
}

void Grid::Trim()
{
    if (IsEmpty())
        return; // No cells.

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
        return;
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
        return; // No changes needed.

    Resize(-ivec2(left, top), ivec2(right - left, bottom - top) + 1);
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

void Grid::Render(Xf camera) const
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

        if (info.tex_index == -1)
            continue; // Invisible.

        ivec2 tile_pix_pos = render_xf * (tile_pos * tile_size + tile_size/2) - tile_size/2;

        // Adjust the corner for rotation.
        int corner = info.corner;
        if (corner != -1)
            corner = mod_ex(corner + render_xf.rot, 4);

        r.iquad(tile_pix_pos, region.region(ivec2(corner + 1, info.tex_index) * tile_size, ivec2(tile_size)));
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
}
