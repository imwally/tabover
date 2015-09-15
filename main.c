#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
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
    xcb_atom_t atom = 0;
        
    c = xcb_intern_atom(conn, 0, strlen(name), name);
    r = xcb_intern_atom_reply(conn, c, NULL);

    if (r) {
      atom = r->atom;
    }

    free(r);
    return atom;
}

char *
window_class(xcb_window_t w)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    char *class = 0;

    c = xcb_get_property(conn, 0, w,
			 XCB_ATOM_WM_CLASS,
			 XCB_ATOM_STRING,
			 0L, 32L);
    r = xcb_get_property_reply(conn, c, NULL);

    if (r) {
	class = (char *) xcb_get_property_value(r);
    }

    free(r);
    return class;
}

char *
window_name(xcb_window_t w)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    char *name = 0;

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
desktop_of_window(xcb_window_t w)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    xcb_atom_t atom = 0;
    int desktop = 0;

    atom = get_atom("_NET_WM_DESKTOP");
    
    c = xcb_get_property(conn, 0, w,
			 atom,
			 XCB_ATOM_CARDINAL,
			 0, 1);
    r = xcb_get_property_reply(conn, c, NULL);

    if (r) {
      desktop = *((int *) xcb_get_property_value(r));
    }

    free(r);
    return desktop;
}

int
client_list(xcb_window_t w, xcb_window_t **windows)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    xcb_atom_t atom = 0;
    int wn = 0;

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
unbuf_stdin() 
{
    struct termios t;
    
    if (-1 == tcgetattr(0, &t)) {
	perror("tcgetattr");
    }

    t.c_lflag &= ~(ICANON | ECHO);
    t.c_lflag |= ISIG;
    t.c_iflag &= ~ICRNL;

    t.c_cc[VMIN] = 1; 
    t.c_cc[VTIME] = 0;

    if (-1 == tcsetattr(0, TCSAFLUSH, &t)) {
	return -1;
    }
    
}

void
cycle_selection(int wn, int selection, xcb_window_t *windows)
{
    char *wname, *wclass;
    char *select = "";
    int i = 0;

    for (i = 0; i < wn; i++) {
	if (selection == i) {
	    select = "\t";
	}
	wclass = window_class(windows[i]);
	wname = window_name(windows[i]);
	printf("%s[%s] %s\n", select, wclass, wname);
	select = "";
    }

}

int
main(int argc, char **argv)
{
    xcb_window_t *windows;
    char ch = 0;
    int wn, i = 0;
    
    // Setup connection to X and grab screen
    init_xcb(&conn);
    get_screen(conn, &scrn);

    // Get a list of windows via _NET_CLIENT_LIST of the root screen
    wn = client_list(scrn->root, &windows);

    // Unbuffer terminal input to watch for key presses
    if (unbuf_stdin()) {
	perror("can't unbuffer terminal");
    }

    // Cycle window selection when TAB is pressed
    while (1) {
	ch = fgetc(stdin);
	if (ch == '\t') {
	    if (i == wn) {
		i = 0;
	    } 
	    system("clear");
	    cycle_selection(wn, ++i, windows);
	}
	if (ch == '`') {
	    system("clear");
	    cycle_selection(wn, --i, windows);
	    if (i < 0) {
		i = wn;
	    }
	}
    }

    free(windows);
    xcb_disconnect(conn);
}
