#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "flownetwork.h"

#define _POINTER (sizeof(struct fnWidgetGen *) * 8)

struct fnWidgetGen {
	GtkWidget *(*add_widget) (FlowNetwork *fn, gpointer data);
	gpointer data;
};

GHashTable *fnWidgetGenTable = NULL;
GHashTable *fnWidgetDragTable = NULL;

enum { TARGET_POINTER };

static GtkTargetEntry target_list[] = {
	{"POINTER", 0, TARGET_POINTER},
};

static guint n_targets = G_N_ELEMENTS(target_list);

void flowNetwork_drag_node_source(GtkWidget *srcNode,
				  GtkWidget *(*add_widget) (FlowNetwork *fn,
							    gpointer data),
				  gpointer data, GtkGrid * nodeGrid);
void flowNetwork_drag_node_implementaion(GtkWidget *n);
static void flowNetwork_drop_init(FlowNetwork *fn);
static void node_drag_begin_handle(GtkWidget *widget,
				   GdkDragContext * context, gpointer user_data);
static void network_data_get_handle(GtkWidget *widget, GdkDragContext *context,
				    GtkSelectionData *selection_data, guint target_type,
				    guint time, gpointer user_data);
static gboolean network_drop_handle(GtkWidget *widget, GdkDragContext *context,
				    gint x, gint y, guint time, gpointer user_data);
static void network_data_received_handle(GtkWidget *widget, GdkDragContext *context,
					 gint x, gint y, GtkSelectionData *selection_data,
					 guint target_type, guint time, gpointer data);
static void node_drag_end_handle(GtkWidget *widget, GdkDragContext * context, gpointer user_data);
static void draw_connections(GtkWidget *widget, cairo_t *cr);
static gboolean draw_cb(GtkWidget *widget, cairo_t * cr, gpointer data);
static GtkWidget *return_widget(FlowNetwork *fn, gpointer data);
static void scroller_offset_x(gpointer scroller, void *acuval);
static void scroller_offset_y(gpointer scroller, void *acuval);
static gint scroller_offset_x_all(GtkWidget *fn);
static gint scroller_offset_y_all(GtkWidget *fn);
static gboolean container_has_child(GtkContainer *container, GtkWidget *widget);
static void resize_flowNetwork(GtkLayout *layout, GtkWidget *widget,
			       gint widget_width, gint widget_height);
static void redraw_layout(GtkLayout * layout);
static void flowNetwork_class_init(FlowNetworkClass * klass);
static void flowNetwork_init(FlowNetwork *fn);

GType flowNetwork_get_type(void)
{
	static GType fn_type = 0;

	if (!fn_type) {
		const GTypeInfo fn_info = {
			sizeof(FlowNetworkClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) flowNetwork_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof(FlowNetwork),
			0,
			(GInstanceInitFunc) flowNetwork_init,
		};

		fn_type =
		    g_type_register_static(GTK_TYPE_SCROLLED_WINDOW,
					   "FlowNetwork", &fn_info, 0);
	}

	return fn_type;
}

static void flowNetwork_class_init(FlowNetworkClass * klass)
{
	return;
}

static void flowNetwork_init(FlowNetwork *fn)
{
	fn->layout = gtk_layout_new(NULL, NULL);
	gtk_widget_set_size_request(GTK_WIDGET(fn->layout), 1000, 250);
	flowNetwork_drop_init(fn);
	g_signal_connect(fn->layout, "draw", G_CALLBACK(draw_cb), NULL);
	gtk_container_add(GTK_CONTAINER(&fn->scrolledWindow), fn->layout);
	gtk_widget_show_all(GTK_WIDGET(&fn->scrolledWindow));
	fn->scrollers = g_slist_append(fn->scrollers, &fn->scrolledWindow);
	fn->data = g_hash_table_new(NULL, NULL);
}

GtkWidget *flowNetwork_new()
{
	FlowNetwork *fn;
	fn = g_object_new(flowNetwork_get_type(), NULL);
	return GTK_WIDGET(fn);
}

void flowNetwork_account_scroller(FlowNetwork *fn, GtkScrolledWindow *gsw)
{
	fn->scrollers = g_slist_append(fn->scrollers, gsw);
}


