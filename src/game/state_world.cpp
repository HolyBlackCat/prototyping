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
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            if (Input::Button(Input::space).pressed())
                my_grid.Trim();

            if (mouse.left.pressed())
                my_grid.xf = my_grid.xf.Rotate(1);

            if (mouse.right.pressed())
                camera = camera.Rotate(1);

            camera.pos = mouse.pos() * 2;
            // my_grid.xf.pos = mouse.pos() * 2;
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            my_grid.Render(camera);

            r.Finish();
        }
    };
}
