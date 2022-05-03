#include "main.h"

#include "game/grid.h"

namespace States
{
    STRUCT( World EXTENDS StateBase )
    {
        MEMBERS()

        Grid my_grid;

        Xf camera;

        World()
        {
            my_grid.LoadFromFile(Program::ExeDir() + "assets/test_ship.json");

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
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            my_grid.Render(camera);
            my_grid.DebugRender(camera, Grid::DebugRenderFlags::all);

            { // Cursor.
                ivec2 point = camera.TransformPixelCenteredPoint(mouse.pos());
                bool a = my_grid.CollidesWithPointInWorldSpace(point);
                bool b = my_grid.CollidesWithPointInWorldSpace(point, true);
                ASSERT(b <= a);

                fvec3 color = b ? fvec3(1,0,0) : a ? fvec3(1,1,0) : fvec3(0,1,0);
                r.iquad(mouse.pos() with(y -= 16), ivec2(1,33)).color(color);
                r.iquad(mouse.pos() with(x -= 16), ivec2(33,1)).color(color);
            }

            r.Finish();
        }
    };
}