/* Section for drag and drop */
void flowNetwork_drag_node_source(GtkWidget *srcNode,
			     GtkWidget *(*add_widget) (FlowNetwork *fn,
							gpointer data),
			     gpointer data, GtkGrid *nodeGrid)
{
	struct fnWidgetGen *wg;

	gtk_drag_source_set(srcNode,	/* widget will be drag-able */
			    GDK_BUTTON1_MASK,	/* modifier that will start a drag */
			    target_list,	/* lists of target to support */
			    n_targets,	/* size of list */
			    GDK_ACTION_COPY);	/* what to do with data after dropped */

	g_signal_connect(srcNode, "drag-data-get",
			 G_CALLBACK(network_data_get_handle), NULL);
	g_signal_connect(srcNode, "drag-begin",
			 G_CALLBACK(node_drag_begin_handle), NULL);
	g_signal_connect(srcNode, "drag-end", G_CALLBACK(node_drag_end_handle), NULL);

	if (fnWidgetGenTable == NULL)
		fnWidgetGenTable = g_hash_table_new(NULL, NULL);

        wg = malloc(sizeof(struct fnWidgetGen));
	wg->add_widget = add_widget;
	wg->data = data;
	g_hash_table_insert(fnWidgetGenTable, srcNode, wg);

	if (nodeGrid != NULL)
		gtk_container_add(GTK_CONTAINER(nodeGrid), srcNode);
}

void flowNetwork_drag_node_implementaion(GtkWidget *n)
{
	struct fnWidgetGen *wg;

	gtk_drag_source_set(n,	/* widget will be drag-able */
			    GDK_BUTTON1_MASK,	/* modifier that will start a drag */
			    target_list,	/* lists of target to support */
			    n_targets,	/* size of list */
			    GDK_ACTION_COPY);	/* what to do with data after dropped */

	g_signal_connect(n, "drag-data-get",
			 G_CALLBACK(network_data_get_handle), NULL);
	g_signal_connect(n, "drag-begin", G_CALLBACK(node_drag_begin_handle), NULL);
	g_signal_connect(n, "drag-end", G_CALLBACK(node_drag_end_handle), NULL);

	wg = malloc(sizeof(struct fnWidgetGen));
	wg->add_widget = return_widget;
	wg->data = n;

	if (fnWidgetGenTable == NULL)
		fnWidgetGenTable = g_hash_table_new(NULL, NULL);

	g_hash_table_insert(fnWidgetGenTable, n, wg);
}

static void flowNetwork_drop_init(FlowNetwork *fn)
{
	gtk_drag_dest_set(GTK_WIDGET(fn),	/* widget that will accept a drop */
			  GTK_DEST_DEFAULT_MOTION	/* default actions for dest on DnD */
			  | GTK_DEST_DEFAULT_HIGHLIGHT, target_list,	/* lists of target to support */
			  n_targets,	/* size of list */
			  GDK_ACTION_COPY);	/* what to do with data after dropped */
	g_signal_connect(GTK_WIDGET(fn), "drag-drop",
			 G_CALLBACK(network_drop_handle), NULL);
	g_signal_connect(GTK_WIDGET(fn), "drag-data-received",
			 G_CALLBACK(network_data_received_handle), NULL);
}

