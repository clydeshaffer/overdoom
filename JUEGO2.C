#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <math.h>

#include "Vector2.h"
#include "keyb.h"
#include "vga.h"

#define GRAD_NONE 0
#define GRAD_H 1
#define GRAD_V 2

#define WALL 1
#define CEILING 2
#define FLOOR 4

#define PI 3.14159

#define minOf(a,b) (a<b)?a:b
#define maxOf(a,b) (a>b)?a:b
#define sgn(x) (x>0)?1:-(x<0)
#define times320(y) ((y<<8)+(y<<6))
#define clamp(x,a,b) (a>x)?a:((x<b)?x:b)

#define INSIDE 0
#define LEFT 1
#define RIGHT 2
#define BOTTOM 3
#define TOP 4

#define QUIT_KEY 1
#define FORWARD_KEY 2
#define BACKWARD_KEY 4
#define STRAFELEFT_KEY 8
#define STRAFERIGHT_KEY 16
#define TURNLEFT_KEY 32
#define TURNRIGHT_KEY 64
#define JUMP_KEY 128

/*#define DEBUGDELAY*/

#define true 1
#define false 0

typedef unsigned short word;
typedef int bool;
typedef int Outcode;

word *my_clock=(word *)0x0000046C;

byte ceiling_color = 52;
byte ground_color = 114;
byte wall_color = 1;
byte wall_color_left = 0;
byte wall_color_right = 0;

byte trape_heights[SCREEN_WIDTH];
byte *gradient_buffer;

int world_vertex_count;
Vector2* world_vertex_list;
Vector2 zeroV;
Vector2 screen_top_right;
Vector2 screen_bottom_left;
Vector2 screen_bottom_right;

int top_clip = 0;
int bottom_clip = 200;

float viewAngleTan = 1.732;

typedef struct Viewport
{
	float top, bottom, left, right;
} Viewport;

typedef struct Sector
{
	float floor, ceiling;
	int corner_count;
	int* corners;
	int* neighbors;
	int color_offset;
	byte ground_color;
	byte ceiling_color;
} Sector;


bool request_debug_delay = false;

#ifdef DEBUGDELAY
void debug_delay() {
	static byte counter = 0;
	if(!request_debug_delay) return;
	if(counter == 0) {
		wait_retrace();
		show_buffer();
	}
	counter+=32;
}
#endif

int world_sector_count;
Sector* world_sector_list;
int current_sector = 0;

float line_side_origin(Vector2 a, Vector2 b) {
	return a.x * a.y + b.x * b.y - 2 * b.x * a.y;
}

float line_side(Vector2 p, Vector2 a, Vector2 b) {
	return vec_scross(vec_sub(b, a), vec_sub(p,a));
}

bool point_inside_triangle(Vector2 point, Vector2 a, Vector2 b, Vector2 c) {
	int i;
	int sign = line_side(point, a, b) < 0;
	if (sign != (line_side(point, b, c) < 0)) return false;
	if (sign != (line_side(point, c, a) < 0)) return false;
	return true;
}

/*assumption: s has at least 3 corners*/
bool point_inside_sector(Vector2 point, Sector s) {
	int i;
	float side = line_side(point, world_vertex_list[s.corners[0]], world_vertex_list[s.corners[1]]);
	int sign = side < 0;
	for(i = 1; i < s.corner_count; i++) {
		side = line_side(point, world_vertex_list[s.corners[i]], world_vertex_list[s.corners[(i+1) % s.corner_count]]);
		if (sign != (side < 0)) return false;
	}
	return true;
}

float viewerAngle = 0;
float viewerAngleSin = 0;
float viewerAngleCos = 1;
float viewer_z_pos = 1;
float viewer_height = 10;
float viewer_knee = 5;

const float darkness_depth = 400;

Vector2 viewerPosition;
Sector* viewerCurrentSector; 

Vector2 vec_trans_world(Vector2 v) {
	return vec_rotate_faster(vec_sub(v, viewerPosition), viewerAngleSin, viewerAngleCos);
}

Vector2 coords_to_screen(Vector2 v) {
	v.x += 160;
	v.y += 100;
	return v;
}

char debug_text[64];


/*int failsBounds(int x, int y) {
	return (x < 0) || (x >= SCREEN_WIDTH) || (y < 0) || (y >= SCREEN_HEIGHT);
}*/

byte depth_to_shade(float depth) {
	depth = 16 * min(depth, darkness_depth) / darkness_depth;
	if(depth > 15) depth = 15;
	else if(depth < 1) depth = 1;
	return (byte)((int) depth);
}

