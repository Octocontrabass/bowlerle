#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

void pngerror( png_structp png_ptr, png_const_charp error_msg )
{
	fprintf( stderr, "libpng error: %s\n", error_msg ? error_msg : "(null)" );
	exit( EXIT_FAILURE );
}

void pngwarning( png_structp png_ptr, png_const_charp warning_msg )
{
	fprintf( stderr, "libpng warning: %s\n", warning_msg ? warning_msg : "(null)" );
	return;
}

uint8_t fgetb( FILE * file, const char * name )
{
	int val = fgetc( file );
	if( val == EOF )
	{
		fprintf( stderr, "Failed to read input file: %s\n", name );
		exit( EXIT_FAILURE );
	}
	return val;
}

void decompress( FILE * file, uint8_t * buffer, const char * name )
{
	while( 1 )
	{
		uint8_t control = fgetb( file, name );
		uint_least16_t length = control & 0x3f;
		if( control & 0x40 )
		{
			length = (length << 8) | fgetb( file, name );
		}
		if( !length ) return;
		if( control & 0x80 )
		{
			while( length-- )
			{
				*buffer++ = fgetb( file, name );
			}
		}
		else
		{
			uint8_t val = fgetb( file, name );
			if( val )
			{
				while( length-- )
				{
					*buffer++ = val;
				}
			}
			else
			{
				buffer += length;
			}
		}
	}
}

