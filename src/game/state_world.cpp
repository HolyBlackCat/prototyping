#include "main.h"

#include "game/grid.h"

namespace States
{
    STRUCT( World EXTENDS StateBase )
    {
        MEMBERS()

        Grid my_grid;
        Grid my_grid_2;

        Xf camera;

        World()
        {
            my_grid.LoadFromFile(Program::ExeDir() + "assets/test_ship.json");
            my_grid.xf.pos = ivec2(0);
            my_grid_2 = my_grid;

            SDL_MaximizeWindow(Interface::Window::Get().Handle());
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            if (Input::Button(Input::d).pressed())
                my_grid.xf = my_grid.xf.Rotate(1);

            if (Input::Button(Input::a).pressed())
                camera = camera.Rotate(1);

            if (mouse.left.released())
                my_grid.ModifyRegion(div_ex(my_grid.OtherToGrid(camera, mouse.pos()), tile_size), ivec2(1), [](auto &&cell){cell(ivec2(0)).mid.tile = Tile::wall;});

            if (mouse.right.released())
                my_grid.RemoveTile(div_ex(my_grid.OtherToGrid(camera, mouse.pos()), tile_size));

            // camera.pos = mouse.pos() * 2;
            // my_grid.xf.pos = mouse.pos() * 2;
            my_grid.xf.pos = mouse.pos();
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            my_grid_2.Render(camera);
            my_grid.Render(camera, my_grid.CollidesWithGrid(my_grid_2, false) ? fvec3(1,0,0) : my_grid.CollidesWithGrid(my_grid_2, true) ? fvec3(1,1,0) : fvec3(0,1,0));

            Grid::DebugRenderFlags debug_render_flags = Grid::DebugRenderFlags::hitbox_points;
            my_grid_2.DebugRender(camera, debug_render_flags);
            my_grid.DebugRender(camera, debug_render_flags);

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