void vert(int xpos, int ystart, int ylen, byte color, int gradient_mode) {
	int i;
	int k, g;
	byte drawn_color[3];
	drawn_color[GRAD_NONE] = color;
	drawn_color[GRAD_H] = gradient_buffer[xpos];
	if(ylen <= 0) return;
	if(ystart + ylen < 0) return;
	ystart = maxOf(ystart, 0);
	g = ystart;
	k = (ystart << 8) + (ystart << 6);
	if(ystart + ylen > SCREEN_HEIGHT) {
		ylen = SCREEN_HEIGHT - ystart;
	}
	#ifdef DEBUGDELAY
	debug_delay();
	#endif
	for(i = 0; i < ylen; i++) {
		drawn_color[GRAD_V] = gradient_buffer[g];
		g++;
		graphic_buffer[k + xpos] = drawn_color[gradient_mode];
		k += SCREEN_WIDTH;
		
	}
	return;
}

void rect(int xpos, int ypos, int width, int height, byte color) {
	int i, addr = times320(ypos) + xpos;
	for(i = 0; i < height; i++) {
		memset(&(graphic_buffer[addr]), color, width);
		addr += SCREEN_WIDTH;
		#ifdef DEBUGDELAY
		debug_delay();
		#endif
	}
}

void grad_rect_h(int xpos, int ypos, int width, int height) {
	int i, addr = times320(ypos) + xpos;
	for(i = 0; i < height; i++) {
		/*memset(&(graphic_buffer[addr]), color, width);*/
		memcpy(&(graphic_buffer[addr]), gradient_buffer+xpos, width);
		addr += SCREEN_WIDTH;
		#ifdef DEBUGDELAY
		debug_delay();
		#endif
	}
}

void grad_rect_v(int xpos, int ypos, int width, int height) {
	int i, addr = times320(ypos) + xpos;
	for(i = 0; i < height; i++) {
		memset(&(graphic_buffer[addr]), gradient_buffer[ypos+i], width);
		addr += SCREEN_WIDTH;
	}
}

void draw_line(int ax, int ay, int bx, int by, byte color) {
	int dx = bx - ax,
		dy = by - ay,
		absx = abs(dx),
		absy = abs(dy),
		sx = sgn(dx),
		sy = sgn(dy),
		stx = absx >> 1,
		sty = absy >> 1,
		i;

	if(abs(dx) > abs(dy)) {
		for(i = 0; i <= abs(dx); i ++) {
			sty += absy;
			if(sty >= absx) {
				sty -= absx;
				ay += sy;
			}
			ax += sx;
			graphic_buffer[(ay << 8) + (ay << 6) + ax] = color;
		}
	} else {
		for(i = 0; i <= abs(dy); i ++) {
			stx += absx;
			if(stx >= absy) {
				stx -= absy;
				ax += sx;
			}
			ay += sy;
			graphic_buffer[(ay << 8) + (ay << 6) + ax] = color;
		}
	}
}

void draw_line2(int ax, int ay, int bx, int by, byte color) {
	int dx = bx - ax,
		dy = by - ay,
		absx = abs(dx),
		absy = abs(dy),
		sx = sgn(dx),
		sy = sgn(dy),
		stx = absx >> 1,
		sty = absy >> 1,
		i, px = ax, py = ay;

	int *dmajor, *smajor, *sminor, *stminor, *absmajor, *absminor, *pmajor, *pminor;

	if(abs(dx) > abs(dy)) {
		dmajor = &dx;
		smajor = &sx;
		sminor = &sy;
		stminor = &sty;
		absmajor = &absx;
		absminor = &absy;	
		pmajor = &px;
		pminor = &py;	
	} else {
		dmajor = &dy;
		smajor = &sy;
		sminor = &sx;
		stminor = &stx;
		absmajor = &absy;
		absminor = &absx;	
		pmajor = &py;
		pminor = &px;
	}

	for(i = 0; i <= abs(*dmajor); i ++) {
		*stminor += *absminor;
		if(*stminor >= *absmajor) {
			*stminor -= *absmajor;
			*pminor += *sminor;
		}
		*pmajor += *smajor;
		pixel(px, py) = color;
	}
}

