/*
	Copyright 2009, 2010 Peter Hofmann

	This file is part of pdfPres.

	pdfPres is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	pdfPres is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pdfPres. If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <libxml/xmlwriter.h>

#include "notes.h"
#include "pdfPres.h"


static gchar **notes = NULL;


void initNotes(void)
{
	int i;

	/* if there were some notes before, kill em. */
	if (notes != NULL)
		g_strfreev(notes);

	/* prepare notes array -- this can be free'd with g_strfreev */
	notes = (gchar **)malloc((doc_n_pages + 1) * sizeof(gchar *));
	for (i = 0; i < doc_n_pages; i++)
	{
		notes[i] = g_strdup("");
	}
	notes[doc_n_pages] = NULL;
}

void printNote(int slideNum)
{
	if (notes == NULL)
	{
		return;
	}

	slideNum--;
	if (slideNum < 0 || slideNum >= doc_n_pages)
	{
		return;
	}

	/* push text into buffer */
	gtk_text_buffer_set_text(noteBuffer, notes[slideNum],
			strlen(notes[slideNum]));
}

void readNotes(char *filename)
{
	char *databuf = NULL, *onlyText = NULL;
	gchar **splitNotes = NULL;
	struct stat statbuf;
	FILE *fp = NULL;
	int thatSlide = -1, i = 0, splitAt = 0;
	GtkWidget *dialog = NULL;

	/* try to load the file */
	if (stat(filename, &statbuf) == -1)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"Could not stat file: %s.",
				g_strerror(errno));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	/* allocate one additional byte so that we can store a null
	 * terminator. */
	databuf = (char *)malloc(statbuf.st_size + 1);
	dieOnNull(databuf, __LINE__);

	fp = fopen(filename, "r");
	if (!fp)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"Could not open file for reading: %s.",
				g_strerror(errno));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		free(databuf);
		return;
	}

	if (fread(databuf, 1, statbuf.st_size, fp) != statbuf.st_size)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"Unexpected end of file.");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		free(databuf);
		fclose(fp);
		return;
	}

	fclose(fp);

	/* terminate the string. otherwise, g_strsplit won't be able to
	 * determine where it ends. */
	databuf[statbuf.st_size] = 0;

	/* init notes with empty string */
	initNotes();

	/* split notes, parse slide numbers and replace entries */
	/* TODO: Spit out a warning when there are more notes than slides */
	/* TODO: Use sth. like libyaml or libxml2 to read the notes */
	splitNotes = g_strsplit(databuf, "-- ", 0);
	for (i = 0; i < doc_n_pages; i++)
	{
		for (splitAt = 0; splitAt < g_strv_length(splitNotes); splitAt++)
		{
			thatSlide = -1;
			sscanf(splitNotes[splitAt], "%d\n", &thatSlide);
			if (thatSlide == (i + 1))
			{
				/* skip slide number and line break */
				/* FIXME: I bet there's a better way to do this. */
				onlyText = strstr(splitNotes[splitAt], "\n");

				if (onlyText != NULL)
				{
					/* if onlyText is NOT null, it'll point to the line
					 * break. that means we're safe to advance one
					 * character -- in the worst case, we'll end up on
					 * the null terminator.
					 *
					 * FIXME: I still bet there's a better way.
					 */
					onlyText += sizeof(char);

					/* replace note text */
					if (notes[i] != NULL)
						g_free(notes[i]);

					notes[i] = g_strdup(onlyText);

					/* quit inner for() */
					break;
				}
			}
		}
	}
	g_strfreev(splitNotes);

	/* print current note */
	printNote(doc_page + 1);

	/* that buffer isn't needed anymore: */
	free(databuf);
}

void saveNotes(char *uri)
{
	int i, rc;
	GtkWidget *dialog = NULL;
	xmlTextWriterPtr writer;

	/* Create a new XmlWriter for uri, with no compression. */
	writer = xmlNewTextWriterFilename(uri, 0);
	if (writer == NULL)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Error creating the xml writer.");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlCleanupParser();
		return;
	}

	/* Start the document with the xml default for the version, encoding
	 * UTF-8 and the default for the standalone declaration. */
	rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
	if (rc < 0)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Could not start document. %d.", rc);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return;
	}

	/* Start root element. It's okay to use "BAD_CAST" since there
	 * are no non-ascii letters. */
	rc = xmlTextWriterStartElement(writer, BAD_CAST "notes");
	if (rc < 0)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Could not start element \"notes\". %d.", rc);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return;
	}

	/* Save all notes which do exist. Leave out empty slides. */
	for (i = 0; i < doc_n_pages; i++)
	{
		if (notes[i] != NULL && g_strcmp0("", notes[i]) != 0)
		{
			/* Start of "slide" element. */
			rc = xmlTextWriterStartElement(writer, BAD_CAST "slide");
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not start element \"slide\". %d.",
						rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}

			/* Write page number as attribute. */
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "number", "%d", (i + 1));
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not write attribute \"number\""
						" for slide %d. %d.", (i + 1), rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}

			/* Write note as element content. */
			rc = xmlTextWriterWriteFormatString(writer,
					"%s", notes[i]);
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not write string"
						" for slide %d. %d.", (i + 1), rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}

			/* End of "slide" element. */
			rc = xmlTextWriterEndElement(writer);
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not end element \"slide\". %d.",
						rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}
		}
	}

	/* Here we could close open elements using the function
	 * xmlTextWriterEndElement, but since we do not want to write any
	 * other elements, we simply call xmlTextWriterEndDocument, which
	 * will do all the work. */
	rc = xmlTextWriterEndDocument(writer);
	if (rc < 0)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Could not end document. %d.", rc);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return;
	}

	xmlFreeTextWriter(writer);
	xmlCleanupParser();

	/* Report success. */
	dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"Notes saved.");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void saveCurrentNote(void)
{
	GtkTextIter start, end;
	gchar *content = NULL;

	/* get text from the buffer */
	gtk_text_buffer_get_bounds(noteBuffer, &start, &end);
	content = gtk_text_buffer_get_text(noteBuffer, &start, &end, FALSE);

	/* replace previous content */
	if (notes[doc_page] != NULL)
		g_free(notes[doc_page]);

	notes[doc_page] = content;
}