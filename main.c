#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

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

uint32_t
get_window_state(xcb_window_t w)
{
    uint32_t result = 0;
    xcb_get_property_cookie_t c;
    xcb_get_property_reply_t *r;
    
    xcb_atom_t atom = get_atom("WM_STATE");
    
    c = xcb_get_property(conn, 0, w, atom, atom, 0L, 2L);
    r = xcb_get_property_reply(conn, c, NULL);

    if (r) {
	if (r->format == 32 && r->length == 2) {
	    result = *((uint32_t *)xcb_get_property_value(r));
	}
    }
	
    free(r);

    return result;
}

int
get_windows(xcb_connection_t *con, xcb_window_t w, xcb_window_t **l)
{
    uint32_t childnum = 0;
    xcb_query_tree_cookie_t c;
    xcb_query_tree_reply_t *r;

    c = xcb_query_tree(con, w);
    r = xcb_query_tree_reply(con, c, NULL);
    if (r == NULL) {
	errx(1, "0x%08x: no such window", w);
    }
    
    childnum = r->children_len;

    *l = malloc(sizeof(xcb_window_t) * childnum);
    memcpy(*l, xcb_query_tree_children(r),
	   sizeof(xcb_window_t) * childnum);

    free(r);

    return childnum;
}

int
main(int argc, char **argv)
{
    int i, wn;
    uint32_t state;
    xcb_window_t *wc;

    init_xcb(&conn);
    get_screen(conn, &scrn);

    wn = get_windows(conn, scrn->root, &wc);

    if (wc == NULL) {
	errx(1, "0x%80x: unable to retrieve children", scrn->root);
    }

    for (i = 0; i < wn; i++) {
	state = get_window_state(wc[i]);
	if (state > 0) {
	    printf("%s\n", get_window_name(wc[i]));
	}
    }

    free(wc);

}