void interpolate_buffer(int ax, int ay, int bx, int by, byte *target_array) {
	int dx = bx - ax,
		dy = by - ay,
		absx = abs(dx),
		absy = abs(dy),
		sx = sgn(dx),
		sy = sgn(dy),
		stx = absx >> 1,
		sty = absy >> 1,
		i, px = ax, py = ay;

	int *dmajor, *smajor, *sminor, *stminor, *absmajor, *absminor, *pmajor, *pminor;

	if(ay == by) {
		memset(target_array+min(ax, bx), by, absx);
	}

	if(abs(dx) > abs(dy)) {
		dmajor = &dx;
		smajor = &sx;
		sminor = &sy;
		stminor = &sty;
		absmajor = &absx;
		absminor = &absy;	
		pmajor = &px;
		pminor = &py;	
	} else {
		dmajor = &dy;
		smajor = &sy;
		sminor = &sx;
		stminor = &stx;
		absmajor = &absy;
		absminor = &absx;	
		pmajor = &py;
		pminor = &px;
	}

	target_array[ax] = ay;

	if(ax < bx) {
		for(i = 0; i <= ax; i++) {
			target_array[i] = ay;
		}

		for(i = bx; i < SCREEN_WIDTH; i++) {
			target_array[i] = by;
		}
	} else {
		for(i = 0; i <= bx; i++) {
			target_array[i] = by;
		}

		for(i = ax; i < SCREEN_WIDTH; i++) {
			target_array[i] = ay;
		}
	}

	for(i = 0; i <= abs(*dmajor); i ++) {
		*stminor += *absminor;
		if(*stminor >= *absmajor) {
			*stminor -= *absmajor;
			*pminor += *sminor;
		}
		*pmajor += *smajor;
		if(px > 0 && px < SCREEN_WIDTH) {
			target_array[px] = clamp(py, 0, SCREEN_WIDTH);
		}
	}
}

void draw_wall_screen(int ax, int ay, int bx, int by, int gradient_type, byte color) {
	int dx = bx - ax,
		dy = by - ay,
		absx = abs(dx),
		absy = abs(dy),
		sx = sgn(dx),
		sy = sgn(dy),
		stx = absx >> 1,
		sty = absy >> 1,
		i, height, px = ax, py = ay,
		lower_top = max(ay,by),
		higher_bottom = min(trape_heights[ax+1], trape_heights[bx-1]);
	byte grad_color_h, *drawn_color;

	/*if(failsBounds(ax,ay)) return;
	if(failsBounds(bx,by)) return;*/
	
	if(gradient_type == GRAD_H) {
		drawn_color = &grad_color_h;
	} else {
		drawn_color = &color;
	}

	if(lower_top < higher_bottom) {
		vert(ax, ay, lower_top - ay, color, gradient_type);
		vert(ax, higher_bottom, trape_heights[ax] - higher_bottom, color, gradient_type);

		vert(bx, by, lower_top - by, color, gradient_type);
		vert(bx, higher_bottom, trape_heights[bx] - higher_bottom, color, gradient_type);
		if(absx > absy) {
			for(i = 0; i <= absx; i ++) {
				sty += absy;
				if(sty >= absx) {
					sty -= absx;
					py += sy;
				}
				px += sx;
				grad_color_h = gradient_buffer[px];
				/*graphic_buffer[(py << 8) + (py << 6) + px] = color;*/
				/*vert(px, py, trape_heights[px] - py, color);*/
				vert(px, py, lower_top - py, *drawn_color, gradient_type);
				vert(px, higher_bottom, trape_heights[px] - higher_bottom, *drawn_color, gradient_type);

			}
		} else {
			for(i = 0; i <= absy; i ++) {
				stx += absx;
				if(stx >= absy) {
					stx -= absy;
					px += sx;
				}
				py += sy;
				grad_color_h = gradient_buffer[px];
				/*graphic_buffer[(py << 8) + (py << 6) + px] = color;*/
				vert(px, py, lower_top - py, *drawn_color, gradient_type);
				vert(px, higher_bottom, trape_heights[px] - higher_bottom, *drawn_color, gradient_type);
			}
		}
		if(gradient_type == GRAD_H) {
			grad_rect_h(ax, lower_top, absx+2, higher_bottom - lower_top);
		} else if(gradient_type == GRAD_V) {
			grad_rect_v(ax, lower_top, absx+2, higher_bottom - lower_top);
		} else {
			rect(ax, lower_top, absx+2, higher_bottom - lower_top, color);
		}
	} else {
		vert(px, py, trape_heights[px] - py, color, gradient_type);
		vert(bx, by, trape_heights[bx] - by, color, gradient_type);
		if(absx > absy) {
			for(i = 0; i <= absx; i ++) {
				sty += absy;
				if(sty >= absx) {
					sty -= absx;
					py += sy;
				}
				px += sx;
				vert(px, py, trape_heights[px] - py, color, gradient_type);

			}
		} else {
			for(i = 0; i <= absy; i ++) {
				stx += absx;
				if(stx >= absy) {
					stx -= absy;
					px += sx;
				}
				py += sy;
				vert(px, py, trape_heights[px] - py, color, gradient_type);
			}
		}
	}
}

