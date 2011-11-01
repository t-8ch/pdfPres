/*
	Copyright 2011 Peter Hofmann
	Copyright (C) 2005, Red Hat, Inc.

	This file is part of pdfpres.

	pdfpres is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	pdfpres is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pdfpres. If not, see <http://www.gnu.org/licenses/>.
*/


/* This is a temporary workaround. GDK-API has been removed from poppler
 * in poppler 0.17 (commit 149b7fe) but pdfpres relies on that API. So
 * for now, I just copied the old poppler code. Of course, this
 * workaround is likely to break.
 *
 * Unfortunately, I'm running out of free time. If you'd like to help me
 * improve pdfpres, please send me patches and/or fork the project on
 * GitHub. I'd very much appreciate it! :-)
 */


#include <glib/poppler.h>
#if POPPLER_MINOR_VERSION > 16

#include <gdk/gdk.h>
#include <glib.h>
#include <stdbool.h>

#include "popplergdk.h"

static void
copy_cairo_surface_to_pixbuf (cairo_surface_t *surface,
			      GdkPixbuf       *pixbuf)
{
  int cairo_width, cairo_height, cairo_rowstride;
  unsigned char *pixbuf_data, *dst, *cairo_data;
  int pixbuf_rowstride, pixbuf_n_channels;
  unsigned int *src;
  int x, y;

  cairo_width = cairo_image_surface_get_width (surface);
  cairo_height = cairo_image_surface_get_height (surface);
  cairo_rowstride = cairo_image_surface_get_stride (surface);
  cairo_data = cairo_image_surface_get_data (surface);

  pixbuf_data = gdk_pixbuf_get_pixels (pixbuf);
  pixbuf_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixbuf_n_channels = gdk_pixbuf_get_n_channels (pixbuf);

  if (cairo_width > gdk_pixbuf_get_width (pixbuf))
    cairo_width = gdk_pixbuf_get_width (pixbuf);
  if (cairo_height > gdk_pixbuf_get_height (pixbuf))
    cairo_height = gdk_pixbuf_get_height (pixbuf);
  for (y = 0; y < cairo_height; y++)
    {
      src = (unsigned int *) (cairo_data + y * cairo_rowstride);
      dst = pixbuf_data + y * pixbuf_rowstride;
      for (x = 0; x < cairo_width; x++) 
	{
	  dst[0] = (*src >> 16) & 0xff;
	  dst[1] = (*src >> 8) & 0xff; 
	  dst[2] = (*src >> 0) & 0xff;
	  if (pixbuf_n_channels == 4)
	      dst[3] = (*src >> 24) & 0xff;
	  dst += pixbuf_n_channels;
	  src++;
	}
    }
}	

static void
_poppler_page_render_to_pixbuf (PopplerPage *page,
				int src_x, int src_y,
				int src_width, int src_height,
				double scale,
				int rotation,
				bool printing,
				GdkPixbuf *pixbuf)
{
  cairo_t *cr;
  cairo_surface_t *surface;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					src_width, src_height);
  cr = cairo_create (surface);
  cairo_save (cr);
  switch (rotation) {
  case 90:
	  cairo_translate (cr, src_x + src_width, -src_y);
	  break;
  case 180:
	  cairo_translate (cr, src_x + src_width, src_y + src_height);
	  break;
  case 270:
	  cairo_translate (cr, -src_x, src_y + src_height);
	  break;
  default:
	  cairo_translate (cr, -src_x, -src_y);
  }

  if (scale != 1.0)
	  cairo_scale (cr, scale, scale);

  if (rotation != 0)
	  cairo_rotate (cr, rotation * G_PI / 180.0);

  if (printing)
	  poppler_page_render_for_printing (page, cr);
  else
	  poppler_page_render (page, cr);
  cairo_restore (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
  cairo_set_source_rgb (cr, 1., 1., 1.);
  cairo_paint (cr);

  cairo_destroy (cr);

  copy_cairo_surface_to_pixbuf (surface, pixbuf);
  cairo_surface_destroy (surface);
}

void
poppler_page_render_to_pixbuf (PopplerPage *page,
			       int src_x, int src_y,
			       int src_width, int src_height,
			       double scale,
			       int rotation,
			       GdkPixbuf *pixbuf)
{
  g_return_if_fail (POPPLER_IS_PAGE (page));
  g_return_if_fail (scale > 0.0);
  g_return_if_fail (pixbuf != NULL);

  _poppler_page_render_to_pixbuf (page, src_x, src_y,
				  src_width, src_height,
				  scale, rotation,
				  false,
				  pixbuf);
}

#endif /* POPPLER_MINOR_VERSION */