int main( int argc, char * * argv )
{
	if( argc < 2 || argc > 3 )
	{
		fprintf( stderr, "Usage: %s input.ani [output.png]\n", argv[0] );
		return (argc == 1) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	char * filename;
	if( argc == 3 )
	{
		filename = malloc( strlen( argv[2] ) + 1 );
		strcpy( filename, argv[2] );
	}
	else
	{
		filename = malloc( strlen( argv[1] ) + strlen( ".png") + 1 );
		strcpy( filename, argv[1] );
		strcat( filename, ".png" );
	}
	FILE * anifile = fopen( argv[1], "rb" );
	if( !anifile )
	{
		fprintf( stderr, "Failed to open input file: %s\n", argv[1] );
		return EXIT_FAILURE;
	}
	uint8_t * buffer = malloc( 4 );
	if( !fread( buffer, 4, 1, anifile ) )
	{
		fprintf( stderr, "Failed to read input file: %s\n", argv[1] );
		return EXIT_FAILURE;
	}
	if( memcmp( buffer, "*ANI", 4 ) )
	{
		fprintf( stderr, "Invalid input file: %s\n", argv[1] );
		return EXIT_FAILURE;
	}
	FILE * pngfile = fopen( filename, "wb" );
	if( !pngfile )
	{
		fprintf( stderr, "Failed to open output file: %s\n", filename );
		return EXIT_FAILURE;
	}
	free( filename );
	png_structp png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, pngerror, pngwarning );
	if( !png_ptr )
	{
		fprintf( stderr, "Failed to initialize libpng.\n" );
		return EXIT_FAILURE;
	}
	png_infop info_ptr = png_create_info_struct( png_ptr );
	if( !info_ptr )
	{
		png_destroy_write_struct( &png_ptr, NULL );
		fprintf( stderr, "Failed to initialize libpng.\n" );
		return EXIT_FAILURE;
	}
	png_init_io( png_ptr, pngfile );
	fseek( anifile, 0xc, SEEK_SET );
	if( !fread( buffer, 4, 1, anifile ) )
	{
		fprintf( stderr, "Failed to read input file: %s\n", argv[1] );
		return EXIT_FAILURE;
	}
	uint_least32_t frames = (uint_least32_t)buffer[0] | ((uint_least32_t)buffer[1] << 8) | ((uint_least32_t)buffer[2] << 16) | ((uint_least32_t)buffer[3] << 24 );
	fseek( anifile, 0x24, SEEK_SET );
	if( !fread( buffer, 4, 1, anifile ) )
	{
		fprintf( stderr, "Failed to read input file: %s\n", argv[1] );
		return EXIT_FAILURE;
	}
	uint_least32_t width = (uint_least32_t)buffer[0] | ((uint_least32_t)buffer[1] << 8) | ((uint_least32_t)buffer[2] << 16) | ((uint_least32_t)buffer[3] << 24 );
	width *= 320;
	fseek( anifile, 0x2c, SEEK_SET );
	if( !fread( buffer, 4, 1, anifile ) )
	{
		fprintf( stderr, "Failed to read input file: %s\n", argv[1] );
		return EXIT_FAILURE;
	}
	uint_least32_t height = (uint_least32_t)buffer[0] | ((uint_least32_t)buffer[1] << 8) | ((uint_least32_t)buffer[2] << 16) | ((uint_least32_t)buffer[3] << 24 );
	png_set_IHDR( png_ptr, info_ptr, width, frames * height, 8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
	fseek( anifile, 0x14, SEEK_SET );
	if( !fread( buffer, 4, 1, anifile ) )
	{
		fprintf( stderr, "Failed to read input file: %s\n", argv[1] );
		return EXIT_FAILURE;
	}
	uint_least32_t pointer = (uint_least32_t)buffer[0] | ((uint_least32_t)buffer[1] << 8) | ((uint_least32_t)buffer[2] << 16) | ((uint_least32_t)buffer[3] << 24 );
	png_color * palette = malloc( sizeof( png_color ) * 256 );
	fseek( anifile, pointer, SEEK_SET );
	for( int i = 0; i < 256; i++ )
	{
		uint8_t byte = fgetb( anifile, argv[1] ) & 0x3f;
		palette[i].red = (byte << 2) | (byte >> 4);
		byte = fgetb( anifile, argv[1] ) & 0x3f;
		palette[i].green = (byte << 2) | (byte >> 4);
		byte = fgetb( anifile, argv[1] ) & 0x3f;
		palette[i].blue = (byte << 2) | (byte >> 4);
	}
	png_set_PLTE( png_ptr, info_ptr, palette, 256 );
	free( palette );
	png_set_sBIT( png_ptr, info_ptr, &(png_color_8){ .red = 6, .green = 6, .blue = 6 } );
	fseek( anifile, 0x18, SEEK_SET );
	if( !fread( buffer, 4, 1, anifile ) )
	{
		fprintf( stderr, "Failed to read input file: %s\n", argv[1] );
		return EXIT_FAILURE;
	}
	pointer = (uint_least32_t)buffer[0] | ((uint_least32_t)buffer[1] << 8) | ((uint_least32_t)buffer[2] << 16) | ((uint_least32_t)buffer[3] << 24 );
	free( buffer );
	buffer = malloc( width * height * frames );
	for( int i = 0; i < frames; i++ )
	{
		if( i )
		{
			memcpy( &buffer[width * height * i], &buffer[width * height * (i - 1)], width * height );
		}
		else
		{
			memset( buffer, 0, width * height );
		}
		fseek( anifile, pointer + 4 * i, SEEK_SET );
		uint_least32_t pointer2 = fgetb( anifile, argv[1] );
		pointer2 |= (uint_least32_t)fgetb( anifile, argv[1] ) << 8;
		pointer2 |= (uint_least32_t)fgetb( anifile, argv[1] ) << 16;
		pointer2 |= (uint_least32_t)fgetb( anifile, argv[1] ) << 24;
		fseek( anifile, pointer2, SEEK_SET );
		decompress( anifile, &buffer[width * height * i], argv[1] );
	}
	png_bytep * rows = malloc( height * frames * sizeof( png_bytep ) );
	for( int i = 0; i < height * frames; i++ )
	{
		rows[i] = &buffer[width * i];
	}
	png_set_rows( png_ptr, info_ptr, rows );
	png_write_png( png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL );
	free( rows );
	free( buffer );
	return EXIT_SUCCESS;
}