bool intersect(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Vector2 *output)
{
	float dotProd = vec_dot(vec_sub(b, a), vec_sub(d, c));
    Vector2 a2b = vec_sub(b, a);
    Vector2 c2d = vec_sub(d, c);
    Vector2 a2c = vec_sub(c, a);
    Vector2 c2a = vec_sub(a, c);
    float aXb, cXd, s, t;
    if(dotProd == 0) return false;
    aXb = vec_scross(a2b, c2d);
    cXd = vec_scross(c2d, a2b);
    s = vec_scross(a2c,c2d) / aXb;
    t = vec_scross(c2a,a2b) / cXd;
    if(s < 0 || s > 1) return false;
    if(t < 0 || t > 1) return false;
    *output = vec_add(a, vec_scale_s(a2b, s));
    return true;
}

Outcode get_outcode(Vector2 v, int top, int bottom) {
	Outcode code = 0;
	if(v.y < top) {
		code |= TOP;
	} else if(v.y > (bottom)) { /* was SCREEN_HEIGHT-1*/
		code |= BOTTOM;
	}
	if(v.x < 0) {
		code |= LEFT;
	} else if(v.x > (SCREEN_WIDTH-1)) { /*was SCREEN_WIDTH-1*/
		code |= RIGHT;
	}
	return code;
}

/* coen-sutherland algorithm */
bool clip_line_to_viewport(Vector2 *pointA, Vector2 *pointB, int top, int bottom) {
	Outcode outcodeA = get_outcode(*pointA, top, bottom),
	outcodeB = get_outcode(*pointB, top, bottom),
	outside_code;
	Vector2 intersect_point,
	originalA = *pointA,
	originalB = *pointB;

	while(true) {
		if(!(outcodeA | outcodeB)) {
			/*line has been corrected*/
			return true;
		} else if(outcodeA & outcodeB) {
			/*return unmodified failing line*/
			*pointA = originalA;
			*pointB = originalB;
			return false;
		} else {
			outside_code = outcodeA ? outcodeA : outcodeB;

			if(outside_code & TOP) {
				intersect_point.x = pointA->x + (pointB->x - pointA->x) * (top - pointA->y) / (pointB->y - pointA->y);
				intersect_point.y = top;
			} else if(outside_code & BOTTOM) {
				intersect_point.x = pointA->x + (pointB->x - pointA->x) * ((bottom) - pointA->y) / (pointB->y - pointA->y);
				intersect_point.y = bottom;
			} else if(outside_code & LEFT) {
				intersect_point.x = 0;
				intersect_point.y = pointA->y + (pointB->y - pointA->y) * (0 - pointA->x) / (pointB->x - pointA->x);
			} else if(outside_code & RIGHT) {
				intersect_point.x = SCREEN_WIDTH-1;
				intersect_point.y = pointA->y + (pointB->y - pointA->y) * ((SCREEN_WIDTH-1) - pointA->x) / (pointB->x - pointA->x);
			}
		}

		if(outcodeA == outside_code) {
			*pointA = intersect_point;
			outcodeA = get_outcode(*pointA, top, bottom);
		} else {
			*pointB = intersect_point;
			outcodeB = get_outcode(*pointB, top, bottom);
		}
	}
}

void clip_wall(Vector2 *pointA, Vector2 *pointB, float leftEdge, float rightEdge) {
	Vector2 leftEndPoint, rightEndPoint, crossing, originalA = *pointA, originalB = *pointB;
	leftEndPoint.x = 1000 * leftEdge;
	rightEndPoint.x = 1000 * rightEdge;
	leftEndPoint.y = 1000;
	rightEndPoint.y = leftEndPoint.y;

	if(intersect(originalA, originalB, zeroV, rightEndPoint, &crossing)) {
		*pointB = crossing;
	}
	if(intersect(originalA, originalB, zeroV, leftEndPoint, &crossing)) {
		*pointA = crossing;
	}
}

