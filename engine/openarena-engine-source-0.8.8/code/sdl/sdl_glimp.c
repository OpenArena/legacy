/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#ifdef SMP
#	ifdef USE_LOCAL_HEADERS
#		include "SDL_thread.h"
#	else
#		include <SDL_thread.h>
#	endif
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "../sys/sys_local.h"
#include "sdl_icon.h"

/* Just hack it for now. */
#ifdef MACOS_X
#include <OpenGL/OpenGL.h>
typedef CGLContextObj QGLContext;
#define GLimp_GetCurrentContext() CGLGetCurrentContext()
#define GLimp_SetCurrentContext(ctx) CGLSetCurrentContext(ctx)
#else
typedef void *QGLContext;
#define GLimp_GetCurrentContext() (NULL)
#define GLimp_SetCurrentContext(ctx)
#endif

static QGLContext opengl_context;

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

static SDL_Surface *screen = NULL;
static const SDL_VideoInfo *videoInfo = NULL;

cvar_t *r_allowSoftwareGL; // Don't abort out if a hardware visual can't be obtained
cvar_t *r_allowResize; // make window resizable
cvar_t *r_centerWindow;
cvar_t *r_sdlDriver;

void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);

void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);

// GL_ARB_shader_objects
GLvoid (APIENTRYP qglDeleteObjectARB) (GLhandleARB obj);
GLhandleARB (APIENTRYP qglGetHandleARB) (GLenum pname);
GLvoid (APIENTRYP qglDetachObjectARB) (GLhandleARB containerObj, GLhandleARB attachedObj);
GLhandleARB (APIENTRYP qglCreateShaderObjectARB) (GLenum shaderType);
GLvoid (APIENTRYP qglShaderSourceARB) (GLhandleARB shaderObj, GLsizei count, const GLcharARB **string,
				       const GLint *length);
