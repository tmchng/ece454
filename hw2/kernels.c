/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/*
 * ECE454 Students:
 * Please fill in the following team struct
 */
team_t team = {
    "TeamTW",              /* Team name */

    "Ting-Hao (Tim) Cheng",     /* First member full name */
    "chengti2@ecf.utoronto.ca",  /* First member email address */

    "Corey Yen",                   /* Second member full name (leave blank if none) */
    "yenyung1@ecf.utoronto.ca"                    /* Second member email addr (leave blank if none) */
};

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/

/*
 * naive_rotate - The naive baseline version of rotate
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst)
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}

/*
 * ECE 454 Students: Write your rotate functions here:
 */

void jump_rotate(int dim, pixel *src, pixel *dst, int jump)
{
    /**
     * Each pixel is 16+16+16 = 48bits or 6 bytes.
     *
     * L1 cache on UG machines are 32KB big, which means it would fit 5460 pixles.
     *
     * Biggest problem is row jumps when writing to src.
     *
     * Cache line is 64B = 10 pixels, so if we jump every 6 px?
     *
     *
     */

    // Lab handout says dim will be multiples of 32.
    const int T = jump;

    int i = 0, j = 0, i2, j2, it, jt;
    for (i = 0; i < dim; i += T)
    {
	it = i + T;

	for (j = 0; j < dim; j += T)
	{
	    jt = j + T;


	    for (i2 = i; i2 < it; i2++)
	    {
		for (j2 = j; j2 < jt; j2++)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		}
	    }

	}
    }

    // Incase some pixels are left to be processed
    for (; i < dim; i++)
    {
	for (; j < dim; j++)
	{
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
	}
    }
}

void jump_rotate2_64(int dim, pixel *src, pixel *dst)
{
    int T = 64;

    int i = 0, j = 0, i2, j2, it, jt;
    for (i = 0; i < dim; i += T)
    {
	it = i+T > dim ? dim : i+T;

	for (j = 0; j < dim; j += T)
	{
	    jt = j+T > dim ? dim : j+T;


	    for (i2 = i; i2 < it; i2+=2)
	    {
		for (j2 = j; j2 < jt; j2+=2)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
		    dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
		    dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		}
	    }

	}
    }
}

void jump_rotate2(int dim, pixel *src, pixel *dst, int jump)
{
    const int T = jump;

    int i = 0, j = 0, i2, j2, it, jt;
    for (i = 0; i < dim; i += T)
    {
	it = i + T;

	for (j = 0; j < dim; j += T)
	{
	    jt = j + T;


	    for (i2 = i; i2 < it; i2+=2)
	    {
		for (j2 = j; j2 < jt; j2+=2)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
		    dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
		    dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		}
	    }

	}
    }

    // Incase some pixels are left to be processed
    for (; i < dim; i++)
    {
	for (; j < dim; j++)
	{
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
	}
    }
}

void select_rotate2(int dim, pixel *src, pixel *dst)
{
    /**
     * Each pixel is 16+16+16 = 48bits or 6 bytes.
     *
     * L1 cache on UG machines are 32KB big, which means it would fit 5460 pixles.
     *
     * Biggest problem is row jumps when writing to src.
     *
     * Cache line is 64B = 10 pixels,
     *
     *
     */

    const int T32 = 32;
    const int Ti = 256;
    const int Tj = 16;
    int i = 0, j = 0, i2, j2, it, jt;

    if (dim >= 1024) {
	for (i = 0; i < dim; i += Ti) {
	    it = i + Ti;
	    if (it > dim) {
		it = dim;
	    }

	    for (j = 0; j < dim; j += Tj) {
		jt = j + Tj;
		if (jt > dim) {
		    jt = dim;
		}

		for (i2 = i; i2 < it; i2+=2) {
		    for (j2 = j; j2 < jt; j2+=2) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
			dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
			dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
			dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		    }
		}
	    }
	}
    } else if (dim <= 512){
	for (i = 0; i < dim; i += Ti) {
	    it = i + Ti;
	    if (it > dim) {
		it = dim;
	    }

	    for (j = 0; j < dim; j += Tj) {
		jt = j + Tj;
		if (jt > dim) {
		    jt = dim;
		}

		for (i2 = i; i2 < it; i2++) {
		    for (j2 = j; j2 < jt; j2++) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    }
		}
	    }
	}
    } else {
	for (i = 0; i < dim; i += T32) {
	    it = i + T32;

	    for (j = 0; j < dim; j += T32) {
		jt = j + T32;

		for (j2 = j; j2 < jt; j2++) {
		    for (i2 = i; i2 < it; i2++) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    }
		}
	    }
	}
    }
}

