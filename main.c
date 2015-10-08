#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>

#define NEXT 1
#define PREV -1

static xcb_connection_t *conn;
static xcb_screen_t *scrn;
static xcb_window_t *ws;
static int wsel = 0;
static struct termios torig;

const uint32_t mask = (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
		       XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
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

    return 0;
}

int
buf_stdin()
{
    if (-1 == tcsetattr(0, TCSAFLUSH, &torig)) {
	return -1;
    }

    return 0;
}

int
init_xcb(xcb_connection_t **con)
{
    *con = xcb_connect(NULL, NULL);
    
    if (xcb_connection_has_error(*con)) {
	perror("unable to connect to the X server");
	return -1;
    }

    return 0;
}

int
get_screen(xcb_connection_t *con, xcb_screen_t **scr)
{
    *scr = xcb_setup_roots_iterator(xcb_get_setup(con)).data;
    
    if (*scr == NULL) {
	perror("unable to retrieve screen information");
	return -1;
    }

    return 0;
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
get_prop_string(xcb_atom_t atom, xcb_window_t w)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    char *string = 0;

    c = xcb_get_property(conn, 0, w, atom, XCB_ATOM_STRING, 0, 32);
    r = xcb_get_property_reply(conn, c, NULL);

    if (r) {
	string = (char *) xcb_get_property_value(r);
    }

    free(r);
    return string;
}

int
desktop_of_window(xcb_window_t w)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    int desktop = 0;

    xcb_atom_t atom = get_atom("_NET_WM_DESKTOP");
    
    c = xcb_get_property(conn, 0, w, atom, XCB_ATOM_CARDINAL, 0, 1);
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
    int wn = 0;

    xcb_atom_t atom = get_atom("_NET_CLIENT_LIST");
    
    c = xcb_get_property(conn, 0, w, atom, XCB_ATOM_WINDOW, 0L, 32L);
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
send_client_message(xcb_window_t destination, xcb_window_t window,
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

    xcb_send_event(conn, 0, destination, mask, (const char *) &event);

}

void
switch_to_desktop(int desktop)
{
    xcb_atom_t atom = get_atom("_NET_CURRENT_DESKTOP");
    uint32_t data[5] = {desktop, 0, 0, 0, 0};
    
    send_client_message(scrn->root, scrn->root, atom, data);

    xcb_flush(conn);
}

void
focus_window(xcb_window_t window)
{
    xcb_atom_t atom = get_atom("_NET_ACTIVE_WINDOW");
    uint32_t data[5] = {2, 0, 0, 0, 0};

    send_client_message(scrn->root, window, atom, data);

    xcb_flush(conn);
}

void
select_window(xcb_window_t window)
{
    switch_to_desktop(desktop_of_window(window));
    focus_window(window);
}

void
print_selection(int wn, xcb_window_t *windows, int wsel)
{
    char *wname, *wclass = 0;
    int wdesktop, i = 0;
    
    for (i = 0; i < wn; i++) {
	if (wsel == i) {
	    printf(">");
	}
	wdesktop = desktop_of_window(windows[i])+1;
	wclass = get_prop_string(XCB_ATOM_WM_CLASS, windows[i]);
	wname  = get_prop_string(XCB_ATOM_WM_NAME, windows[i]);
	printf(" %d: [%s] %s\n", wdesktop, wclass, wname);
    }
}
  
void
cycle_selection(int direction, int wn, xcb_window_t *windows, int select)
{
    system("clear");

    wsel += direction;

    if (select) {
	select_window(windows[wsel]);
    }

    if (wsel >= wn) {
	wsel = 0;
    }
    
    if (wsel < 0) {
	wsel = wn-1;
    }

    print_selection(wn, windows, wsel);
}

void
cleanup()
{
    // Return orginal terminal settings
    if (buf_stdin()) {
	perror("buffer input");
    }
    
    free(ws);
    xcb_disconnect(conn);
}

int
main(int argc, char **argv)
{
    char ch = 0;
    int wn = 0;
    
    // Setup connection to X and grab screen
    init_xcb(&conn);
    get_screen(conn, &scrn);

    // Get a list of windows via _NET_CLIENT_LIST of the root screen
    wn = client_list(scrn->root, &ws);

    // Save original stdin settings into torig
    if (-1 == tcgetattr(0, &torig)) {
	perror("tcgetattr");
	return -1;
    }
    
    // Unbuffer terminal input to watch for key presses
    if (unbuf_stdin()) {
	perror("can't unbuffer terminal");
	return -1;
    }

    // Invocation: start window selection at zero, or the first window
    cycle_selection(0, wn, ws, 0);
    
    // Cycle window selection when TAB or ` is pressed and activate
    // window on return
    while (1) {
	ch = fgetc(stdin);
	switch (ch) {
	case '\t':
	    cycle_selection(NEXT, wn, ws, 0);
	    break;
	case 'j':
	    cycle_selection(NEXT, wn, ws, 0);
	    break;
	case '`':
	    cycle_selection(PREV, wn, ws, 0);
	    break;
	case 'k':
	    cycle_selection(PREV, wn, ws, 0);
	    break;
	case 'q':
	    cleanup();
	    return 0;
	case '\r':
	    cycle_selection(0, wn, ws, 1);
	    cleanup();
	    return 0;
	}
    }
}
