# OoTAnimInterp

Patch for OoT's SkelAnime system which allows skeletons to properly interpolate
between animations without sometimes "flipping out". Probably works on MM as
well.

### License

Copyright (C) 2021 Sauraen \
You may redistribute, create derivative works of, and otherwise copy this
software, subject to the following conditions:
- If the software or modified versions of it are distributed in source form,
  it must retain this copyright notice and list of conditions
- Sauraen must be credited (e.g. in the Special Thanks) of any project using
  this software or modified versions of it

This repo does not contain any information derived from the Gigaleak.

### How do I use this?

Replace SkelAnime_InterpFrameTable with Patched_InterpFrameTable. The
other functions are used by it and must be accessible to it somewhere. Of
course you can move the Quaternion struct to a header, remove `static inline`,
etc. There are more details in code comments.

### What has this been tested on?

A project on 1.0U (compiled by gcc, using z64hdr) with a custom skeleton. I set
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
  (Folded skeletons are not required for OoT itself; it has no concept of bones,
  just limbs with rotations. A limb with a rotation of 0 can be shaped like
  whatever you want, it does not have to be sticking up vertically.) However,
  making this change would make fast64 no longer able to create custom
  animations for vanilla skeletons (unless the old version was kept as an
  option... which would of course keep the issue in these cases).
- If the animations happened to move the bones far from their rest position,
  the issues would still occur. This isn't a solution, just sweeping the problem
  under the rug.

### Are there any disadvantages / downsides to this patch?

- The slerp algorithm is much more expensive than interpolating the Euler angles
  independently. Checks have been added to the code to only run the slerp
  algorithm when the naive algorithm is more likely to give bad results. And,
  OoT is typically not CPU bound, so it may not matter; you may way to try using
  the slerp algorithm for all cases.
- The slerp algorithm is designed to always take the shortest path between two
  orientations. For certain situations, the shortest path is physically the
  wrong answer. For example, raise your hand as high as you can, then rotate
  your arm backwards as far as it can go. Now rotate it forwards until it aims
  downwards, and keep going until it points down and backwards. If these were
  the two orientations to interpolate between, the slerp algorithm would find
  the shortest path, which is going around backwards, the wrong way. There's no
  way the game would know about the limits of rotation of your arm. (Besides,
  the naive algorithm also has this behavior, for example when only one of the
  Euler angles is changing.)

### How does OoT represent rotations (and other transformations)?

OoT uses a matrix stack in `sys_matrix.c`. In the SkelAnime context, the matrix
state currently being manipulated on the stack is the `model` matrix *M*, which
is the transformation from the coordinate space of the limb into world space
*w*. That is, for a vertex *v* in the limb's DL, *w = Mv*. Actually the RSP
multiplies the model, view, and projection matrices like *PVM* and applies the
resulting single matrix to the vertex, *w = (PVM)v*. This is the same as *w = P
* (V * (M * v))*; matrices and transformations are applied to the vertex right
to left, simply because the rightmost matrix is the one immediately next to *v*.
(Matrix multiplication is not commutative, changing the order gives a different
result.)

When a transform is applied to the matrix on the stack, it's applied on the
right. So if the stack currently holds *M* and you call `Matrix_Mult` with 
another matrix *Q*, or `Matrix_Translate` with a translation corresponding to
matrix *T*, or any of the other similar functions, the stack will then hold
*MQ* or *MT* etc. The various SkelAnime draw functions call 
`Matrix_JointPosition`; looking at its source code we can see that it applies
the limb position, then the Z rotation, then the Y rotation, and then the X
rotation. So this would be *MPZYX*. So you can see that when the limb is
drawn, first the X rotation matrix is applied to it, then the Y, and then Z.
These matrices being applied in `Matrix_JointPosition` are global rotations,
so to be more precise, when the limb is drawn, it is first rotated in global X,
than in global Y, then in global Z, then translated by a global delta of P, and
then this process repeats for the parent limbs whose transforms are already on
the stack. The last transforms are done before your actor's draw function is
called; in order of being applied to the vertex (opposite order that they call
the `Matrix_` functions), they are the actor scale, rotation, and world
position.

So in summary, OoT uses the Euler angles convention global-X, global-Y,
global-Z. (This is also equivalent to doing local transforms in the opposite
order: local-Z, local-Y, local-X.) Fortunately, this is a common standard, and
was used on the Wikipedia page (but this is not the same standard as used in the
euclideanspace.com pages).

### Algorithms modified from:

- https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
- http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/
- http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/Quaternions.pdf
