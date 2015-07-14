#ifndef FLOWNETWORK_H
#define FLOWNETWORK_H

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define FLOWNETWORK_TYPE            (flowNetwork_get_type ())
#define FLOWNETWORK(obj)            (G_TYPE_CHECK_INSTANCE_CAST \
				     ((obj), FLOWNETWORK_TYPE, FlowNetwork))
#define FLOWNETWORK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST \
				     ((klass), FLOWNETWORK_TYPE, FlowNetworkClass))
#define IS_FLOWNETWORK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOWNETWORK_TYPE))
#define IS_FLOWNETWORK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOWNETWORK_TYPE))
typedef struct _FlowNetwork FlowNetwork;
typedef struct _FlowNetworkClass FlowNetworkClass;

struct _FlowNetwork {
	GtkScrolledWindow scrolledWindow;
	GtkWidget *grid;
	GtkWidget *layout;
	GSList *scrollers;
	GHashTable *data;
};

struct _FlowNetworkClass {
	GtkScrolledWindowClass parent_class;

	void (*flowNetwork) (FlowNetwork *fn);
};

GType flowNetwork_get_type(void);
GtkWidget *flowNetwork_new(void);
void flowNetwork_drag_node_source(GtkWidget *srcNode,
				  GtkWidget *(*add_widget) (FlowNetwork *fn,
							     gpointer data),
				  gpointer data, GtkGrid *nodeGrid);
void flowNetwork_drag_node_implementaion(GtkWidget *n);
void flowNetwork_account_scroller(FlowNetwork *fn, GtkScrolledWindow *gsw);

G_END_DECLS
#endif