void draw_wall(Vector2 pointA, Vector2 pointB, float floor, float ceiling, float leftEdge, float rightEdge, int portal_top, int portal_bottom, int flags) {
	Vector2 lowerA;
	Vector2 lowerB;
	Vector2 crossing;
	Vector2 viewEnd;
	Vector2 leftEndPoint;
	Vector2 rightEndPoint;
	float a_clip, b_clip, far_depth;
	int lower_top, higher_bottom;
	bool top_on_screen, bottom_on_screen;

	floor -= viewer_z_pos;
	ceiling -= viewer_z_pos;

	pointA = vec_sub(pointA , viewerPosition);
	pointB = vec_sub(pointB , viewerPosition);
	pointA = vec_rotate_faster(pointA, viewerAngleSin, viewerAngleCos);
	pointB = vec_rotate_faster(pointB, viewerAngleSin, viewerAngleCos);

	if(line_side(zeroV, pointA, pointB) > 0) return;

	far_depth = max(vec_square_mag(pointA), vec_square_mag(pointB));

	lowerA = pointA;
	lowerB = pointB;

	leftEndPoint.x = 1000 * leftEdge;
	rightEndPoint.x = 1000 * rightEdge;
	leftEndPoint.y = 1000;
	rightEndPoint.y = leftEndPoint.y;


	clip_wall(&pointA, &pointB, leftEdge, rightEdge);

	leftEndPoint.x -= 0.01;
	rightEndPoint.x += 0.01;
	if(!point_inside_triangle(pointA, zeroV, leftEndPoint, rightEndPoint) && !(point_inside_triangle(pointB, zeroV, leftEndPoint, rightEndPoint))) {
		return;
	}


	pointA.x = pointA.x / pointA.y;
	lowerA.y = floor / pointA.y;
	pointA.y = ceiling / pointA.y;

	pointB.x = pointB.x / pointB.y;
	lowerB.y = floor / pointB.y;
	pointB.y = ceiling / pointB.y;

	/*if(pointA.x > pointB.x) return;*/

	pointA = vec_scale_s(pointA, 15);
	pointB = vec_scale_s(pointB, 15);
	lowerA = vec_scale_s(lowerA, 15);
	lowerB = vec_scale_s(lowerB, 15);
	pointA.x *= 12;
	pointB.x *= 12;

	pointA.y *= -1;
	pointB.y *= -1;
	lowerA.y *= -1;
	lowerB.y *= -1;

	pointA.x += SCREEN_WIDTH/2;
	pointA.y += SCREEN_HEIGHT/2;
	pointB.x += SCREEN_WIDTH/2;
	pointB.y += SCREEN_HEIGHT/2;
	lowerA.y += SCREEN_HEIGHT/2;
	lowerB.y += SCREEN_HEIGHT/2;


	lowerA.x = pointA.x;
	lowerB.x = pointB.x;


	
	/*if(color == 51) {
		sprintf(debug_text, "%.2f %.2f %.2f %.2f", lowerA.x, lowerA.y, lowerB.x, lowerB.y);
	}*/

	/* CLIP THE LINES TO THE VIEWPORT,
		THEN CALCULATE HOW MUCH TO COMPENSATE SCREEN FILL */
	a_clip = pointA.x;
	b_clip = pointB.x;
	top_on_screen = clip_line_to_viewport(&pointA, &pointB, portal_top, portal_bottom);
	bottom_on_screen = clip_line_to_viewport(&lowerA, &lowerB, portal_top, portal_bottom);

	lower_top = maxOf(pointA.y, pointB.y);
	higher_bottom = minOf(lowerA.y, lowerB.y);

	if(top_on_screen && (flags & CEILING)) {
		/*interpolate_buffer(0, 31, lower_top, 31 - depth_to_shade(far_depth), gradient_buffer);*/
		/*memset(gradient_buffer, 0, SCREEN_WIDTH);*/
		interpolate_buffer(pointA.x, pointA.y, pointB.x, pointB.y, trape_heights);
		draw_wall_screen(pointA.x, portal_top, pointB.x, portal_top, GRAD_NONE, ceiling_color); /* ceiling */
	}

	memset(trape_heights, (byte) portal_bottom, SCREEN_WIDTH);
	if(bottom_on_screen) {
		if(flags & FLOOR) {
			/*interpolate_buffer(higher_bottom, 31 - depth_to_shade(far_depth), SCREEN_HEIGHT-1, 31, gradient_buffer);*/
			draw_wall_screen(lowerA.x, lowerA.y, lowerB.x, lowerB.y, GRAD_NONE, ground_color); /* draw ground */
		}
		interpolate_buffer(lowerA.x, lowerA.y, lowerB.x, lowerB.y, trape_heights);
	} else if(higher_bottom < 0) {
		return;
	}

	if(flags & WALL) {
		/*interpolate_buffer(a_clip, wall_color_left, b_clip, wall_color_right, gradient_buffer);*/

		if(top_on_screen) {
			draw_wall_screen(
			pointA.x,
			pointA.y,
			pointB.x,
			pointB.y,
			GRAD_H,
			0);

			if(!(pointA.y > portal_top && pointB.y > portal_top)) {
				if(a_clip != pointA.x) {
					draw_wall_screen(
						a_clip,
						portal_top,
						pointA.x,
						portal_top,
						GRAD_H,
						0);
				}
				if(b_clip != pointB.x) {
					draw_wall_screen(
					pointB.x,
					portal_top,
					b_clip,
					portal_top,
					GRAD_H,
					0);
				}
			}
		} else if(!(pointA.y > portal_top && pointB.y > portal_top)) {
			draw_wall_screen(
				pointA.x,
				portal_top,
				pointB.x,
				portal_top,
				GRAD_H,
				0);
		}
	}
}

