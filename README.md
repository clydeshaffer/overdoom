````
 ▄██████▄   ▄█    █▄     ▄████████    ▄████████ ████████▄   ▄██████▄   ▄██████▄    ▄▄▄▄███▄▄▄▄   
███    ███ ███    ███   ███    ███   ███    ███ ███   ▀███ ███    ███ ███    ███ ▄██▀▀▀███▀▀▀██▄ 
███    ███ ███    ███   ███    █▀    ███    ███ ███    ███ ███    ███ ███    ███ ███   ███   ███ 
███    ███ ███    ███  ▄███▄▄▄      ▄███▄▄▄▄██▀ ███    ███ ███    ███ ███    ███ ███   ███   ███ 
███    ███ ███    ███ ▀▀███▀▀▀     ▀▀███▀▀▀▀▀   ███    ███ ███    ███ ███    ███ ███   ███   ███ 
███    ███ ███    ███   ███    █▄  ▀███████████ ███    ███ ███    ███ ███    ███ ███   ███   ███ 
███    ███ ███    ███   ███    ███   ███    ███ ███   ▄███ ███    ███ ███    ███ ███   ███   ███ 
 ▀██████▀   ▀██████▀    ██████████   ███    ███ ████████▀   ▀██████▀   ▀██████▀   ▀█   ███   █▀  
                                     ███    ███                                                  
````

DOS FPS game engine. Named Overdoom because I have half a mind to do a retro "demake" tribute to my favorite modern FPS.

Currently my objective is to get this running nicely on real actual vintage hardware. (Specifically this 486 PC I got on eBay, clocked at 66MHz) At time of writing it kind of slogs a little on the old computer, due in part to an overdraw bug.

Level is loaded loaded from a .JSD file (Juego Sector Description), which lists the world vertices, then rooms as vertex index lists and ceiling/floor heights. vertex index list entries include optional field for sector index, to which that vertex and the next form a portal

Three JSD files are in the repo, but I don't remember if TEST1 and TEST2 use the latest format. They were typed by hand after drawing the level on graph paper. Currently the level filename is hardcoded.

Player viewpoint starts in sector 0. Sectors are rendered wall-at-a-time onto a subset of the screen pixels. If the wall is a portal then instead of a wall, the corresponding adjacent sector is drawn within the screen pixels that the wall would have occupied.

Computations are done using fixed point arithmetic. Angles are represented as a single byte.

At the start of the program, 128 palette colors are dedicated to 16 shades of 8 colors to use for gradients. The gradients are used to represent distance from the camera, with the walls getting darker as they get further away. Ordered Dithering is employed to sell the blend effect a bit better.

Controls are WASD to strafe around, with the left and right arrow keys used to rotate the view.

Run make and then JUEGO2.EXE to test it out
