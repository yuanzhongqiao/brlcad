/*
 *			R L E - P I X . C
 *
 *  Decode a Utah Raster Toolkit RLE image, and output as a pix(5) file.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "fb.h"
#include "svfb_global.h"

extern int	optind;
extern char	*optarg;
extern char	*getenv();

extern char	*malloc();

static FILE	*infp = stdin;
static char	*infile;
static FILE	*outfp = stdout;

static int	background[3];
static int	override_background;

unsigned char	*rows[4];		/* Character pointers for rle_getrow */
	
static RGBpixel	*scan_buf;		/* single scanline buffer */
static RGBpixel	*bg_buf;		/* single scanline of background */
static ColorMap	cmap;

static int	screen_width = 0;
static int	screen_height = 0;

static int	crunch;
static int	r_debug;

static char	usage[] = "\
Usage: rle-pix [-c -d -h] [-C r/g/b]\n\
	[file.rle [file.pix]]\n\
";

/*
 *			G E T _ A R G S
 */
static int
get_args( argc, argv )
register char	**argv;
{
	register int	c;

	while( (c = getopt( argc, argv, "cdhs:S:w:W:n:N:C:" )) != EOF )  {
		switch( c )  {
		case 'd':
			r_debug = 1;
			break;
		case 'c':
			crunch = 1;
			break;
		case 'h':
			/* high-res */
			screen_height = screen_width = 1024;
			break;
		case 'S':
		case 's':
			/* square screen size */
			screen_height = screen_width = atoi(optarg);
			break;
		case 'W':
		case 'w':
			screen_width = atoi(optarg);
			break;
		case 'N':
		case 'n':
			screen_height = atoi(optarg);
			break;
		case 'C':
			{
				register char *cp = optarg;
				register int *conp = background;

				/* premature null => atoi gives zeros */
				for( c=0; c < 3; c++ )  {
					*conp++ = atoi(cp);
					while( *cp && *cp++ != '/' ) ;
				}
				override_background = 1;
			}
			break;
		default:
		case '?':
			return	0;
		}
	}
	if( argv[optind] != NULL )  {
		if( (infp = fopen( (infile=argv[optind]), "r" )) == NULL )  {
			perror(infile);
			return	0;
		}
		optind++;
	} else {
		infile = "-";
	}
	if( argv[optind] != NULL )  {
		if( access( argv[optind], 0 ) == 0 )  {
			(void) fprintf( stderr,
				"rle-pix: \"%s\" already exists.\n",
				argv[optind] );
			exit( 1 );
		}
		if( (outfp = fopen( argv[optind], "w" )) == NULL )  {
			perror(argv[optind]);
			return	0;
		}
	}
	if( argc > ++optind )
		(void) fprintf( stderr, "rle-pix:  excess arguments ignored\n" );

	if( isatty(fileno(infp)) || isatty(fileno(outfp)) )
		return 0;
	return	1;
}

/*
 *			M A I N
 */