void draw_sector(Sector s, float leftEdge, float rightEdge, int portal_top, int portal_bottom) {
	int i;
	Vector2 leftSide, rightSide;
	float subLeftEdge, subRightEdge;
	Sector *neighbor;
	if(portal_bottom - portal_top < 2) return;
	if(portal_top < 0) portal_top = 0;
	if(portal_bottom > 199) portal_bottom = 199;
	for(i = 0; i < s.corner_count; i++) {
		byte base_color;
		byte color_left;
		byte color_right;
		leftSide = vec_trans_world(world_vertex_list[s.corners[i]]);
		rightSide = vec_trans_world(world_vertex_list[s.corners[(i+1)%s.corner_count]]);


		if(line_side(zeroV, leftSide, rightSide) < 0) {

			clip_wall(&leftSide, &rightSide, leftEdge, rightEdge);

			base_color = 128 + ((s.color_offset % 8) << 4);
			color_left = depth_to_shade(vec_square_mag(leftSide)) + base_color;
			color_right = depth_to_shade(vec_square_mag(rightSide)) + base_color;

			subLeftEdge = leftSide.x / leftSide.y;
			subRightEdge = rightSide.x / rightSide.y;

			if(subLeftEdge < leftEdge) {
				subLeftEdge = leftEdge;
			}
			if(subRightEdge > rightEdge) {
				subRightEdge = rightEdge;
			}



			if(subLeftEdge < subRightEdge) {
				if(s.neighbors[i] >= 0) {
					draw_sector(world_sector_list[s.neighbors[i]], subLeftEdge, subRightEdge,
						100 - (15 * (world_sector_list[s.neighbors[i]].ceiling - viewer_z_pos) / min(leftSide.y, rightSide.y)),
						100 - (15 * (world_sector_list[s.neighbors[i]].floor - viewer_z_pos) / min(leftSide.y, rightSide.y)));
					sprintf(debug_text, "%.3f", 0);
						/*15 * s.ceiling / max(leftSide.y, rightSide.y) + 100, 15 * s.floor / min(leftSide.y, rightSide.y) + 100);*/
					ceiling_color = s.ceiling_color;
					ground_color = s.ground_color;
					wall_color = base_color;
					wall_color_left = color_left;
					wall_color_right = color_right;
					interpolate_buffer(subLeftEdge * 180 + 160, color_left, subRightEdge * 180 + 160, color_right,gradient_buffer);
					draw_wall(world_vertex_list[s.corners[i]],
						world_vertex_list[s.corners[(i+1)%s.corner_count]],
						s.floor, s.ceiling,
						subLeftEdge, subRightEdge,
						portal_top, portal_bottom,
						CEILING | FLOOR);
					if(world_sector_list[s.neighbors[i]].floor > s.floor)
						draw_wall(world_vertex_list[s.corners[i]],
							world_vertex_list[s.corners[(i+1)%s.corner_count]],
							s.floor, world_sector_list[s.neighbors[i]].floor,
							subLeftEdge, subRightEdge,
							portal_top, portal_bottom,
							WALL);
					if(world_sector_list[s.neighbors[i]].ceiling < s.ceiling)
						draw_wall(world_vertex_list[s.corners[i]],
							world_vertex_list[s.corners[(i+1)%s.corner_count]],
							world_sector_list[s.neighbors[i]].ceiling, s.ceiling,
							subLeftEdge, subRightEdge,
							portal_top, portal_bottom,
							WALL);
				} else {
					ceiling_color = s.ceiling_color;
					ground_color = s.ground_color;
					wall_color = color_left;
					wall_color_left = color_left;
					wall_color_right = color_right;
					interpolate_buffer(subLeftEdge * 180 + 160, color_left, subRightEdge * 180 + 160, color_right, gradient_buffer);
					draw_wall(world_vertex_list[s.corners[i]],
						world_vertex_list[s.corners[(i+1)%s.corner_count]],
						s.floor, s.ceiling,
						subLeftEdge, subRightEdge,
						portal_top, portal_bottom,
						WALL | CEILING | FLOOR);
				}
			}
		}
	}
}

void sector_outline(Sector s) {
	int i;
	Vector2 leftSide, rightSide;
	for(i = 0; i < s.corner_count; i++) {
		leftSide = vec_trans_world(world_vertex_list[s.corners[i]]);
		rightSide = vec_trans_world(world_vertex_list[s.corners[(i+1)%s.corner_count]]);
		leftSide = coords_to_screen(leftSide);
		rightSide = coords_to_screen(rightSide);
		if(clip_line_to_viewport(&leftSide, &rightSide, 0, SCREEN_HEIGHT-1)) {
			draw_line(leftSide.x, leftSide.y, rightSide.x, rightSide.y, 15);
		}
	}
}