static void
node_drag_begin_handle(GtkWidget *widget,
		       GdkDragContext *context,
		       gpointer user_data)
{
	const gchar *name;
	GtkRequisition *natural_size;
	GdkWindow *window;
	GdkPixbuf *pixbuf;
	cairo_surface_t *surface_window;
	cairo_surface_t *surface_alpha;
	cairo_t *cr;
	GtkWidget *parent;

	name = gtk_widget_get_name(widget);
	g_print("%s: node_drag_begin_handle\n", name);
	natural_size = gtk_requisition_new();
	gtk_widget_get_preferred_size(widget, NULL, natural_size);
	window = gtk_widget_get_window(widget);
	/* gdk_window_set_opacity(window, 0.3); */
	pixbuf = gdk_pixbuf_get_from_window(window, 0, 0,
					    natural_size->width,
					    natural_size->height);

	/* pixbuf = gdk_pixbuf_new_from_data(gdk_pixbuf_get_pixels(pixbuf), */
	/* 				  gdk_pixbuf_get_colorspace(pixbuf), */
	/* 				  TRUE, */
	/* 				  gdk_pixbuf_get_bits_per_sample(pixbuf), */
	/* 				  gdk_pixbuf_get_width(pixbuf), */
	/* 				  gdk_pixbuf_get_height(pixbuf), */
	/* 				  gdk_pixbuf_get_rowstride(pixbuf), */
	/* 				  NULL, NULL); */
	/* GValue t = G_VALUE_INIT; */
	/* g_value_init(&t, G_TYPE_BOOLEAN); */
	/* g_value_set_boolean(&t, TRUE); */
	/* g_object_set_property(G_OBJECT(pixbuf), */
	/* 		      "has-alpha", */
	/* 		      &t); */
	/* gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0); */
        surface_window = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);
        surface_alpha = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						   natural_size->width,
						   natural_size->height);
	cr = cairo_create(surface_alpha);
	cairo_set_source_surface(cr, surface_window, 0, 0);
	/* cairo_rectangle(cr, */
	/* 		0, */
	/* 		0, */
	/* 		natural_size->width, */
	/* 		natural_size->height); */
	cairo_arc(cr, natural_size->width/2, natural_size->height/2, 64, 0, 2 * G_PI);
	cairo_fill(cr);
	/* cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); */
	/* cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.5); */
	/* /\* cairo_set_line_width(cr, 10); *\/ */
	/* cairo_rectangle(cr, 12, 12, 50, */
	/* 		80); */
	//cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	/* cairo_rectangle(cr, 0, 0, 50, 50); */
	//cairo_stroke(cr);
	/* g_print("%f\n", cairo_get_tolerance (cr)); */
	/* cairo_surface_write_to_png(surface, "test.png"); */
 		/* gdk_window_create_similar_image_surface(window, */
		/* 					CAIRO_FORMAT_ARGB32, */
		/* 					natural_size->width, */
		/* 					natural_size->height, */
		/* 					0); */
	/* cairo_t *cr = gdk_cairo_create(window); */
	/* /\* gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0); *\/ */
	/* cairo_set_source_rgb(cr, 0.42, 0.65, 0.80); */
	/* cairo_set_line_width(cr, 6); */
	/* cairo_rectangle(cr, 0, 0, natural_size->width, */
	/* 		natural_size->height); */
	/* cairo_stroke(cr); */

	pixbuf = gdk_pixbuf_get_from_surface(surface_alpha, 0, 0,
					     natural_size->width,
					     natural_size->height);
	/* pixbuf = gdk_pixbuf_new_from_file("../../cairo/clearexample.png", NULL); */
	g_print("%i\n", gdk_pixbuf_get_has_alpha(pixbuf));
	gtk_drag_set_icon_pixbuf(context, pixbuf, natural_size->width / 2,
				 natural_size->height / 2);
	/* gtk_drag_set_icon_surface(context, get_background(widget)); */
	gtk_widget_hide(widget);
	gtk_requisition_free(natural_size);
	parent = gtk_widget_get_parent(widget);
	if (GTK_IS_LAYOUT(parent))
		redraw_layout(GTK_LAYOUT(parent));
}





static void network_data_get_handle(GtkWidget *widget,
				    GdkDragContext *context,
				    GtkSelectionData *selection_data,
				    guint target_type, guint time,
				    gpointer user_data)
{
	struct fnWidgetGen *node;

	node = g_hash_table_lookup(fnWidgetGenTable, widget);
	gtk_selection_data_set(selection_data,	/* Allocated GdkSelectionData object */
			       gtk_selection_data_get_target(selection_data),	/* target type */
			       _POINTER,	/* number of bits per 'unit' */
			       (guchar *) & node,	/* pointer to data to be sent */
			       sizeof(node));	/* length of data in units */
	gtk_widget_show(widget);
}

static gboolean network_drop_handle(GtkWidget *widget,
				    GdkDragContext *context,
				    gint x, gint y, guint time,
				    gpointer user_data)
{
	GdkAtom target_type;

	if (IS_FLOWNETWORK(widget)) {
		target_type = GDK_POINTER_TO_ATOM
		    (g_list_nth_data
		     (gdk_drag_context_list_targets(context), TARGET_POINTER));
		gtk_drag_get_data(widget,	/* will receive 'drag-data-received' signal */
				  context,	/* represents the current state of the DnD */
				  target_type,	/* the target type we want */
				  time);	/* time stamp */
		return TRUE;
	}
	return FALSE;
}