GLvoid (APIENTRYP qglCompileShaderARB) (GLhandleARB shaderObj);
GLhandleARB (APIENTRYP qglCreateProgramObjectARB) (void);
GLvoid (APIENTRYP qglAttachObjectARB) (GLhandleARB containerObj, GLhandleARB obj);
GLvoid (APIENTRYP qglLinkProgramARB) (GLhandleARB programObj);
GLvoid (APIENTRYP qglUseProgramObjectARB) (GLhandleARB programObj);
GLvoid (APIENTRYP qglValidateProgramARB) (GLhandleARB programObj);
GLvoid (APIENTRYP qglUniform1fARB) (GLint location, GLfloat v0);
GLvoid (APIENTRYP qglUniform2fARB) (GLint location, GLfloat v0, GLfloat v1);
GLvoid (APIENTRYP qglUniform3fARB) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
GLvoid (APIENTRYP qglUniform4fARB) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
GLvoid (APIENTRYP qglUniform1iARB) (GLint location, GLint v0);
GLvoid (APIENTRYP qglUniform2iARB) (GLint location, GLint v0, GLint v1);
GLvoid (APIENTRYP qglUniform3iARB) (GLint location, GLint v0, GLint v1, GLint v2);
GLvoid (APIENTRYP qglUniform4iARB) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
GLvoid (APIENTRYP qglUniform1fvARB) (GLint location, GLsizei count, const GLfloat *value);
GLvoid (APIENTRYP qglUniform2fvARB) (GLint location, GLsizei count, const GLfloat *value);
GLvoid (APIENTRYP qglUniform3fvARB) (GLint location, GLsizei count, const GLfloat *value);
GLvoid (APIENTRYP qglUniform4fvARB) (GLint location, GLsizei count, const GLfloat *value);
GLvoid (APIENTRYP qglUniform1ivARB) (GLint location, GLsizei count, const GLint *value);
GLvoid (APIENTRYP qglUniform2ivARB) (GLint location, GLsizei count, const GLint *value);
GLvoid (APIENTRYP qglUniform3ivARB) (GLint location, GLsizei count, const GLint *value);
GLvoid (APIENTRYP qglUniform4ivARB) (GLint location, GLsizei count, const GLint *value);
GLvoid (APIENTRYP qglUniformMatrix2fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GLvoid (APIENTRYP qglUniformMatrix3fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GLvoid (APIENTRYP qglUniformMatrix4fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GLvoid (APIENTRYP qglGetObjectParameterfvARB) (GLhandleARB obj, GLenum pname, GLfloat *params);
GLvoid (APIENTRYP qglGetObjectParameterivARB) (GLhandleARB obj, GLenum pname, GLint *params);
GLvoid (APIENTRYP qglGetInfoLogARB) (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
GLvoid (APIENTRYP qglGetAttachedObjectsARB) (GLhandleARB containerObj, GLsizei maxCount, GLsizei *count,
					     GLhandleARB *obj);
GLint(APIENTRYP qglGetUniformLocationARB) (GLhandleARB programObj, const GLcharARB * name);
GLvoid (APIENTRYP qglGetActiveUniformARB) (GLhandleARB programObj, GLuint index, GLsizei maxLength,
					   GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
GLvoid (APIENTRYP qglGetUniformfvARB) (GLhandleARB programObj, GLint location, GLfloat *params);
GLvoid (APIENTRYP qglGetUniformivARB) (GLhandleARB programObj, GLint location, GLint *params);
GLvoid (APIENTRYP qglGetShaderSourceARB) (GLhandleARB obj, GLsizei maxLength, GLsizei *length,
					  GLcharARB *source);
GLvoid (APIENTRYP qglVertexAttrib1fARB) (GLuint index, GLfloat v0);
GLvoid (APIENTRYP qglVertexAttrib1sARB) (GLuint index, GLshort v0);
GLvoid (APIENTRYP qglVertexAttrib1dARB) (GLuint index, GLdouble v0);
GLvoid (APIENTRYP qglVertexAttrib2fARB) (GLuint index, GLfloat v0, GLfloat v1);
GLvoid (APIENTRYP qglVertexAttrib2sARB) (GLuint index, GLshort v0, GLshort v1);
GLvoid (APIENTRYP qglVertexAttrib2dARB) (GLuint index, GLdouble v0, GLdouble v1);
GLvoid (APIENTRYP qglVertexAttrib3fARB) (GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
GLvoid (APIENTRYP qglVertexAttrib3sARB) (GLuint index, GLshort v0, GLshort v1, GLshort v2);
GLvoid (APIENTRYP qglVertexAttrib3dARB) (GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
GLvoid (APIENTRYP qglVertexAttrib4fARB) (GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
GLvoid (APIENTRYP qglVertexAttrib4sARB) (GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
GLvoid (APIENTRYP qglVertexAttrib4dARB) (GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
GLvoid (APIENTRYP qglVertexAttrib4NubARB) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
GLvoid (APIENTRYP qglVertexAttrib1fvARB) (GLuint index, GLfloat *v);
GLvoid (APIENTRYP qglVertexAttrib1svARB) (GLuint index, GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib1dvARB) (GLuint index, GLdouble *v);
GLvoid (APIENTRYP qglVertexAttrib2fvARB) (GLuint index, GLfloat *v);
GLvoid (APIENTRYP qglVertexAttrib2svARB) (GLuint index, GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib2dvARB) (GLuint index, GLdouble *v);
GLvoid (APIENTRYP qglVertexAttrib3fvARB) (GLuint index, GLfloat *v);
GLvoid (APIENTRYP qglVertexAttrib3svARB) (GLuint index, GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib3dvARB) (GLuint index, GLdouble *v);
GLvoid (APIENTRYP qglVertexAttrib4fvARB) (GLuint index, GLfloat *v);
GLvoid (APIENTRYP qglVertexAttrib4svARB) (GLuint index, GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib4dvARB) (GLuint index, GLdouble *v);
GLvoid (APIENTRYP qglVertexAttrib4ivARB) (GLuint index, GLint *v);
GLvoid (APIENTRYP qglVertexAttrib4bvARB) (GLuint index, GLbyte *v);
GLvoid (APIENTRYP qglVertexAttrib4ubvARB) (GLuint index, GLubyte *v);
GLvoid (APIENTRYP qglVertexAttrib4usvARB) (GLuint index, GLushort *v);
GLvoid (APIENTRYP qglVertexAttrib4uivARB) (GLuint index, GLuint *v);
GLvoid (APIENTRYP qglVertexAttrib4NbvARB) (GLuint index, const GLbyte *v);
GLvoid (APIENTRYP qglVertexAttrib4NsvARB) (GLuint index, const GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib4NivARB) (GLuint index, const GLint *v);
GLvoid (APIENTRYP qglVertexAttrib4NubvARB) (GLuint index, const GLubyte *v);
GLvoid (APIENTRYP qglVertexAttrib4NusvARB) (GLuint index, const GLushort *v);
GLvoid (APIENTRYP qglVertexAttrib4NuivARB) (GLuint index, const GLuint *v);
GLvoid (APIENTRYP qglVertexAttribPointerARB) (GLuint index, GLint size, GLenum type, GLboolean normalized,
					      GLsizei stride, const GLvoid *pointer);
GLvoid (APIENTRYP qglEnableVertexAttribArrayARB) (GLuint index);
GLvoid (APIENTRYP qglDisableVertexAttribArrayARB) (GLuint index);
GLvoid (APIENTRYP qglBindAttribLocationARB) (GLhandleARB programObj, GLuint index, const GLcharARB *name);
GLvoid (APIENTRYP qglGetActiveAttribARB) (GLhandleARB programObj, GLuint index, GLsizei maxLength,
					  GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
GLint(APIENTRYP qglGetAttribLocationARB) (GLhandleARB programObj, const GLcharARB * name);
GLvoid (APIENTRYP qglGetVertexAttribdvARB) (GLuint index, GLenum pname, GLdouble *params);
GLvoid (APIENTRYP qglGetVertexAttribfvARB) (GLuint index, GLenum pname, GLfloat *params);
GLvoid (APIENTRYP qglGetVertexAttribivARB) (GLuint index, GLenum pname, GLint *params);
GLvoid (APIENTRYP qglGetVertexAttribPointervARB) (GLuint index, GLenum pname, GLvoid **pointer);
/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
	IN_Shutdown();

	SDL_QuitSubSystem( SDL_INIT_VIDEO );
	screen = NULL;

	Com_Memset( &glConfig, 0, sizeof( glConfig ) );
	Com_Memset( &glState, 0, sizeof( glState ) );
}

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize(void)
{
	SDL_WM_IconifyWindow();
}


/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( char *comment )
{
}

/*
===============
GLimp_CompareModes
===============
*/
static int GLimp_CompareModes( const void *a, const void *b )
{
	const float ASPECT_EPSILON = 0.001f;
	SDL_Rect *modeA = *(SDL_Rect **)a;
	SDL_Rect *modeB = *(SDL_Rect **)b;
	float aspectA = (float)modeA->w / (float)modeA->h;
	float aspectB = (float)modeB->w / (float)modeB->h;
	int areaA = modeA->w * modeA->h;
	int areaB = modeB->w * modeB->h;
	float aspectDiffA = fabs( aspectA - displayAspect );
	float aspectDiffB = fabs( aspectB - displayAspect );
	float aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if( aspectDiffsDiff > ASPECT_EPSILON )
		return 1;
	else if( aspectDiffsDiff < -ASPECT_EPSILON )
		return -1;
	else
		return areaA - areaB;
}


/*
===============
GLimp_DetectAvailableModes
===============
*/
static void GLimp_DetectAvailableModes(void)
{
	char buf[ MAX_STRING_CHARS ] = { 0 };
	SDL_Rect **modes;
	int numModes;
	int i;

	modes = SDL_ListModes( videoInfo->vfmt, SDL_OPENGL | SDL_FULLSCREEN );

	if( !modes )
	{
		ri.Printf( PRINT_WARNING, "Can't get list of available modes\n" );
		return;
	}

	if( modes == (SDL_Rect **)-1 )
	{
		ri.Printf( PRINT_ALL, "Display supports any resolution\n" );
		return; // can set any resolution
	}

	for( numModes = 0; modes[ numModes ]; numModes++ );

	if( numModes > 1 )
		qsort( modes, numModes, sizeof( SDL_Rect* ), GLimp_CompareModes );

	for( i = 0; i < numModes; i++ )
	{
		const char *newModeString = va( "%ux%u ", modes[ i ]->w, modes[ i ]->h );

		if( strlen( newModeString ) < (int)sizeof( buf ) - strlen( buf ) )
			Q_strcat( buf, sizeof( buf ), newModeString );
		else
			ri.Printf( PRINT_WARNING, "Skipping mode %ux%x, buffer too small\n", modes[i]->w, modes[i]->h );
	}

	if( *buf )
	{
		buf[ strlen( buf ) - 1 ] = 0;
		ri.Printf( PRINT_ALL, "Available modes: '%s'\n", buf );
		ri.Cvar_Set( "r_availableModes", buf );
	}
}

/*
===============
GLimp_SetMode
===============
*/
static int GLimp_SetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	const char*   glstring;
	int sdlcolorbits;
	int colorbits, depthbits, stencilbits;
	int tcolorbits, tdepthbits, tstencilbits;
	int samples;
	int i = 0;
	SDL_Surface *vidscreen = NULL;
	Uint32 flags = SDL_OPENGL;

	ri.Printf( PRINT_ALL, "Initializing OpenGL display\n");

	if ( r_allowResize->integer )
		flags |= SDL_RESIZABLE;

	if( videoInfo == NULL )
	{
		static SDL_VideoInfo sVideoInfo;
		static SDL_PixelFormat sPixelFormat;

		videoInfo = SDL_GetVideoInfo( );

		// Take a copy of the videoInfo
		Com_Memcpy( &sPixelFormat, videoInfo->vfmt, sizeof( SDL_PixelFormat ) );
		sPixelFormat.palette = NULL; // Should already be the case
		Com_Memcpy( &sVideoInfo, videoInfo, sizeof( SDL_VideoInfo ) );
		sVideoInfo.vfmt = &sPixelFormat;
		videoInfo = &sVideoInfo;

		if( videoInfo->current_h > 0 )
		{
			// Guess the display aspect ratio through the desktop resolution
			// by assuming (relatively safely) that it is set at or close to
			// the display's native aspect ratio
			displayAspect = (float)videoInfo->current_w / (float)videoInfo->current_h;

			ri.Printf( PRINT_ALL, "Estimated display aspect: %.3f\n", displayAspect );
		}
		else
		{
			ri.Printf( PRINT_ALL,
					"Cannot estimate display aspect, assuming 1.333\n" );
		}
	}

	ri.Printf (PRINT_ALL, "...setting mode %d:", mode );

	if ( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )
	{
		ri.Printf( PRINT_ALL, " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	ri.Printf( PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight);

	if (fullscreen)
	{
		flags |= SDL_FULLSCREEN;
		glConfig.isFullscreen = qtrue;
	}
	else
	{
		if (noborder)
			flags |= SDL_NOFRAME;

		glConfig.isFullscreen = qfalse;
	}

	colorbits = r_colorbits->value;
	if ((!colorbits) || (colorbits >= 32))
		colorbits = 24;

	if (!r_depthbits->value)
		depthbits = 24;
	else
		depthbits = r_depthbits->value;
	stencilbits = r_stencilbits->value;
	samples = r_ext_multisample->value;

	for (i = 0; i < 16; i++)
	{
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
				case 2 :
					if (colorbits == 24)
						colorbits = 16;
					break;
				case 1 :
					if (depthbits == 24)
						depthbits = 16;
					else if (depthbits == 16)
						depthbits = 8;
				case 3 :
					if (stencilbits == 24)
						stencilbits = 16;
					else if (stencilbits == 16)
						stencilbits = 8;
			}
		}

		tcolorbits = colorbits;
		tdepthbits = depthbits;
		tstencilbits = stencilbits;

		if ((i % 4) == 3)
		{ // reduce colorbits
			if (tcolorbits == 24)
				tcolorbits = 16;
		}

		if ((i % 4) == 2)
		{ // reduce depthbits
			if (tdepthbits == 24)
				tdepthbits = 16;
			else if (tdepthbits == 16)
				tdepthbits = 8;
		}

		if ((i % 4) == 1)
		{ // reduce stencilbits
			if (tstencilbits == 24)
				tstencilbits = 16;
			else if (tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		sdlcolorbits = 4;
		if (tcolorbits == 24)
			sdlcolorbits = 8;

#ifdef __sgi /* Fix for SGIs grabbing too many bits of color */
		if (sdlcolorbits == 4)
			sdlcolorbits = 0; /* Use minimum size for 16-bit color */

		/* Need alpha or else SGIs choose 36+ bit RGB mode */
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 1);
#endif

		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, tdepthbits );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, tstencilbits );

		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, samples );

		if(r_stereoEnabled->integer)
		{
			glConfig.stereoEnabled = qtrue;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
		}
		else
		{
			glConfig.stereoEnabled = qfalse;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
		}
		
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

#if 0 // See http://bugzilla.icculus.org/show_bug.cgi?id=3526
		// If not allowing software GL, demand accelerated
		if( !r_allowSoftwareGL->integer )
		{
			if( SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 ) < 0 )
			{
				ri.Printf( PRINT_ALL, "Unable to guarantee accelerated "
						"visual with libSDL < 1.2.10\n" );
			}
		}
#endif

		if( SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, r_swapInterval->integer ) < 0 )
			ri.Printf( PRINT_ALL, "r_swapInterval requires libSDL >= 1.2.10\n" );

#ifdef USE_ICON
		{
			SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(
					(void *)CLIENT_WINDOW_ICON.pixel_data,
					CLIENT_WINDOW_ICON.width,
					CLIENT_WINDOW_ICON.height,
					CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
					CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
					0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
					0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
					);

			SDL_WM_SetIcon( icon, NULL );
			SDL_FreeSurface( icon );
		}
#endif

		SDL_WM_SetCaption(CLIENT_WINDOW_TITLE, CLIENT_WINDOW_MIN_TITLE);
		SDL_ShowCursor(0);

		if (!(vidscreen = SDL_SetVideoMode(glConfig.vidWidth, glConfig.vidHeight, colorbits, flags)))
		{
			ri.Printf( PRINT_DEVELOPER, "SDL_SetVideoMode failed: %s\n", SDL_GetError( ) );
			continue;
		}

		opengl_context = GLimp_GetCurrentContext();

		ri.Printf( PRINT_ALL, "Using %d/%d/%d Color bits, %d depth, %d stencil display.\n",
				sdlcolorbits, sdlcolorbits, sdlcolorbits, tdepthbits, tstencilbits);

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;
		break;
	}

	GLimp_DetectAvailableModes();

	if (!vidscreen)
	{
		ri.Printf( PRINT_ALL, "Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	screen = vidscreen;

	glstring = (char *) qglGetString (GL_RENDERER);
	ri.Printf( PRINT_ALL, "GL_RENDERER: %s\n", glstring );

	return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	rserr_t err;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		char driverName[ 64 ];

		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			ri.Printf( PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n",
					SDL_GetError());
			return qfalse;
		}

		SDL_VideoDriverName( driverName, sizeof( driverName ) - 1 );
		ri.Printf( PRINT_ALL, "SDL using driver \"%s\"\n", driverName );
		Cvar_Set( "r_sdlDriver", driverName );
	}

	if (fullscreen && Cvar_VariableIntegerValue( "in_nograb" ) )
	{
		ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}
	
	err = GLimp_SetMode(mode, fullscreen, noborder);

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			ri.Printf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			ri.Printf( PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode );
			return qfalse;
		default:
			break;
	}

	return qtrue;
}

static qboolean GLimp_HaveExtension(const char *ext)
{
	const char *ptr = Q_stristr( glConfig.extensions_string, ext );
	if (ptr == NULL)
		return qfalse;
	ptr += strlen(ext);
	return ((*ptr == ' ') || (*ptr == '\0'));  // verify it's complete string.
}


/*
===============
GLimp_InitExtensions
===============
*/
static void GLimp_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	glConfig.textureCompression = TC_NONE;

	// GL_EXT_texture_compression_s3tc
	if ( GLimp_HaveExtension( "GL_ARB_texture_compression" ) &&
	     GLimp_HaveExtension( "GL_EXT_texture_compression_s3tc" ) )
	{
		if ( r_ext_compressed_textures->value )
		{
			glConfig.textureCompression = TC_S3TC_ARB;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n" );
	}

	// GL_S3_s3tc ... legacy extension before GL_EXT_texture_compression_s3tc.
	if (glConfig.textureCompression == TC_NONE)
	{
		if ( GLimp_HaveExtension( "GL_S3_s3tc" ) )
		{
			if ( r_ext_compressed_textures->value )
			{
				glConfig.textureCompression = TC_S3TC;
				ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
			}
			else
			{
				ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
		}
	}


	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if ( GLimp_HaveExtension( "EXT_texture_env_add" ) )
	{
		if ( r_ext_texture_env_add->integer )
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		}
		else
		{
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if ( GLimp_HaveExtension( "GL_ARB_multitexture" ) )
	{
		if ( r_ext_multitexture->value )
		{
			qglMultiTexCoord2fARB = SDL_GL_GetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = SDL_GL_GetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = SDL_GL_GetProcAddress( "glClientActiveTextureARB" );

			if ( qglActiveTextureARB )
			{
				GLint glint = 0;
				qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glint );
				glConfig.numTextureUnits = (int) glint;
				if ( glConfig.numTextureUnits > 1 )
				{
					ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	if ( GLimp_HaveExtension( "GL_EXT_compiled_vertex_array" ) )
	{
		if ( r_ext_compiled_vertex_array->value )
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
			qglLockArraysEXT = ( void ( APIENTRY * )( GLint, GLint ) ) SDL_GL_GetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ( void ( APIENTRY * )( void ) ) SDL_GL_GetProcAddress( "glUnlockArraysEXT" );
			if (!qglLockArraysEXT || !qglUnlockArraysEXT)
			{
				ri.Error (ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}

	textureFilterAnisotropic = qfalse;
	if ( GLimp_HaveExtension( "GL_EXT_texture_filter_anisotropic" ) )
	{
		if ( r_ext_texture_filter_anisotropic->integer ) {
			qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&maxAnisotropy );
			if ( maxAnisotropy <= 0 ) {
				ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not properly supported!\n" );
				maxAnisotropy = 0;
			}
			else
			{
				ri.Printf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", maxAnisotropy );
				textureFilterAnisotropic = qtrue;
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
	}
	vertexShaders = qfalse;
	if ( GLimp_HaveExtension( "GL_ARB_shader_objects" )
	     && GLimp_HaveExtension( "GL_ARB_fragment_shader" )
	     && GLimp_HaveExtension( "GL_ARB_vertex_shader" )
	     && GLimp_HaveExtension( "GL_ARB_shading_language_100" ) )
	{
		if ( r_ext_vertex_shader->integer ) {
		  ri.Printf( PRINT_ALL, "...using GL_ARB_vertex_shader\n" );

		  qglDeleteObjectARB = (GLvoid (APIENTRYP)(GLhandleARB)) SDL_GL_GetProcAddress("glDeleteObjectARB");
		  qglGetHandleARB = (GLhandleARB (APIENTRYP)(GLenum)) SDL_GL_GetProcAddress("glGetHandleARB");
		  qglDetachObjectARB = (GLvoid (APIENTRYP)(GLhandleARB, GLhandleARB)) SDL_GL_GetProcAddress("glDetachObjectARB");
		  qglCreateShaderObjectARB = (GLhandleARB (APIENTRYP)(GLenum)) SDL_GL_GetProcAddress("glCreateShaderObjectARB");
		  qglShaderSourceARB = (GLvoid (APIENTRYP)(GLhandleARB, GLsizei, const GLcharARB **,
							   const GLint *)) SDL_GL_GetProcAddress("glShaderSourceARB");
		  qglCompileShaderARB = (GLvoid (APIENTRYP)(GLhandleARB)) SDL_GL_GetProcAddress("glCompileShaderARB");
		  qglCreateProgramObjectARB = (GLhandleARB (APIENTRYP)(void)) SDL_GL_GetProcAddress("glCreateProgramObjectARB");
		  qglAttachObjectARB = (GLvoid (APIENTRYP)(GLhandleARB, GLhandleARB)) SDL_GL_GetProcAddress("glAttachObjectARB");
		  qglLinkProgramARB = (GLvoid (APIENTRYP)(GLhandleARB)) SDL_GL_GetProcAddress("glLinkProgramARB");
		  qglUseProgramObjectARB = (GLvoid (APIENTRYP)(GLhandleARB)) SDL_GL_GetProcAddress("glUseProgramObjectARB");
		  qglValidateProgramARB = (GLvoid (APIENTRYP)(GLhandleARB)) SDL_GL_GetProcAddress("glValidateProgramARB");
		  qglUniform1fARB = (GLvoid (APIENTRYP)(GLint, GLfloat)) SDL_GL_GetProcAddress("glUniform1fARB");
		  qglUniform2fARB = (GLvoid (APIENTRYP)(GLint, GLfloat, GLfloat)) SDL_GL_GetProcAddress("glUniform2fARB");
		  qglUniform3fARB = (GLvoid (APIENTRYP)(GLint, GLfloat, GLfloat, GLfloat)) SDL_GL_GetProcAddress("glUniform3fARB");
		  qglUniform4fARB = (GLvoid (APIENTRYP)(GLint, GLfloat, GLfloat, GLfloat, GLfloat)) SDL_GL_GetProcAddress("glUniform4fARB");
		  qglUniform1iARB = (GLvoid (APIENTRYP)(GLint, GLint)) SDL_GL_GetProcAddress("glUniform1iARB");
		  qglUniform2iARB = (GLvoid (APIENTRYP)(GLint, GLint, GLint)) SDL_GL_GetProcAddress("glUniform2iARB");
		  qglUniform3iARB = (GLvoid (APIENTRYP)(GLint, GLint, GLint, GLint)) SDL_GL_GetProcAddress("glUniform3iARB");
		  qglUniform4iARB = (GLvoid (APIENTRYP)(GLint, GLint, GLint, GLint, GLint)) SDL_GL_GetProcAddress("glUniform4iARB");
		  qglUniform1fvARB = (GLvoid (APIENTRYP)(GLint, GLsizei, const GLfloat *)) SDL_GL_GetProcAddress("glUniform1fvARB");
		  qglUniform2fvARB = (GLvoid (APIENTRYP)(GLint, GLsizei, const GLfloat *)) SDL_GL_GetProcAddress("glUniform2fvARB");
		  qglUniform3fvARB = (GLvoid (APIENTRYP)(GLint, GLsizei, const GLfloat *)) SDL_GL_GetProcAddress("glUniform3fvARB");
		  qglUniform4fvARB = (GLvoid (APIENTRYP)(GLint, GLsizei, const GLfloat *)) SDL_GL_GetProcAddress("glUniform4fvARB");
		  qglUniform1ivARB = (GLvoid (APIENTRYP)(GLint, GLsizei, const GLint *)) SDL_GL_GetProcAddress("glUniform1ivARB");
		  qglUniform2ivARB = (GLvoid (APIENTRYP)(GLint, GLsizei, const GLint *)) SDL_GL_GetProcAddress("glUniform2ivARB");
		  qglUniform3ivARB = (GLvoid (APIENTRYP)(GLint, GLsizei, const GLint *)) SDL_GL_GetProcAddress("glUniform3ivARB");
		  qglUniform4ivARB = (GLvoid (APIENTRYP)(GLint, GLsizei, const GLint *)) SDL_GL_GetProcAddress("glUniform4ivARB");
		  qglUniformMatrix2fvARB = (GLvoid (APIENTRYP)(GLint, GLsizei, GLboolean, const GLfloat *)) SDL_GL_GetProcAddress("glUniformMatrix2fvARB");
		  qglUniformMatrix3fvARB = (GLvoid (APIENTRYP)(GLint, GLsizei, GLboolean, const GLfloat *)) SDL_GL_GetProcAddress("glUniformMatrix3fvARB");
		  qglUniformMatrix4fvARB = (GLvoid (APIENTRYP)(GLint, GLsizei, GLboolean, const GLfloat *)) SDL_GL_GetProcAddress("glUniformMatrix4fvARB");
		  qglGetObjectParameterfvARB = (GLvoid (APIENTRYP)(GLhandleARB, GLenum, GLfloat *)) SDL_GL_GetProcAddress("glGetObjectParameterfvARB");
		  qglGetObjectParameterivARB = (GLvoid (APIENTRYP)(GLhandleARB, GLenum, GLint *)) SDL_GL_GetProcAddress("glGetObjectParameterivARB");
		  qglGetInfoLogARB = (GLvoid (APIENTRYP)(GLhandleARB, GLsizei, GLsizei *, GLcharARB *)) SDL_GL_GetProcAddress("glGetInfoLogARB");
		  qglGetAttachedObjectsARB = (GLvoid (APIENTRYP)(GLhandleARB, GLsizei, GLsizei *, GLhandleARB *)) SDL_GL_GetProcAddress("glGetAttachedObjectsARB");
		  qglGetUniformLocationARB = (GLint (APIENTRYP)(GLhandleARB, const GLcharARB *)) SDL_GL_GetProcAddress("glGetUniformLocationARB");
		  qglGetActiveUniformARB = (GLvoid (APIENTRYP)(GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *)) SDL_GL_GetProcAddress("glGetActiveUniformARB");
		  qglGetUniformfvARB = (GLvoid (APIENTRYP)(GLhandleARB, GLint, GLfloat *)) SDL_GL_GetProcAddress("glGetUniformfvARB");
		  qglGetUniformivARB = (GLvoid (APIENTRYP)(GLhandleARB, GLint, GLint *)) SDL_GL_GetProcAddress("glGetUniformivARB");
		  qglGetShaderSourceARB = (GLvoid (APIENTRYP)(GLhandleARB, GLsizei, GLsizei *, GLcharARB *)) SDL_GL_GetProcAddress("glGetShaderSourceARB");

		  qglVertexAttrib1fARB = (GLvoid (APIENTRYP)(GLuint, GLfloat)) SDL_GL_GetProcAddress("glVertexAttrib1fARB");
		  qglVertexAttrib1sARB = (GLvoid (APIENTRYP)(GLuint, GLshort)) SDL_GL_GetProcAddress("glVertexAttrib1sARB");
		  qglVertexAttrib1dARB = (GLvoid (APIENTRYP)(GLuint, GLdouble)) SDL_GL_GetProcAddress("glVertexAttrib1dARB");
		  qglVertexAttrib2fARB = (GLvoid (APIENTRYP)(GLuint, GLfloat, GLfloat)) SDL_GL_GetProcAddress("glVertexAttrib2fARB");
		  qglVertexAttrib2sARB = (GLvoid (APIENTRYP)(GLuint, GLshort, GLshort)) SDL_GL_GetProcAddress("glVertexAttrib2sARB");
		  qglVertexAttrib2dARB = (GLvoid (APIENTRYP)(GLuint, GLdouble, GLdouble)) SDL_GL_GetProcAddress("glVertexAttrib2dARB");
		  qglVertexAttrib3fARB = (GLvoid (APIENTRYP)(GLuint, GLfloat, GLfloat, GLfloat)) SDL_GL_GetProcAddress("glVertexAttrib3fARB");
		  qglVertexAttrib3sARB = (GLvoid (APIENTRYP)(GLuint, GLshort, GLshort, GLshort)) SDL_GL_GetProcAddress("glVertexAttrib3sARB");
		  qglVertexAttrib3dARB = (GLvoid (APIENTRYP)(GLuint, GLdouble, GLdouble, GLdouble)) SDL_GL_GetProcAddress("glVertexAttrib3dARB");
		  qglVertexAttrib4fARB = (GLvoid (APIENTRYP)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat)) SDL_GL_GetProcAddress("glVertexAttrib4fARB");
		  qglVertexAttrib4sARB = (GLvoid (APIENTRYP)(GLuint, GLshort, GLshort, GLshort, GLshort)) SDL_GL_GetProcAddress("glVertexAttrib4sARB");
		  qglVertexAttrib4dARB = (GLvoid (APIENTRYP)(GLuint, GLdouble, GLdouble, GLdouble, GLdouble)) SDL_GL_GetProcAddress("glVertexAttrib4dARB");
		  qglVertexAttrib4NubARB = (GLvoid (APIENTRYP)(GLuint, GLubyte, GLubyte, GLubyte, GLubyte)) SDL_GL_GetProcAddress("glVertexAttrib4NubARB");
		  qglVertexAttrib1fvARB = (GLvoid (APIENTRYP)(GLuint, GLfloat *)) SDL_GL_GetProcAddress("glVertexAttrib1fvARB");
		  qglVertexAttrib1svARB = (GLvoid (APIENTRYP)(GLuint, GLshort *)) SDL_GL_GetProcAddress("glVertexAttrib1svARB");
		  qglVertexAttrib1dvARB = (GLvoid (APIENTRYP)(GLuint, GLdouble *)) SDL_GL_GetProcAddress("glVertexAttrib1dvARB");
		  qglVertexAttrib2fvARB = (GLvoid (APIENTRYP)(GLuint, GLfloat *)) SDL_GL_GetProcAddress("glVertexAttrib2fvARB");
		  qglVertexAttrib2svARB = (GLvoid (APIENTRYP)(GLuint, GLshort *)) SDL_GL_GetProcAddress("glVertexAttrib2svARB");
		  qglVertexAttrib2dvARB = (GLvoid (APIENTRYP)(GLuint, GLdouble *)) SDL_GL_GetProcAddress("glVertexAttrib2dvARB");
		  qglVertexAttrib3fvARB = (GLvoid (APIENTRYP)(GLuint, GLfloat *)) SDL_GL_GetProcAddress("glVertexAttrib3fvARB");
		  qglVertexAttrib3svARB = (GLvoid (APIENTRYP)(GLuint, GLshort *)) SDL_GL_GetProcAddress("glVertexAttrib3svARB");
		  qglVertexAttrib3dvARB = (GLvoid (APIENTRYP)(GLuint, GLdouble *)) SDL_GL_GetProcAddress("glVertexAttrib3dvARB");
		  qglVertexAttrib4fvARB = (GLvoid (APIENTRYP)(GLuint, GLfloat *)) SDL_GL_GetProcAddress("glVertexAttrib4fvARB");
		  qglVertexAttrib4svARB = (GLvoid (APIENTRYP)(GLuint, GLshort *)) SDL_GL_GetProcAddress("glVertexAttrib4svARB");
		  qglVertexAttrib4dvARB = (GLvoid (APIENTRYP)(GLuint, GLdouble *)) SDL_GL_GetProcAddress("glVertexAttrib4dvARB");
		  qglVertexAttrib4ivARB = (GLvoid (APIENTRYP)(GLuint, GLint *)) SDL_GL_GetProcAddress("glVertexAttrib4ivARB");
		  qglVertexAttrib4bvARB = (GLvoid (APIENTRYP)(GLuint, GLbyte *)) SDL_GL_GetProcAddress("glVertexAttrib4bvARB");
		  qglVertexAttrib4ubvARB = (GLvoid (APIENTRYP)(GLuint, GLubyte *)) SDL_GL_GetProcAddress("glVertexAttrib4ubvARB");
		  qglVertexAttrib4usvARB = (GLvoid (APIENTRYP)(GLuint, GLushort *)) SDL_GL_GetProcAddress("glVertexAttrib4usvARB");
		  qglVertexAttrib4uivARB = (GLvoid (APIENTRYP)(GLuint, GLuint *)) SDL_GL_GetProcAddress("glVertexAttrib4uivARB");
		  qglVertexAttrib4NbvARB = (GLvoid (APIENTRYP)(GLuint, const GLbyte *)) SDL_GL_GetProcAddress("glVertexAttrib4NbvARB");
		  qglVertexAttrib4NsvARB = (GLvoid (APIENTRYP)(GLuint, const GLshort *)) SDL_GL_GetProcAddress("glVertexAttrib4NsvARB");
		  qglVertexAttrib4NivARB = (GLvoid (APIENTRYP)(GLuint, const GLint *)) SDL_GL_GetProcAddress("glVertexAttrib4NivARB");
		  qglVertexAttrib4NubvARB = (GLvoid (APIENTRYP)(GLuint, const GLubyte *)) SDL_GL_GetProcAddress("glVertexAttrib4NubvARB");
		  qglVertexAttrib4NusvARB = (GLvoid (APIENTRYP)(GLuint, const GLushort *)) SDL_GL_GetProcAddress("glVertexAttrib4NusvARB");
		  qglVertexAttrib4NuivARB = (GLvoid (APIENTRYP)(GLuint, const GLuint *)) SDL_GL_GetProcAddress("glVertexAttrib4NuivARB");
		  qglVertexAttribPointerARB = (GLvoid (APIENTRYP)(GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *)) SDL_GL_GetProcAddress("glVertexAttribPointerARB");
		  qglEnableVertexAttribArrayARB = (GLvoid (APIENTRYP)(GLuint)) SDL_GL_GetProcAddress("glEnableVertexAttribArrayARB");
		  qglDisableVertexAttribArrayARB = (GLvoid (APIENTRYP)(GLuint)) SDL_GL_GetProcAddress("glDisableVertexAttribArrayARB");
		  qglBindAttribLocationARB = (GLvoid (APIENTRYP)(GLhandleARB, GLuint, const GLcharARB *)) SDL_GL_GetProcAddress("glBindAttribLocationARB");
		  qglGetActiveAttribARB = (GLvoid (APIENTRYP)(GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *)) SDL_GL_GetProcAddress("glGetActiveAttribARB");
		  qglGetAttribLocationARB = (GLint (APIENTRYP)(GLhandleARB, const GLcharARB *)) SDL_GL_GetProcAddress("glGetAttribLocationARB");
		  qglGetVertexAttribdvARB = (GLvoid (APIENTRYP)(GLuint, GLenum, GLdouble *)) SDL_GL_GetProcAddress("glGetVertexAttribdvARB");
		  qglGetVertexAttribfvARB = (GLvoid (APIENTRYP)(GLuint, GLenum, GLfloat *)) SDL_GL_GetProcAddress("glGetVertexAttribfvARB");
		  qglGetVertexAttribivARB = (GLvoid (APIENTRYP)(GLuint, GLenum, GLint *)) SDL_GL_GetProcAddress("glGetVertexAAttribivARB");
		  qglGetVertexAttribPointervARB = (GLvoid (APIENTRYP)(GLuint, GLenum, GLvoid **)) SDL_GL_GetProcAddress("glGetVertexAttribPointervARB");
		  vertexShaders = qtrue;
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_vertex_shader\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_ARB_vertex_shader not found\n" );
	}
}

#define R_MODE_FALLBACK 3 // 640 * 480

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( void )
{
	r_allowSoftwareGL = ri.Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	r_sdlDriver = ri.Cvar_Get( "r_sdlDriver", "", CVAR_ROM );
	r_allowResize = ri.Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE );
	r_centerWindow = ri.Cvar_Get( "r_centerWindow", "0", CVAR_ARCHIVE );

	if( Cvar_VariableIntegerValue( "com_abnormalExit" ) )
	{
		ri.Cvar_Set( "r_mode", va( "%d", R_MODE_FALLBACK ) );
		ri.Cvar_Set( "r_fullscreen", "0" );
		ri.Cvar_Set( "r_centerWindow", "0" );
		ri.Cvar_Set( "com_abnormalExit", "0" );
		ri.Cvar_Set( "r_postprocess", "none");
		ri.Cvar_Set( "r_ext_vertex_shader", "0");
	}

	Sys_SetEnv( "SDL_VIDEO_CENTERED", r_centerWindow->integer ? "1" : "" );

	Sys_GLimpInit( );

	// Create the window and set up the context
	if(GLimp_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer, r_noborder->integer))
		goto success;

	// Try again, this time in a platform specific "safe mode"
	Sys_GLimpSafeInit( );

	if(GLimp_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer, qfalse))
		goto success;

	// Finally, try the default screen resolution
	if( r_mode->integer != R_MODE_FALLBACK )
	{
		ri.Printf( PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n",
				r_mode->integer, R_MODE_FALLBACK );

		if(GLimp_StartDriverAndSetMode(R_MODE_FALLBACK, qfalse, qfalse))
			goto success;
	}

	// Nothing worked, give up
	ri.Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem\n" );

success:
	// This values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;
	glConfig.deviceSupportsGamma = SDL_SetGamma( 1.0f, 1.0f, 1.0f ) >= 0;

	// Mysteriously, if you use an NVidia graphics card and multiple monitors,
	// SDL_SetGamma will incorrectly return false... the first time; ask
	// again and you get the correct answer. This is a suspected driver bug, see
	// http://bugzilla.icculus.org/show_bug.cgi?id=4316
	glConfig.deviceSupportsGamma = SDL_SetGamma( 1.0f, 1.0f, 1.0f ) >= 0;

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (char *) qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (char *) qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	if (*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n')
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
	Q_strncpyz( glConfig.version_string, (char *) qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string, (char *) qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	// initialize extensions
	GLimp_InitExtensions( );

	ri.Cvar_Get( "r_availableModes", "", CVAR_ROM );

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init( );
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
	// don't flip if drawing to front buffer
	if ( Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 )
	{
		SDL_GL_SwapBuffers();
	}

	if( r_fullscreen->modified )
	{
		qboolean    fullscreen;
		qboolean    needToToggle = qtrue;
		qboolean    sdlToggled = qfalse;
		SDL_Surface *s = SDL_GetVideoSurface( );

		if( s )
		{
			// Find out the current state
			fullscreen = !!( s->flags & SDL_FULLSCREEN );
				
			if( r_fullscreen->integer && Cvar_VariableIntegerValue( "in_nograb" ) )
			{
				ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
				ri.Cvar_Set( "r_fullscreen", "0" );
				r_fullscreen->modified = qfalse;
			}

			// Is the state we want different from the current state?
			needToToggle = !!r_fullscreen->integer != fullscreen;

			if( needToToggle )
				sdlToggled = SDL_WM_ToggleFullScreen( s );
		}

		if( needToToggle )
		{
			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if( !sdlToggled )
				Cbuf_AddText( "vid_restart" );

			IN_Restart( );
		}

		r_fullscreen->modified = qfalse;
	}
}



#ifdef SMP
/*
===========================================================

SMP acceleration

===========================================================
*/

/*
 * I have no idea if this will even work...most platforms don't offer
 * thread-safe OpenGL libraries, and it looks like the original Linux
 * code counted on each thread claiming the GL context with glXMakeCurrent(),
 * which you can't currently do in SDL. We'll just have to hope for the best.
 */

static SDL_mutex *smpMutex = NULL;
static SDL_cond *renderCommandsEvent = NULL;
static SDL_cond *renderCompletedEvent = NULL;
static void (*glimpRenderThread)( void ) = NULL;
static SDL_Thread *renderThread = NULL;

/*
===============
GLimp_ShutdownRenderThread
===============
*/
static void GLimp_ShutdownRenderThread(void)
{
	if (smpMutex != NULL)
	{
		SDL_DestroyMutex(smpMutex);
		smpMutex = NULL;
	}

	if (renderCommandsEvent != NULL)
	{
		SDL_DestroyCond(renderCommandsEvent);
		renderCommandsEvent = NULL;
	}

	if (renderCompletedEvent != NULL)
	{
		SDL_DestroyCond(renderCompletedEvent);
		renderCompletedEvent = NULL;
	}

	glimpRenderThread = NULL;
}

/*
===============
GLimp_RenderThreadWrapper
===============
*/
static int GLimp_RenderThreadWrapper( void *arg )
{
	Com_Printf( "Render thread starting\n" );

	glimpRenderThread();

	GLimp_SetCurrentContext(NULL);

	Com_Printf( "Render thread terminating\n" );

	return 0;
}

/*
===============
GLimp_SpawnRenderThread
===============
*/
qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
	static qboolean warned = qfalse;
	if (!warned)
	{
		Com_Printf("WARNING: You enable r_smp at your own risk!\n");
		warned = qtrue;
	}

#ifndef MACOS_X
	return qfalse;  /* better safe than sorry for now. */
#endif

	if (renderThread != NULL)  /* hopefully just a zombie at this point... */
	{
		Com_Printf("Already a render thread? Trying to clean it up...\n");
		SDL_WaitThread(renderThread, NULL);
		renderThread = NULL;
		GLimp_ShutdownRenderThread();
	}

	smpMutex = SDL_CreateMutex();
	if (smpMutex == NULL)
	{
		Com_Printf( "smpMutex creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderCommandsEvent = SDL_CreateCond();
	if (renderCommandsEvent == NULL)
	{
		Com_Printf( "renderCommandsEvent creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderCompletedEvent = SDL_CreateCond();
	if (renderCompletedEvent == NULL)
	{
		Com_Printf( "renderCompletedEvent creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	glimpRenderThread = function;
	renderThread = SDL_CreateThread(GLimp_RenderThreadWrapper, NULL);
	if ( renderThread == NULL )
	{
		ri.Printf( PRINT_ALL, "SDL_CreateThread() returned %s", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}
	else
	{
		// tma 01/09/07: don't think this is necessary anyway?
		//
		// !!! FIXME: No detach API available in SDL!
		//ret = pthread_detach( renderThread );
		//if ( ret ) {
		//ri.Printf( PRINT_ALL, "pthread_detach returned %d: %s", ret, strerror( ret ) );
		//}
	}

	return qtrue;
}

static volatile void    *smpData = NULL;
static volatile qboolean smpDataReady;

/*
===============
GLimp_RendererSleep
===============
*/
void *GLimp_RendererSleep( void )
{
	void  *data = NULL;

	GLimp_SetCurrentContext(NULL);

	SDL_LockMutex(smpMutex);
	{
		smpData = NULL;
		smpDataReady = qfalse;

		// after this, the front end can exit GLimp_FrontEndSleep
		SDL_CondSignal(renderCompletedEvent);

		while ( !smpDataReady )
			SDL_CondWait(renderCommandsEvent, smpMutex);

		data = (void *)smpData;
	}
	SDL_UnlockMutex(smpMutex);

	GLimp_SetCurrentContext(opengl_context);

	return data;
}

/*
===============
GLimp_FrontEndSleep
===============
*/
void GLimp_FrontEndSleep( void )
{
	SDL_LockMutex(smpMutex);
	{
		while ( smpData )
			SDL_CondWait(renderCompletedEvent, smpMutex);
	}
	SDL_UnlockMutex(smpMutex);

	GLimp_SetCurrentContext(opengl_context);
}

/*
===============
GLimp_WakeRenderer
===============
*/
void GLimp_WakeRenderer( void *data )
{
	GLimp_SetCurrentContext(NULL);

	SDL_LockMutex(smpMutex);
	{
		assert( smpData == NULL );
		smpData = data;
		smpDataReady = qtrue;

		// after this, the renderer can continue through GLimp_RendererSleep
		SDL_CondSignal(renderCommandsEvent);
	}
	SDL_UnlockMutex(smpMutex);
}

#else

// No SMP - stubs
void GLimp_RenderThreadWrapper( void *arg )
{
}

qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
	ri.Printf( PRINT_WARNING, "ERROR: SMP support was disabled at compile time\n");
	return qfalse;
}

void *GLimp_RendererSleep( void )
{
	return NULL;
}

void GLimp_FrontEndSleep( void )
{
}

void GLimp_WakeRenderer( void *data )
{
}

#endif