void select_rotate3(int dim, pixel *src, pixel *dst)
{
    /**
     * Each pixel is 16+16+16 = 48bits or 6 bytes.
     *
     * L1 cache on UG machines are 32KB big, which means it would fit 5460 pixles.
     *
     * Biggest problem is row jumps when writing to src.
     *
     * Cache line is 64B = 10 pixels,
     *
     *
     */

    const int T32 = 32;
    const int Ti = 256;
    const int Tj = 16;
    int i = 0, j = 0, i2, j2, it, jt;

    if (dim >= 512) {
	for (i = 0; i < dim; i += Ti) {
	    it = i + Ti;
	    if (it > dim) {
		it = dim;
	    }

	    for (j = 0; j < dim; j += Tj) {
		jt = j + Tj;
		if (jt > dim) {
		    jt = dim;
		}

		for (j2 = j; j2 < jt; j2+=2) {
		    for (i2 = i; i2 < it; i2+=2) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
			dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
			dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
			dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		    }
		}
	    }
	}
    } else {
	for (i = 0; i < dim; i += T32) {
	    it = i + T32;

	    for (j = 0; j < dim; j += T32) {
		jt = j + T32;

		for (j2 = j; j2 < jt; j2++) {
		    for (i2 = i; i2 < it; i2++) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    }
		}
	    }
	}
    }
}

void select_rotate4(int dim, pixel *src, pixel *dst)
{
    /**
     * Each pixel is 16+16+16 = 48bits or 6 bytes.
     *
     * L1 cache on UG machines are 32KB big, which means it would fit 5460 pixles.
     *
     * Biggest problem is row jumps when writing to src.
     *
     * Cache line is 64B = 10 pixels,
     *
     *
     */

    const int T32 = 32;
    const int Ti = 64;
    const int Tj = 64;
    int i = 0, j = 0, i2, j2, it, jt;

    if (dim >= 512) {
	for (i = 0; i < dim; i += Ti) {
	    it = i + Ti;
	    if (it > dim) {
		it = dim;
	    }

	    for (j = 0; j < dim; j += Tj) {
		jt = j + Tj;
		if (jt > dim) {
		    jt = dim;
		}

		for (i2 = i; i2 < it; i2+=2) {
		    for (j2 = j; j2 < jt; j2+=2) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
			dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
			dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
			dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		    }
		}
	    }
	}
    } else {
	for (i = 0; i < dim; i += T32) {
	    it = i + T32;

	    for (j = 0; j < dim; j += T32) {
		jt = j + T32;

		for (j2 = j; j2 < jt; j2++) {
		    for (i2 = i; i2 < it; i2++) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    }
		}
	    }
	}
    }
}

void select_rotate5(int dim, pixel *src, pixel *dst)
{
    /**
     * Each pixel is 16+16+16 = 48bits or 6 bytes.
     *
     * L1 cache on UG machines are 32KB big, which means it would fit 5460 pixles.
     *
     * Biggest problem is row jumps when writing to src.
     *
     * Cache line is 64B = 10 pixels,
     *
     *
     */

    const int T32 = 32;
    const int Ti = 128;
    const int Tj = 32;
    int i = 0, j = 0, i2, j2, it, jt;

    if (dim >= 512) {
	for (i = 0; i < dim; i += Ti) {
	    it = i + Ti;
	    if (it > dim) {
		it = dim;
	    }

	    for (j = 0; j < dim; j += Tj) {
		jt = j + Tj;
		if (jt > dim) {
		    jt = dim;
		}

		for (i2 = i; i2 < it; i2+=2) {
		    for (j2 = j; j2 < jt; j2+=2) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
			dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
			dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
			dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		    }
		}
	    }
	}
    } else {
	for (i = 0; i < dim; i += T32) {
	    it = i + T32;

	    for (j = 0; j < dim; j += T32) {
		jt = j + T32;

		for (j2 = j; j2 < jt; j2++) {
		    for (i2 = i; i2 < it; i2++) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    }
		}
	    }
	}
    }
}