static void network_data_received_handle(GtkWidget *widget,
					 GdkDragContext *context,
					 gint x, gint y,
					 GtkSelectionData *selection_data,
					 guint target_type, guint time, gpointer data)
{
	struct fnWidgetGen *wg;
	GtkWidget *w;
	gint widget_width, widget_height;
	GtkRequisition *natural_size;
	gint new_x;
	gint new_y;

        wg = *(struct fnWidgetGen **)gtk_selection_data_get_data(selection_data);
	w = wg->add_widget(FLOWNETWORK(widget), wg->data);
	gtk_widget_get_size_request(w, &widget_width, &widget_height);
        natural_size = gtk_requisition_new();
	gtk_widget_get_preferred_size(w, NULL, natural_size);
	if (widget_width == -1)
		widget_width = natural_size->width;
	if (widget_height == -1)
		widget_height = natural_size->height;

	g_print("x: %i, y: %i\n", widget_width, widget_height);
	new_x = scroller_offset_x_all(widget) + x - widget_width / 2;
	new_y = scroller_offset_y_all(widget) + y - widget_height / 2;

	if (container_has_child(GTK_CONTAINER(FLOWNETWORK(widget)->layout), w)) {
		gtk_layout_move(GTK_LAYOUT(FLOWNETWORK(widget)->layout), w,
				new_x, new_y);
	} else {
		gtk_layout_put(GTK_LAYOUT(FLOWNETWORK(widget)->layout), w,
			       new_x, new_y);
	}

	resize_flowNetwork(GTK_LAYOUT(FLOWNETWORK(widget)->layout), w,
			   widget_width, widget_height);
	gtk_widget_get_preferred_size(FLOWNETWORK(widget)->layout, NULL,
				      natural_size);
	gtk_requisition_free(natural_size);
	/* gtk_widget_queue_draw_area(FLOWNETWORK(widget)->layout, */
	/*                           0, 0, natural_size->width, natural_size->height); */
	gtk_widget_show_all(widget);
	gtk_drag_finish(context, TRUE, TRUE, time);
}

static void node_drag_end_handle(GtkWidget *widget,
				 GdkDragContext *context,
				 gpointer user_data) {

	const gchar *name;

	name = gtk_widget_get_name(widget);
	g_print("%s: node_drag_end_handle\n", name);
	gtk_widget_show(widget);
}


/* Section for drawing */
static void draw_connections(GtkWidget *widget, cairo_t *cr)
{
	GtkWidget *fn;
	GtkRequisition *natural_size;
	GList *children;
	GValue gx = G_VALUE_INIT;
	GValue gy = G_VALUE_INIT;
	gboolean prev_visible;


	fn = gtk_widget_get_parent(widget);
	natural_size = gtk_requisition_new();
	children = gtk_container_get_children(GTK_CONTAINER(widget));

	g_value_init(&gx, G_TYPE_INT);
	g_value_init(&gy, G_TYPE_INT);

	prev_visible = TRUE;
	while (children != NULL) {
		if (gtk_widget_is_visible(GTK_WIDGET(children->data))) {
			gint x;
			gint y;

			gtk_widget_get_preferred_size(GTK_WIDGET
						      (children->data), NULL,
						      natural_size);

			gtk_container_child_get_property(GTK_CONTAINER(widget),
							 GTK_WIDGET
							 (children->data), "x",
							 &gx);
			gtk_container_child_get_property(GTK_CONTAINER(widget),
							 GTK_WIDGET
							 (children->data), "y",
							 &gy);
			x = g_value_get_int(&gx) + natural_size->width / 2 -
				scroller_offset_x_all(fn);
			y = g_value_get_int(&gy) + natural_size->height / 2 -
				scroller_offset_y_all(fn);
			if (prev_visible) {
				cairo_line_to(cr, x, y);
				cairo_stroke(cr);
			}
			cairo_move_to(cr, x, y);
			prev_visible = TRUE;
		} else {
			prev_visible = FALSE;
		}
		children = children->next;
	}
	gtk_requisition_free(natural_size);
	g_value_unset(&gx);
	g_value_unset(&gy);
}