void free_first_n_sectors_members(int n) {
	int i;
	for(i = 0; i < n; i ++) {
		free(world_sector_list[i].corners);
		free(world_sector_list[i].neighbors);
	}
}

int load_sectors_from_file(char* filename) {
	FILE *fp;
	char buf[1024];
	int i,k, scanned;

	fp = fopen(filename, "r");
	if(fp == NULL) {
		printf("coult not open file %s\n", filename);
		return 0;
	}

	if(!fgets(buf,1024, fp)) {
		printf("file %s was empty\n", filename);
		return 0;
	}

	if(sscanf(buf, "%d %d", &world_vertex_count, &world_sector_count) != 2 ) {
		printf("header of file %s was invalid\n", filename);
		return 0;
	}

	world_vertex_list = malloc(sizeof(Vector2) * world_vertex_count);

	for(i = 0; i < world_vertex_count; i++) {
		if(!fgets(buf,1024, fp)) {
			free(world_vertex_list);
			world_vertex_list = NULL;
			printf("unexpected end of file in vertex section\n");
			return 0;
		}
		if(sscanf(buf, " %f %f", &(world_vertex_list[i].x), &(world_vertex_list[i].y)) != 2 ) {
			free(world_vertex_list);
			world_vertex_list = NULL;
			printf("invalid format for vertex\n");
			return 0;
		}
	}

	world_sector_list = malloc(sizeof(Sector) * world_sector_count);

	for(i = 0; i < world_sector_count; i++) {
		if(!fgets(buf,1024, fp)) {
			free(world_vertex_list);
			world_vertex_list = NULL;
			free_first_n_sectors_members(i);
			free(world_sector_list);
			world_sector_list = NULL;
			printf("unexpected end of file in sector %d\n", i);
			return 0;
		}
		if(sscanf(buf, "%d", &(world_sector_list[i].corner_count)) != 1 ) {
			free(world_vertex_list);
			world_vertex_list = NULL;
			free_first_n_sectors_members(i);
			free(world_sector_list);
			world_sector_list = NULL;
			printf("invalid format for corner count in sector %d\n", i);
			return 0;
		}
		world_sector_list[i].color_offset = i;
		world_sector_list[i].ceiling_color = (i % 8) + 21;
		world_sector_list[i].ground_color = (i % 8) + 20;
		world_sector_list[i].corners = malloc(sizeof(int) * world_sector_list[i].corner_count);
		world_sector_list[i].neighbors = malloc(sizeof(int) * world_sector_list[i].corner_count);
		for(k = 0; k < world_sector_list[i].corner_count; k++) {
			if(!fgets(buf,1024, fp)) {
				free(world_vertex_list);
				world_vertex_list = NULL;
				free(world_sector_list);
				free_first_n_sectors_members(i+1);
				world_sector_list = NULL;
				printf("unexpected end of file in sector section\n");
				return 0;
			}
			scanned = sscanf(buf, " %d %d", &(world_sector_list[i].corners[k]), &(world_sector_list[i].neighbors[k]));
			if(scanned < 1) {
				free(world_vertex_list);
				world_vertex_list = NULL;
				free(world_sector_list);
				free_first_n_sectors_members(i+1);
				world_sector_list = NULL;
				printf("invalid format for corner reference in sector %d, corner %d\n", i, k);
				return 0;
			}
			if(scanned == 1) {
				world_sector_list[i].neighbors[k] = -1;
			}
		}
		if(!fgets(buf,1024, fp)) {
			free(world_vertex_list);
			world_vertex_list = NULL;
			free(world_sector_list);
			free_first_n_sectors_members(i+1);
			world_sector_list = NULL;
			printf("unexpected end of file at floor/ceiling heights in sector %d\n", i);
			return 0;
		}
		if(sscanf(buf, " %f %f", &(world_sector_list[i].floor), &(world_sector_list[i].ceiling)) != 2 ) {
			free(world_vertex_list);
			world_vertex_list = NULL;
			free_first_n_sectors_members(i);
			free(world_sector_list);
			world_sector_list = NULL;
			printf("invalid format for floor/ceiling heights in sector %d\n", i);
			return 0;
		}
	}
	return 1;
}

