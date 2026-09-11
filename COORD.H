#ifndef COORD_H
#define COORD_H

#include<math.h>

//==============================================================================
//   This is a simple point class used for storing floating point coordinates.
//   It is intended to mimmic the complex class, except floats are used instead
//   of doubles to save storage space, since double precision is not needed
//   here, and the array of points can be quite large.  The sole purpose of
//   this class is to reduce point array storage requirements while still
//   providing floating point precision.  Everything has been inlined so that
//   the functions can be used as a library.
//==============================================================================

class coord
{
    float x, y;

  public:
    coord() { x = 0.0; y = 0.0; }
    coord(float xx, float yy) { x = xx; y = yy; }
    coord(const coord & pt) { x = pt.x; y = pt.y; }

    coord operator+(const coord & pt) const
    { coord temp;
      temp.x = x + pt.x;
      temp.y = y + pt.y;
      return temp;
    }
    coord operator-(const coord & pt) const
    { coord temp;
      temp.x = x - pt.x;
      temp.y = y - pt.y;
      return temp;
    }
    coord operator*(float val) const
    { coord temp;
      temp.x = x*val;
      temp.y = y*val;
      return temp;
    }
    void operator=(const coord & pt)
    { x = pt.x;  y = pt.y; }
    int operator==(const coord & pt) const
    { if (x==pt.x && y==pt.y) return 1;
      else return 0;
    }
    int operator!=(const coord & pt) const
    { if (x!=pt.x || y!=pt.y) return 1;
      else return 0;
    }
    // 2026 PORT: these were non-const, which made taskEnvironment::getCurrentLoc()
    // (returning const coord &) unusable without copying first.  Adding const is
    // source-compatible -- non-const objects may still call them.
    float getX() const { return x; }
    float getY() const { return y; }

    friend float angle(const coord & pt1, const coord & pt2);
    friend float distance(const coord & pt1, const coord & pt2);
};

/*
   Allows multiplication in the other direction.
*/

inline coord operator*(float val, const coord & pt)
{
  return pt * val;
}

/*
   This function gets the angle between two points.  A vector is constructed
   from pt1 to pt2 and the angle is the angle that this vector makes with
   the x axis.
*/

inline float angle(const coord & pt1, const coord & pt2)
{
  coord pt3 = pt2 - pt1;
  if (pt3.x == 0.0 && pt3.y == 0.0) return 0.0;
  else return float(atan2(pt3.y,pt3.x));
}

/*
   This function gets the distance between two points.
*/

inline float distance(const coord & pt1, const coord & pt2)
{
  coord pt3 = pt2 - pt1;
  return float(sqrt(pt3.x*pt3.x + pt3.y*pt3.y));
}

#endif
