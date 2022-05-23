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

            obj.grid.xf.pos = ivec2(0);
            // obj.grid.xf.rot = 1;
            obj.vel = fvec2(1,0.17);
            // obj.vel = fvec2(1,0.23);
            my_grid_id = grids.AddGrid(obj);

            obj.grid.xf.pos = ivec2(150,50);
            obj.grid.xf.rot = 0;
            obj.vel = fvec2();
            grids.AddGrid(obj);

            GridObject cube;
            cube.grid.ModifyRegion(ivec2(), ivec2(2), [](auto &&cell)
            {
                for (ivec2 pos : vector_range(ivec2(2)))
                    cell(pos).mid.tile = Tile::wall;
            });

            cube.grid.xf.pos = ivec2(0,-100);
            cube.vel = fvec2(-0.24,0);
            grids.AddGrid(cube);

            GridObject bracket;
            bracket.grid.ModifyRegion(ivec2(), ivec2(6,3), [](auto &&cell)
            {
                cell(ivec2(1,0)).mid.tile = cell(ivec2(2,0)).mid.tile = cell(ivec2(3,0)).mid.tile = cell(ivec2(4,0)).mid.tile = Tile::wall;
                cell(ivec2(0,1)).mid.tile = cell(ivec2(5,1)).mid.tile = cell(ivec2(1,2)).mid.tile = cell(ivec2(4,2)).mid.tile = Tile::wall;
                cell(ivec2(0,0)).mid.tile = Tile::wall_c;
                cell(ivec2(5,0)).mid.tile = Tile::wall_d;
                cell(ivec2(0,2)).mid.tile = Tile::wall_b;
                cell(ivec2(5,2)).mid.tile = Tile::wall_a;
            });
            bracket.grid.xf.pos = cube.grid.xf.pos with(y -= 12);
            bracket.vel = fvec2(-0.24,0);
            grids.AddGrid(bracket);

            GridObject wedge;
            wedge.grid.ModifyRegion(ivec2(), ivec2(4,2), [](auto &&cell)
            {
                cell(ivec2(0,0)).mid.tile = Tile::wall_c;
                cell(ivec2(3,0)).mid.tile = Tile::wall_d;
                cell(ivec2(0,1)).mid.tile = Tile::wall_b;
                cell(ivec2(3,1)).mid.tile = Tile::wall_a;
                for (ivec2 pos : vector_range(ivec2(2)) + ivec2(1,0))
                    cell(pos).mid.tile = Tile::wall;
            });
            wedge.grid.xf.pos = ivec2(-150,0);
            wedge.grid.xf.pos.x -= 64;
            // wedge.vel = fvec2(0.24,0);
            wedge.vel = fvec2(0.5,0);
            grids.AddGrid(wedge);

            // float wedge2speed = 0.24;
            float wedge2speed = 0;

            GridObject wedge2long;
            wedge2long.grid.ModifyRegion(ivec2(), ivec2(4,1), [](auto &&cell)
            {
                cell(ivec2(0,0)).mid.tile = Tile::wall_b;
                cell(ivec2(1,0)).mid.tile = Tile::wall;
                cell(ivec2(2,0)).mid.tile = Tile::wall;
                cell(ivec2(3,0)).mid.tile = Tile::wall_a;
            });
            GridObject wedge2;
            wedge2.grid.ModifyRegion(ivec2(), ivec2(3,1), [](auto &&cell)
            {
                cell(ivec2(0,0)).mid.tile = Tile::wall_b;
                cell(ivec2(1,0)).mid.tile = Tile::wall;
                cell(ivec2(2,0)).mid.tile = Tile::wall_a;
            });
            wedge2long.grid.xf.pos = ivec2(-150+18,24);
            wedge2long.grid.xf.rot = 1;
            wedge2long.vel = fvec2(0,wedge2speed);
            grids.AddGrid(wedge2long);
            wedge2long.grid.xf.pos = ivec2(-150+18,-24);
            wedge2long.grid.xf.rot = 1;
            wedge2long.vel = fvec2(0,-wedge2speed);
            grids.AddGrid(wedge2long);
            wedge2.grid.xf.pos = ivec2(-150+6,-42);
            wedge2.grid.xf.rot = 0;
            wedge2.vel = fvec2(-wedge2speed,0);
            grids.AddGrid(wedge2);
            wedge2.grid.xf.pos = ivec2(-150+6,42);
            wedge2.grid.xf.rot = 2;
            wedge2.vel = fvec2(-wedge2speed,0);
            grids.AddGrid(wedge2);
            wedge2.grid.xf.pos = ivec2(-150-6,-30);
            wedge2.grid.xf.rot = 3;
            wedge2.vel = fvec2(0,wedge2speed);
            grids.AddGrid(wedge2);
            wedge2.grid.xf.pos = ivec2(-150-6,30);
            wedge2.grid.xf.rot = 3;
            wedge2.vel = fvec2(0,-wedge2speed);
            grids.AddGrid(wedge2);

            GridObject bar;
            bar.grid.ModifyRegion(ivec2(), ivec2(5, 1), [](auto &&cell)
            {
                for (ivec2 pos : vector_range(ivec2(5, 1)))
                    cell(pos).mid.tile = Tile::wall;
            });
            bar.grid.xf.pos = ivec2(-90, -24);
            grids.AddGrid(bar);
            bar.grid.xf.pos = ivec2(-90, 24);
            grids.AddGrid(bar);

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

            grids.TickPhysics();

            // grids.ModifyGrid(my_grid_id, [&](GridObject &obj)
            // {
            //     obj.grid.xf.pos = mouse.pos();
            // });
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            grids.Render(camera);
            // grids.DebugRender(camera, Grid::DebugRenderFlags::all);

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