void main()
{
	int i, keystates = 0;
	bool found_next_sector = false;
	int* keymap;
	float frame_duration;
	byte unshaded_colors[] = {
		64,  0,  0,
		 0, 64,  0,
		 0,  0, 64,
		 0, 64, 64,
		64,  0, 64,
		64, 64,  0,
		64, 64, 64,
		13, 13, 51
	};
	word start;
	Vector2 old_position;

	Sector square, trapezoid;

	old_position.x = 0;
	old_position.y = 0;

	if(!load_sectors_from_file("TEST3.JSD")) {
		return;
	}

	gradient_buffer = malloc(SCREEN_WIDTH);
	memset(gradient_buffer, 0, SCREEN_WIDTH);

	keymap = malloc(sizeof(int) * sizeof(int) * 8);
	memset(keymap, 0, sizeof(int) * sizeof(int) * 8);
	keymap[0] = 2;
	keymap[1] = 17;
	keymap[2] = 31;
	keymap[3] = 30;
	keymap[4] = 32;
	keymap[5] = 75;
	keymap[6] = 77;
	keymap[7] = 57;

	init_keyboard();

	set_mode(VGA_256_COLOR_MODE);

	setup_palette(unshaded_colors);

	screen_top_right.x = SCREEN_WIDTH - 1;

	screen_bottom_right.x = SCREEN_WIDTH - 1;
	screen_bottom_right.y = SCREEN_HEIGHT - 1;

	screen_bottom_left.y = SCREEN_HEIGHT - 1;
	
				viewerAngle -= PI / 2;
				viewerAngleSin = sin(viewerAngle);
				viewerAngleCos = cos(viewerAngle);

	/*draw_wall(2, 3, -10, 10, 13);*/
	while(!(keystates & QUIT_KEY)) {
		
		start = *my_clock;

		memset(graphic_buffer, 0, 64000L);

		found_next_sector = false;
		if(!point_inside_sector(viewerPosition, world_sector_list[current_sector])) {
			for(i = 0; i < world_sector_list[current_sector].corner_count; i++) {
				if(world_sector_list[current_sector].neighbors[i] >= 0) {
					if(world_sector_list[world_sector_list[current_sector].neighbors[i]].floor < viewer_z_pos - viewer_height + viewer_knee
						&& point_inside_sector(viewerPosition, world_sector_list[world_sector_list[current_sector].neighbors[i]])) {
						current_sector = world_sector_list[current_sector].neighbors[i];
						viewer_z_pos = maxOf(viewer_z_pos, world_sector_list[current_sector].floor + viewer_height);
						found_next_sector = true;
						break;
					}
				}
			}
		} else {
			found_next_sector = true;
		}

		if(!found_next_sector) {
			viewerPosition = old_position;
		}
		draw_sector(world_sector_list[current_sector], -0.5, 0.5, 0, SCREEN_HEIGHT-1);
		request_debug_delay = false;
		/*for(i = 0; i < world_sector_count; i++) {
			sector_outline(world_sector_list[i]);
		}
		draw_line(0, 100, 319, 100, 14);
		draw_line(160, 100, 160, 102, 14);
		draw_line(160, 100, 220, 190, 14);
		draw_line(160, 100, 100, 190, 14);*/
		wait_retrace();

		show_buffer();
		frame_duration = (*my_clock - start) / 18.2;
		/*printf("%.3f\r", frame_duration);*/ 
		/*printf("%s\r", debug_text);*/

		keystates = update_keystates(keystates,keymap, 8);

		old_position = viewerPosition;
		if(viewer_z_pos > world_sector_list[current_sector].floor + viewer_height) {
			viewer_z_pos--;
		} else {

			if(keystates & FORWARD_KEY) {
				viewerPosition.x += cos(viewerAngle + PI / 2);
				viewerPosition.y += sin(viewerAngle + PI / 2);
			}

			if(keystates & BACKWARD_KEY) {
				viewerPosition.x -= cos(viewerAngle + PI / 2);
				viewerPosition.y -= sin(viewerAngle + PI / 2);
			} 

			if(keystates & STRAFELEFT_KEY) {
				viewerPosition.x -= cos(viewerAngle);
				viewerPosition.y -= sin(viewerAngle);
			} 

			if(keystates & STRAFERIGHT_KEY) {
				viewerPosition.x += cos(viewerAngle);
				viewerPosition.y += sin(viewerAngle);
			}

			if(keystates & TURNLEFT_KEY) {
				viewerAngle += 0.1;
				viewerAngleSin = sin(viewerAngle);
				viewerAngleCos = cos(viewerAngle);
			}

			if(keystates & TURNRIGHT_KEY) {
				viewerAngle -= 0.1;
				viewerAngleSin = sin(viewerAngle);
				viewerAngleCos = cos(viewerAngle);
			}

			if(keystates & JUMP_KEY) {
				request_debug_delay = true;
			}
		}
	}
	fade_out();
	wait_retrace();
	set_mode(VGA_TEXT_MODE);
	deinit_keyboard();
	free(keymap);
	return;
}