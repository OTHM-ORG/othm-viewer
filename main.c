#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include "flownetwork.h"

static void useCSS(gchar *cssFile)
{
  GtkCssProvider *provider;
  GdkDisplay *display;
  GdkScreen *screen;
  provider = gtk_css_provider_new ();
  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  gtk_style_context_add_provider_for_screen (screen,
					     GTK_STYLE_PROVIDER (provider),
					     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gsize bytes_written, bytes_read;

  GError *error = 0;

  gtk_css_provider_load_from_path(provider,
				  g_filename_to_utf8(cssFile, strlen(cssFile), &bytes_read,
						     &bytes_written, &error),
				  NULL);
  g_object_unref (provider);
}

void helloWorld(GtkWidget *widget,
		gpointer   data )
{
  g_print("Hello World!\n");
}

GtkWidget *add_node(FlowNetwork *fn, gpointer data)
{
  GtkWidget *eventbox = gtk_event_box_new();
  gtk_widget_set_size_request(GTK_WIDGET(eventbox), 128, 128);
  gtk_widget_set_name(eventbox, "test");
  g_signal_connect (G_OBJECT (eventbox), "button_press_event",
  		      G_CALLBACK (helloWorld), (gpointer) "cool button");
  flowNetwork_drag_node_implementaion(GTK_WIDGET(eventbox));
  return eventbox;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *fn;
  GtkWidget *frame;
  GtkWidget *grid;
  GtkWidget *grid2;
  GtkWidget *button;
  GtkWidget *scrolled;

  gtk_init(&argc, &argv);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (window), "Aspect Frame");

  g_signal_connect (window, "destroy",
                    G_CALLBACK (exit), NULL);

  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  fn = flowNetwork_new();

  scrolled = gtk_scrolled_window_new(NULL, NULL);
  grid2 = gtk_grid_new();

  grid = gtk_grid_new();
  gtk_widget_set_size_request(grid, 500, 500);
  button = gtk_button_new_with_label("hi!");
  gtk_widget_set_name (GTK_WIDGET(button), "testButton");
  gtk_container_add(GTK_CONTAINER(grid), button);


  flowNetwork_drag_node_source(button, add_node, NULL, NULL);


  frame = gtk_frame_new (NULL);
  gtk_widget_set_size_request(frame, 920, 500);

  flowNetwork_account_scroller(FLOWNETWORK(fn), GTK_SCROLLED_WINDOW(scrolled));
  
  gtk_container_add(GTK_CONTAINER(frame), fn);
  gtk_container_add(GTK_CONTAINER(grid2), grid);
  gtk_grid_attach_next_to(GTK_GRID(grid2), frame, grid,
			  GTK_POS_BOTTOM, 1, 1);
  gtk_container_add (GTK_CONTAINER (scrolled), grid2);
  gtk_container_add (GTK_CONTAINER (window), scrolled);
  useCSS("test.css");
  gtk_widget_show_all (window);
  
  gtk_main ();
}