static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	/* Set color for background */
	/* cairo_set_source_rgb(cr, 1, 1, 1); */
	/* fill in the background color */
	/* cairo_paint(cr); */

	/* cairo_set_source_rgba(cr, 0, 0, 0, 0); */
	/* cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); */
	/* cairo_paint(cr); */

	/* set color for rectangle */
	cairo_set_source_rgb(cr, 0.42, 0.65, 0.80);
	/* set the line width */
	cairo_set_line_width(cr, 6);
	/* draw the rectangle's path beginning at 3,3 */
	cairo_rectangle(cr, 3, 3, 100, 100);
	/* stroke the rectangle's path with the chosen color so it's actually visible */
	cairo_stroke(cr);

	/* draw circle */
	cairo_set_source_rgb(cr, 0.17, 0.63, 0.12);
	cairo_set_line_width(cr, 2);
	cairo_arc(cr, 150, 210, 20, 0, 2 * G_PI);
	cairo_stroke(cr);

	/* draw horizontal line */
	cairo_set_source_rgb(cr, 0.77, 0.16, 0.13);
	cairo_set_line_width(cr, 6);
	cairo_move_to(cr, 80, 160);
	cairo_line_to(cr, 200, 160);
	cairo_stroke(cr);

	draw_connections(widget, cr);

	return FALSE;
}

/* Utility */
static GtkWidget *return_widget(FlowNetwork *fn, gpointer data)
{
	return GTK_WIDGET(data);
}

static void scroller_offset_x(gpointer scroller, void *acuval)
{
	*(gint *) acuval += (gint)
	    round(gtk_adjustment_get_value
		  (gtk_scrolled_window_get_hadjustment
		   (GTK_SCROLLED_WINDOW(scroller))));
}

static void scroller_offset_y(gpointer scroller, void *acuval)
{
	*(gint *) acuval += (gint)
	    round(gtk_adjustment_get_value
		  (gtk_scrolled_window_get_vadjustment
		   (GTK_SCROLLED_WINDOW(scroller))));
}

static gint scroller_offset_x_all(GtkWidget *fn)
{
	gint acuval;

	acuval = 0;
	g_slist_foreach(FLOWNETWORK(fn)->scrollers, scroller_offset_x, &acuval);
	return acuval;
}

static gint scroller_offset_y_all(GtkWidget *fn)
{
	int acuval;

	acuval = 0;
	g_slist_foreach(FLOWNETWORK(fn)->scrollers, scroller_offset_y, &acuval);
	return acuval;
}

static gboolean container_has_child(GtkContainer *container,
				    GtkWidget *widget)
{
	GList *children;
	children = gtk_container_get_children(GTK_CONTAINER(container));
	if (g_list_find(children, widget) != NULL)
		return TRUE;
	return FALSE;
}

static void resize_flowNetwork(GtkLayout *layout, GtkWidget *widget,
			       gint widget_width, gint widget_height)
{
	GtkAdjustment *adjust_width;
	GtkAdjustment *adjust_height;
	gint swWidth;
	gint swHeight;
	gint widgetEdge_x;
	gint widgetEdge_y;
	gint new_width;
	gint new_height;
	GValue gx = G_VALUE_INIT;
	GValue gy = G_VALUE_INIT;

	adjust_width = gtk_scrollable_get_hadjustment
		(GTK_SCROLLABLE(layout));
	adjust_height = gtk_scrollable_get_vadjustment
		(GTK_SCROLLABLE(layout));

	swWidth = gtk_adjustment_get_upper(adjust_width);
        swHeight = gtk_adjustment_get_upper(adjust_height);

	g_value_init(&gx, G_TYPE_INT);
	g_value_init(&gy, G_TYPE_INT);

	gtk_container_child_get_property(GTK_CONTAINER(layout), widget, "x", &gx);
	gtk_container_child_get_property(GTK_CONTAINER(layout), widget, "y", &gy);

	widgetEdge_x = g_value_get_int(&gx) + 2 * widget_width;
	widgetEdge_y = g_value_get_int(&gy) + 2 * widget_height;

	new_width = (widgetEdge_x > swWidth) ? widgetEdge_x : swWidth;
	new_height = (widgetEdge_y > swHeight) ? widgetEdge_y : swHeight;

	gtk_layout_set_size(layout, new_width, new_height);

	g_value_unset(&gx);
	g_value_unset(&gy);

}

static void redraw_layout(GtkLayout *layout)
{
	GtkRequisition *natural_size;

	natural_size = gtk_requisition_new();
	gtk_widget_get_preferred_size(GTK_WIDGET(layout), NULL, natural_size);
	gtk_widget_queue_draw_area(GTK_WIDGET(layout), 0, 0,
				   natural_size->width, natural_size->height);
	gtk_requisition_free(natural_size);
}
