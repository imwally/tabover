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

char *
get_window_name(xcb_window_t w)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *r;
    char *name;

    cookie = xcb_get_property(conn, 0, w,
			      XCB_ATOM_WM_NAME,
			      XCB_ATOM_STRING,
			      0L, 32L);
    r = xcb_get_property_reply(conn, cookie, NULL);

    if (r) {
	name = (char *) xcb_get_property_value(r);
    }

    free(r);

    return name;
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

int
get_client_list(xcb_window_t w, xcb_window_t **list)
{
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    xcb_atom_t atom;
    int wn;

    atom = get_atom("_NET_CLIENT_LIST");
    
    c = xcb_get_property(conn, 0, w, atom, XCB_ATOM_WINDOW, 0L, 32L);
    r = xcb_get_property_reply(conn, c, NULL);

    if (r) {
	*list = malloc(sizeof(xcb_window_t) * r->length);
	memcpy(*list, xcb_get_property_value(r), sizeof(xcb_window_t) * r->length);
	wn = r->length;
    }
    
    free(r);

    return wn;
}

int
main(int argc, char **argv)
{
    int wn, i;
    xcb_window_t *list;
    
    // Setup connection to X and grab screen
    init_xcb(&conn);
    get_screen(conn, &scrn);

    // Get a list of windows via _NET_CLIENT_LIST of the root screen
    wn = get_client_list(scrn->root, &list);

    // Iterate of number of windows and print the name
    for (i = 0; i < wn; i++) {
	printf("%s\n", get_window_name(list[i]));
    }

    free(list);

}