void select_rotate(int dim, pixel *src, pixel *dst)
{
    /**
     * Each pixel is 16+16+16 = 48bits or 6 bytes.
     *
     * L1 cache on UG machines are 32KB big, which means it would fit 5460 pixles.
     *
     * Cache line is 64B ~= 8 pixels,
     *
     */

    const int T32 = 32;
    const int Ti = 256;
    const int Tj = 16;
    int i = 0, j = 0, i2, j2, it, jt;

    if (dim >= 512) {
	for (i = 0; i < dim; i += Ti) {
	    it = i + Ti;
	    if (it > dim) {
		it = dim;
	    }

	    for (j = 0; j < dim; j += Tj) {
		jt = j + Tj;
		if (jt > dim) {
		    jt = dim;
		}

		for (i2 = i; i2 < it; i2+=2) {
		    for (j2 = j; j2 < jt; j2+=2) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
			dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
			dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
			dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		    }
		}
	    }
	}
    } else {
	// Large tile size and loop unrolling are not ideal for smaller dimensions.
	// 32x32 works the best after testing.
	for (i = 0; i < dim; i += T32) {
	    it = i + T32;

	    for (j = 0; j < dim; j += T32) {
		jt = j + T32;

		for (j2 = j; j2 < jt; j2++) {
		    for (i2 = i; i2 < it; i2++) {
			dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    }
		}
	    }
	}
    }
}

void select_rotate_nounroll(int dim, pixel *src, pixel *dst)
{
    /**
     * Each pixel is 16+16+16 = 48bits or 6 bytes.
     *
     * L1 cache on UG machines are 32KB big, which means it would fit 5460 pixles.
     *
     * Biggest problem is row jumps when writing to src.
     *
     * Cache line is 64B = 10 pixels,
     *
     *
     */

    const int T = 32;
    int i = 0, j = 0, i2, j2, it, jt, Ti, Tj;

    if (dim >= 512) {
	Ti = 256;
	Tj = 16;
    } else {
	Ti = 32;
	Tj = 32;
    }



    for (i = 0; i < dim; i += Ti)
    {
	it = i + Ti;
	for (j = 0; j < dim; j += Tj)
	{
	    jt = j + Tj;
	    for (i2 = i; i2 < it; i2++)
	    {
		for (j2 = j; j2 < jt; j2++)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		}
	    }

	}
    }
}
void fast_rotate(int dim, pixel *src, pixel *dst, const int Ti, const int Tj)
{
    /**
     * Each pixel is 16+16+16 = 48bits or 6 bytes.
     *
     * L1 cache on UG machines are 32KB big, which means it would fit 5460 pixles.
     *
     * Biggest problem is row jumps when writing to src.
     *
     * Cache line is 64B = 10 pixels,
     *
     *
     */

    const int T = 32;
    int i = 0, j = 0, i2, j2, it, jt;

    for (i = 0; i < dim; i += Ti)
    {
	it = i + Ti;
	if (it > dim) {
	    it = dim;
	}

	for (j = 0; j < dim; j += Tj)
	{
	    jt = j + Tj;
	    if (jt > dim) {
		jt = dim;
	    }


	    for (i2 = i; i2 < it; i2+=2)
	    {
		for (j2 = j; j2 < jt; j2+=2)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
		    dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
		    dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		}
	    }

	}
    }

    // Incase some pixels are left to be processed
    for (; i < dim; i++)
    {
	for (; j < dim; j++)
	{
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
	}
    }
}

void jump_rotate3(int dim, pixel *src, pixel *dst, int jump)
{
    const int T = jump;

    int i = 0, j = 0, i2, j2, it, jt;
    for (i = 0; i < dim; i += T)
    {
	it = i + T - 3;

	for (j = 0; j < dim; j += T)
	{
	    jt = j + T - 3;


	    for (i2 = i; i2 < it; i2+=3)
	    {
		for (j2 = j; j2 < jt; j2+=3)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
		    dst[RIDX(dim-1-j2, i2+2, dim)] = src[RIDX(i2+2, j2, dim)];

		    dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
		    dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		    dst[RIDX(dim-1-j2-1, i2+2, dim)] = src[RIDX(i2+2, j2+1, dim)];

		    dst[RIDX(dim-1-j2-2, i2, dim)] = src[RIDX(i2, j2+2, dim)];
		    dst[RIDX(dim-1-j2-2, i2+1, dim)] = src[RIDX(i2+1, j2+2, dim)];
		    dst[RIDX(dim-1-j2-2, i2+2, dim)] = src[RIDX(i2+2, j2+2, dim)];
		}

	    }

	    for (i2 = it; i2 < i+T; i2++)
	    {
		for (j2 = jt; j2 < j+T; j2++)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		}
	    }
	}
    }

    // Incase some pixels are left to be processed
    for (; i < dim; i++)
    {
	for (; j < dim; j++)
	{
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
	}
    }
}

