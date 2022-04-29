#include "grid.h"

#include "game/main.h"

static const auto tile_info = []{
    TileInfo ret[] = {
        TileInfo{ .tile = Tile::air },
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

bool Grid::IsEmpty() const
{
    return (cells.size() <= 0).any();
}

Xf Grid::GridToWorld() const
{
    Xf ret = xf;
    ret.pos -= ret.Matrix() * (cells.size() * tile_size / 2);
    return ret;
}

void Grid::LoadFromFile(Stream::ReadOnlyData data)
{
    Json json(data.string(), 32);

    try
    {
        auto input_layer = Tiled::LoadTileLayer(Tiled::FindLayer(json.GetView(), "mid"));

        cells = Array2D<Cell>(input_layer.size());
        for (xvec2 pos : vector_range(cells.size()))
        {
            int tile_index = input_layer.safe_throwing_at(pos);
            if (tile_index < 0 || tile_index >= int(Tile::_count))
                throw std::runtime_error(FMT("Tile {} at {} is out of range.", tile_index, pos));
            cells.safe_throwing_at(pos).mid.tile = Tile(tile_index);
        }

        // Trim();
    }
    catch (std::exception &e)
    {
        throw std::runtime_error(FMT("While loading map `{}`:\n{}", data.name(), e.what()));
    }
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

    auto new_cells = decltype(cells)(ivec2(right - left, bottom - top) + 1);
    for (ivec2 pos : vector_range(new_cells.size()))
        new_cells.safe_throwing_at(pos) = std::move(cells.safe_throwing_at(pos + ivec2(left, top)));

    // Realign position to avoid visual movement.
    xf.pos += xf.Matrix() * (ivec2(left, top) * tile_size + new_cells.size() * tile_size / 2 - cells.size() * tile_size / 2);

    cells = std::move(new_cells);
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
