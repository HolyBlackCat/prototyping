#include "main.h"

#include "game/grid_manager.h"
#include "game/grid.h"

namespace States
{
    STRUCT( World EXTENDS StateBase )
    {
        MEMBERS()

        GridManager grids;
        GridId my_grid_id;

        Xf camera;

        World()
        {
            GridObject obj;
            obj.grid.LoadFromFile(Program::ExeDir() + "assets/test_ship.json");
            my_grid_id = grids.AddGrid(obj);
            obj.grid.xf.pos -= ivec2(100);
            grids.AddGrid(obj);

            SDL_MaximizeWindow(Interface::Window::Get().Handle());
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            if (Input::Button(Input::d).pressed())
                grids.ModifyGrid(my_grid_id, [](GridObject &obj){obj.grid.xf = obj.grid.xf.Rotate(1);});

            if (Input::Button(Input::a).pressed())
                camera = camera.Rotate(1);

            if (mouse.left.released())
            {
                grids.ModifyGrid(my_grid_id, [&](GridObject &obj)
                {
                    obj.grid.ModifyRegion(div_ex(obj.grid.OtherToGrid(camera, mouse.pos()), tile_size), ivec2(1), [](auto &&cell){cell(ivec2(0)).mid.tile = Tile::wall;});
                });
            }


            if (mouse.right.released())
            {
                grids.ModifyGrid(my_grid_id, [&](GridObject &obj)
                {
                    obj.grid.RemoveTile(div_ex(obj.grid.OtherToGrid(camera, mouse.pos()), tile_size));
                });
            }

            // camera.pos = mouse.pos() * 2;
            // my_grid.xf.pos = mouse.pos() * 2;
            grids.ModifyGrid(my_grid_id, [&](GridObject &obj)
            {
                obj.grid.xf.pos = mouse.pos();
            });
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            Grid::DebugRenderFlags debug_render_flags = Grid::DebugRenderFlags::all;
            grids.Render(camera);
            grids.DebugRender(camera, debug_render_flags);

            fvec3 color = grids.CollideGrid(my_grid_id, {}, true, [](GridId){return true;}) ? fvec3(1,0,0) : fvec3(0,1,0);
            r.iquad(mouse.pos(), ivec2(8)).center().color(color);

            // { // Cursor.
            //     ivec2 point = camera.TransformPixelCenteredPoint(mouse.pos());
            //     fvec3 color = my_grid.CollidesWithPointInWorldSpace(point) ? fvec3(1,0,0) : fvec3(0,1,0);
            //     r.iquad(mouse.pos() with(y -= 16), ivec2(1,33)).color(color);
            //     r.iquad(mouse.pos() with(x -= 16), ivec2(33,1)).color(color);
            // }

            r.Finish();
        }
    };
}