void jump_rotate4(int dim, pixel *src, pixel *dst, int jump)
{
    const int T = jump;

    int i = 0, j = 0, i2, j2, it, jt;
    for (i = 0; i < dim; i += T)
    {
	it = i + T;

	for (j = 0; j < dim; j += T)
	{
	    jt = j + T;


	    for (i2 = i; i2 < it; i2+=4)
	    {
		for (j2 = j; j2 < jt; j2+=4)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		    dst[RIDX(dim-1-j2, i2+1, dim)] = src[RIDX(i2+1, j2, dim)];
		    dst[RIDX(dim-1-j2, i2+2, dim)] = src[RIDX(i2+2, j2, dim)];
		    dst[RIDX(dim-1-j2, i2+3, dim)] = src[RIDX(i2+3, j2, dim)];

		    dst[RIDX(dim-1-j2-1, i2, dim)] = src[RIDX(i2, j2+1, dim)];
		    dst[RIDX(dim-1-j2-1, i2+1, dim)] = src[RIDX(i2+1, j2+1, dim)];
		    dst[RIDX(dim-1-j2-1, i2+2, dim)] = src[RIDX(i2+2, j2+1, dim)];
		    dst[RIDX(dim-1-j2-1, i2+3, dim)] = src[RIDX(i2+3, j2+1, dim)];

		    dst[RIDX(dim-1-j2-2, i2, dim)] = src[RIDX(i2, j2+2, dim)];
		    dst[RIDX(dim-1-j2-2, i2+1, dim)] = src[RIDX(i2+1, j2+2, dim)];
		    dst[RIDX(dim-1-j2-2, i2+2, dim)] = src[RIDX(i2+2, j2+2, dim)];
		    dst[RIDX(dim-1-j2-2, i2+3, dim)] = src[RIDX(i2+3, j2+2, dim)];

		    dst[RIDX(dim-1-j2-3, i2, dim)] = src[RIDX(i2, j2+3, dim)];
		    dst[RIDX(dim-1-j2-3, i2+1, dim)] = src[RIDX(i2+1, j2+3, dim)];
		    dst[RIDX(dim-1-j2-3, i2+2, dim)] = src[RIDX(i2+2, j2+3, dim)];
		    dst[RIDX(dim-1-j2-3, i2+3, dim)] = src[RIDX(i2+3, j2+3, dim)];
		}
	    }

	    for (i2 = i2-4; i2 < i+T; i2++)
	    {
		for (j2 = j2-4; j2 < j+T; j2++)
		{
		    dst[RIDX(dim-1-j2, i2, dim)] = src[RIDX(i2, j2, dim)];
		}
	    }
	}
    }

    // Incase some pixels are left to be processed
    for (; i < dim; i++)
    {
	for (; j < dim; j++)
	{
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
	}
    }
}
/*
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_descr[] = "rotate: Current working version. 256x16 tile";
void rotate(int dim, pixel *src, pixel *dst)
{
    select_rotate(dim, src, dst);
}



/*
 * second attempt (commented o b of 32.
 *     const int T = 32;
 *     emultiples of 32.
 const int T = 32;
 ut for now)
 char rotate_two_descr[] = "second attempt";
 void attempt_two(int dim, pixel *src, pixel *dst)
 {
 naive_rotate(dim, src, dst);
 }
 */


/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.
 *********************************************************************/

void register_rotate_functions()
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);
    add_rotate_function(&rotate, rotate_descr);

    //add_rotate_function(&attempt_two, rotate_two_descr);
    //add_rotate_function(&attempt_three, rotate_three_descr);
    //add_rotate_function(&attempt_four, rotate_four_descr);
    //add_rotate_function(&attempt_five, rotate_five_descr);
    //add_rotate_function(&attempt_six, rotate_six_descr);
    //add_rotate_function(&attempt_seven, rotate_seven_descr);
    //add_rotate_function(&attempt_eight, rotate_eight_descr);
    //add_rotate_function(&attempt_nine, rotate_nine_descr);
    //add_rotate_function(&attempt_ten, rotate_ten_descr);
    //add_rotate_function(&attempt_eleven, rotate_eleven_descr);

    /* ... Register additional rotate functions here */
}

