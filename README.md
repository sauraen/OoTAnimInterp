# OoTAnimInterp

Patch for OoT's SkelAnime system which allows skeletons to properly interpolate
between animations without sometimes "flipping out". Probably works on MM as
well.

### License

Copyright (C) 2021 Sauraen
You may redistribute, create derivative works of, and otherwise copy this
software, subject to the following conditions:
- If the software or modified versions of it are distributed in source form,
  it must retain this copyright notice and list of conditions
- Sauraen must be credited (e.g. in the Special Thanks) of any project using
  this software or modified versions of it

### How do I use this?

Replace SkelAnime_InterpFrameTable with Patched_InterpFrameTable. The
other functions are used by it and must be accessible to it somewhere. Of
course you can move the Quaternion struct to a header, remove `static inline`,
etc. There are more details in code comments.

### What has this been tested on?

A project on 1.0U (compiled by gcc, using z64hdr) with a custom SkelAnime. I set
up a button to enable and disable the slerp algorithm and then interpolated
between custom animations. With the vanilla algorithm, some cases of
interpolation cause the skeleton to have very badly wrong rotations during the
interpolation. With the new algorithm, these same interpolation conditions work
properly.

### Why is this needed?

OoT uses Euler angles to represent orientations. When you want to interpolate
between two orientations, the game simply linearly interpolates each of the
Euler angles independently. This is *a* path between the two orientations, but
this is only a sane path between the two orientations when the changes in the
Euler angles are small, or there is only one of the three which changes by a
large amount. However, when two or more of them change by a large amount, the
resulting path is generally "wrong". We expect the path to be the shortest
rotation path between the two orientations, but in these cases it is far from
that. If this happens to your skeleton's root bone, it will look very bad.

### What is the solution?

Convert the starting and ending rotations to quaternions, perform a *slerp*
(spherical linear interpolation) between them, and convert the resulting
intermediate quaternion back to Euler angles. This is a standard approach in
games, but it's a math-heavy algorithm with a lot of different conventions and
details to get wrong. (This took several hours to get working fully.)

### Is there a different solution besides this patch?

How bad the results are with the vanilla algorithm is a function of how the
skeleton is constructed. If the rotations are usually near 0, the problems are
much less severe. One could create a SkelAnime mesh where when all the limb
rotations are 0, the mesh is in a nice, rest position for the character. Then,
the animations would typically be values around 0, and then the interpolation
of Euler angles directly would not be too bad. There are two problems with this:
- fast64 would have to be modified to export skeletons and animations this way.
  Vanilla skeletons are typically set up (by whatever modeling software Nintendo
  used) to have each limb point along its bone, and the bone rotation to be 0
  when the bone is straight up. This is what produces the "folded skeletons".
  This would make fast64 less compatible with vanilla skeletons.
- If the animations happened to move the bones far from their rest position,
  the issues would still occur. This isn't a solution, just a way to make the
  problem much less likely to be encountered.

### Are there any disadvantages / downsides to this patch?

- The slerp algorithm is much more expensive than interpolating the Euler angles
  independently. Checks have been added to the code to only run the slerp
  algorithm when the naive algorithm is more likely to give bad results. And,
  OoT is typically not CPU bound, so it may not matter; you may way to try using
  the slerp algorithm globally.
- The slerp algorithm is designed to always take the shortest path between two
  orientations. For certain situations, the shortest path is physically the
  wrong answer. For example, raise your hand as high as you can, then rotate
  your arm backwards as far as it can go. Now rotate it forwards until it aims
  downwards, and keep going until it points down and backwards. If these were
  the two orientations to interpolate between, the slerp algorithm would find
  the shortest path, which is going around backwards, the wrong way. There's no
  way the game would know about this, and the naive algorithm also has this
  behavior when only one of the Euler angles is changing.

### How does OoT represent rotations (and other transformations)?
