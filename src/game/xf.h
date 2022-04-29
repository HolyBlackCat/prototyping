#pragma once

struct Xf
{
    ivec2 pos;
    int rot = 0;

    [[nodiscard]] Xf Rotate(int steps) const
    {
        Xf ret;
        ret.pos = pos;
        ret.rot = mod_ex(rot + steps, 4);
        return ret;
    }

    [[nodiscard]] imat2 Matrix() const
    {
        ivec2 dir_vec = ivec2::dir4(rot);
        return imat2(dir_vec, dir_vec.rot90());
    }

    [[nodiscard]] ivec2 operator*(ivec2 target) const
    {
        return pos + Matrix() * target;
    }

    [[nodiscard]] Xf operator*(const Xf &other) const
    {
        Xf ret;
        ret.pos = pos + Matrix() * other.pos;
        ret.rot = mod_ex(rot + other.rot, 4);
        return ret;
    }

    [[nodiscard]] Xf Inverse() const
    {
        Xf ret;
        ret.rot = mod_ex(-rot, 4);
        ret.pos = ret.Matrix() * -pos;
        return ret;
    }
};