main( argc, argv)
int argc;
char ** argv;
{
	FBIO	*fbp;
	register int i;
	int	file_width;		/* unclipped width of rectangle */
	int	file_skiplen;		/* # of pixels to skip on l.h.s. */
	int	screen_xbase;		/* screen X of l.h.s. of rectangle */
	int	screen_xlen;		/* clipped len of rectangle */
	int	ncolors;

	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	sv_globals.svfb_fd = infp;
	if( rle_get_setup( &sv_globals ) < 0 )  {
		fprintf(stderr, "rle-pix: Error reading setup information\n");
		exit(1);
	}

	if (r_debug)  {
		fprintf( stderr,"Image bounds\n\tmin %d %d\n\tmax %d %d\n",
			sv_globals.sv_xmin, sv_globals.sv_ymin,
			sv_globals.sv_xmax, sv_globals.sv_ymax );
		fprintf(stderr, "%d color channels\n", sv_globals.sv_ncolors);
		fprintf(stderr,"%d color map channels\n", sv_globals.sv_ncmap);
		if ( sv_globals.sv_alpha )
			fprintf( stderr, "Alpha Channel present in input, ignored.\n");
		for( i=0; i < sv_globals.sv_ncolors; i++ )
			fprintf(stderr,"Background channel %d = %d\n",
				i, sv_globals.sv_bg_color[i] );
		rle_debug(1);
	}

	if( sv_globals.sv_ncmap == 0 )
		crunch = 0;

	/* Only interested in R, G, & B */
	SV_CLR_BIT(sv_globals, SV_ALPHA);
	for (i = 3; i < sv_globals.sv_ncolors; i++)
		SV_CLR_BIT(sv_globals, i);
	ncolors = sv_globals.sv_ncolors > 3 ? 3 : sv_globals.sv_ncolors;

	/* Optional background color override */
	if( override_background )  {
		for( i=0; i<ncolors; i++ )
			sv_globals.sv_bg_color[i] = background[i];
	}

	file_width = sv_globals.sv_xmax - sv_globals.sv_xmin + 1;

	/* Default screen (output) size tracks input rectangle upper right corner */
	if( screen_width == 0 )  {
	    	screen_width = sv_globals.sv_xmax + 1;
	}
	if( screen_height == 0 )  {
	    	screen_height = sv_globals.sv_ymax + 1;
	}

	/* Discard any scanlines which exceed screen height */
	if( sv_globals.sv_ymax > screen_height-1 )
		sv_globals.sv_ymax = screen_height-1;

	/* Clip left edge */
	screen_xbase = sv_globals.sv_xmin;
	screen_xlen = screen_width;
	file_skiplen = 0;
	if( screen_xbase < 0 )  {
		file_skiplen = -screen_xbase;
		screen_xbase = 0;
		screen_xlen -= file_skiplen;
	}
	/* Clip right edge */
	if( screen_xbase + screen_xlen > screen_width )
		screen_xlen = screen_width - screen_xbase;

	if( screen_xlen <= 0 ||
	    sv_globals.sv_ymin > screen_height ||
	    sv_globals.sv_ymax < 0 )  {
	    	fprintf(stderr,
		"rle-pix:  Warning:  RLE image rectangle entirely off screen\n");
		goto done;
	}

	/* NOTE:  This code can't do repositioning very well.
	 * background flooding on the edges is needed, at a minimum.
 	 */

	scan_buf = (RGBpixel *)malloc( sizeof(RGBpixel) * screen_width );
	bg_buf = (RGBpixel *)malloc( sizeof(RGBpixel) * screen_width );

	/* Fill in background buffer */
	for( i=0; i<screen_xlen; i++ )  {
		bg_buf[i][0] = sv_globals.sv_bg_color[0];
		bg_buf[i][1] = sv_globals.sv_bg_color[1];
		bg_buf[i][2] = sv_globals.sv_bg_color[2];
	}

	for( i=0; i < ncolors; i++ )
		rows[i] = (unsigned char *)malloc(file_width);
	for( ; i < 3; i++ )
		rows[i] = rows[0];	/* handle monochrome images */

	/*
	 *  Import Utah color map, converting to libfb format.
	 *  Check for old format color maps, where high 8 bits
	 *  were zero, and correct them.
	 *  XXX need to handle < 3 channels of color map, by replication.
	 */
	if( crunch && sv_globals.sv_ncmap > 0 )  {
		register int maplen = (1 << sv_globals.sv_cmaplen);
		register int all = 0;
		for( i=0; i<256; i++ )  {
			cmap.cm_red[i] = sv_globals.sv_cmap[i];
			cmap.cm_green[i] = sv_globals.sv_cmap[i+maplen];
			cmap.cm_blue[i] = sv_globals.sv_cmap[i+2*maplen];
			all |= cmap.cm_red[i] | cmap.cm_green[i] |
				cmap.cm_blue[i];
		}
		if( (all & 0xFF00) == 0 && (all & 0x00FF) != 0 )  {
			/*  This is an old (Edition 2) color map.
			 *  Correct by shifting it left 8 bits.
			 */
			for( i=0; i<256; i++ )  {
				cmap.cm_red[i] <<= 8;
				cmap.cm_green[i] <<= 8;
				cmap.cm_blue[i] <<= 8;
			}
			fprintf(stderr,
				"rle-pix: correcting for old style colormap\n");
		}
	}

	/* Handle any lines below zero in y.  Decode and discard. */
	for( i = sv_globals.sv_ymin; i < 0; i++ )
		rle_getrow( &sv_globals, rows );

	/* Background-fill any lines above 0, below ymin */
	for( i=0; i < sv_globals.sv_ymin; i++ )
		fwrite( (char *)bg_buf, sizeof(RGBpixel), screen_xlen, outfp );

	for( ; i <= sv_globals.sv_ymax; i++)  {
		register unsigned char	*pp = (unsigned char *)scan_buf;
		register rle_pixel	*rp = &(rows[0][file_skiplen]);
		register rle_pixel	*gp = &(rows[1][file_skiplen]);
		register rle_pixel	*bp = &(rows[2][file_skiplen]);
		register int		j;

		rle_getrow(&sv_globals, rows );

		/* Grumble, convert from Utah layout */
		if( !crunch )  {
			for( j = 0; j < screen_xlen; j++)  {
				*pp++ = *rp++;
				*pp++ = *gp++;
				*pp++ = *bp++;
			}
		} else {
			for( j = 0; j < screen_xlen; j++)  {
				*pp++ = cmap.cm_red[*rp++]>>8;
				*pp++ = cmap.cm_green[*gp++]>>8;
				*pp++ = cmap.cm_blue[*bp++]>>8;
			}
		}
		fwrite( (char *)scan_buf, sizeof(RGBpixel), screen_xlen, outfp );
	}

	/* Background-fill any lines above ymax, below screen_height */
	for( ; i < screen_height; i++ )
		fwrite( (char *)bg_buf, sizeof(RGBpixel), screen_xlen, outfp );
done:
	fclose( outfp );
	exit(0);
}
