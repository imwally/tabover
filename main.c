#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>

static xcb_connection_t *conn;
static xcb_screen_t *scrn;

void
init_xcb(xcb_connection_t **con)
{
    *con = xcb_connect(NULL, NULL);
    
    if (xcb_connection_has_error(*con)) {
	errx(1, "unable to connect to the X server");
    }
}

void
get_screen(xcb_connection_t *con, xcb_screen_t **scr)
{
    *scr = xcb_setup_roots_iterator(xcb_get_setup(con)).data;
    
    if (*scr == NULL) {
	errx(1, "unable to retrieve screen information");
    }
}

xcb_atom_t
get_atom(char *name)
{
    xcb_intern_atom_cookie_t c;
    xcb_intern_atom_reply_t *r;
    xcb_atom_t atom;
        
    c = xcb_intern_atom(conn, 0, strlen(name), name);
    r = xcb_intern_atom_reply(conn, c, NULL);
    atom = r->atom;

    free(r);
    return atom;
}

char *
get_window_name(xcb_window_t w)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    char *name;

    c = xcb_get_property(conn, 0, w,
			 XCB_ATOM_WM_NAME,
			 XCB_ATOM_STRING,
			 0L, 32L);
    r = xcb_get_property_reply(conn, c, NULL);

    if (r) {
	name = (char *) xcb_get_property_value(r);
    }

    free(r);
    return name;
}

uint32_t
get_desktop_of_window(xcb_window_t w)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    xcb_atom_t atom;
    int desktop;

    atom = get_atom("_NET_WM_DESKTOP");
    
    c = xcb_get_property(conn, 0, w,
			 atom,
			 XCB_ATOM_CARDINAL,
			 0, 1);
    r = xcb_get_property_reply(conn, c, NULL);
    desktop = *((int *) xcb_get_property_value(r));

    free(r);
    return desktop;
}

int
get_client_list(xcb_window_t w, xcb_window_t **windows)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    xcb_atom_t atom;
    int wn;

    atom = get_atom("_NET_CLIENT_LIST");
    
    c = xcb_get_property(conn, 0, w,
			 atom,
			 XCB_ATOM_WINDOW,
			 0L, 32L);
    r = xcb_get_property_reply(conn, c, NULL);

    if (r) {
	*windows = malloc(sizeof(xcb_window_t) * r->length);
	memcpy(*windows, xcb_get_property_value(r), sizeof(xcb_window_t) * r->length);
	wn = r->length;
    }
    
    free(r);
    return wn;
}

void
send_client_message(xcb_connection_t *con, uint32_t mask,
		    xcb_window_t destination, xcb_window_t window,
		    xcb_atom_t message, const uint32_t data[])
{
    xcb_client_message_event_t event;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.sequence = 0;
    event.window = window;
    event.type = message;

    int i;
    for (i = 0; i < 5; i++) {
	event.data.data32[i] = data[i];
    }

    xcb_send_event(con, 0, destination, mask, (const char *) &event);

}

void
switch_to_desktop(int desktop)
{
    uint32_t data[5] = {
	(uint32_t)desktop, 0, 0, 0, 0
    };
    
    uint32_t mask = (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
		     XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
    
    send_client_message(conn, mask, scrn->root, scrn->root,
			get_atom("_NET_CURRENT_DESKTOP"), data);

    xcb_flush(conn);
}

void
focus_window(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t window)
{
    uint32_t data[5] = {
	2L, 0, 0, 0, 0
    };
    
    uint32_t mask = (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
		     XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
    
    send_client_message(conn, mask, screen->root, window,
			get_atom("_NET_ACTIVE_WINDOW"), data);

       
    xcb_flush(conn);
}

int
main(int argc, char **argv)
{
    xcb_window_t *windows;
    int desktop, wn, i;
    char *wname;
    
    // Setup connection to X and grab screen
    init_xcb(&conn);
    get_screen(conn, &scrn);

    // Get a list of windows via _NET_CLIENT_LIST of the root screen
    wn = get_client_list(scrn->root, &windows);

    // Iterate over number of windows and print the name
    for (i = 0; i < wn; i++) {
	desktop = get_desktop_of_window(windows[i]) + 1;
	wname = get_window_name(windows[i]);
	printf("%d: ", desktop);
	printf("%s\n", wname);
    }
    
    free(windows);
    xcb_disconnect(conn);
}
